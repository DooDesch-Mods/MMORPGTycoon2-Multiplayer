"""Fixed read-only Subscriber lifecycle witness for a bound paused test process."""
import ctypes
from ctypes import wintypes
import hashlib
from pathlib import Path
import struct
from runtime import RuntimeErrorDetail, INSTALLED_EXE_SHA256, windows_modules, framework_module_paths


def require(value, cause, code='lifecycle_read_guard'):
    if not value:
        raise RuntimeErrorDetail(code, cause)


class ProcessRead:
    def __init__(self, manifest):
        self.kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        k = self.kernel
        k.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        k.OpenProcess.restype = wintypes.HANDLE
        k.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        k.ReadProcessMemory.restype = wintypes.BOOL
        k.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
        k.GetProcessTimes.restype = wintypes.BOOL
        k.CloseHandle.argtypes = [wintypes.HANDLE]
        k.CloseHandle.restype = wintypes.BOOL
        self.handle = k.OpenProcess(0x1000 | 0x0010, False, manifest['pid'])
        require(self.handle, f"OpenProcess query/read pid={manifest['pid']} error={ctypes.get_last_error()}", 'process_read_failed')
        try:
            times = [wintypes.FILETIME() for _ in range(4)]
            require(k.GetProcessTimes(self.handle, *(ctypes.byref(v) for v in times)), f'GetProcessTimes error={ctypes.get_last_error()}', 'process_read_failed')
            token = str((times[0].dwHighDateTime << 32) | times[0].dwLowDateTime)
            require(token == manifest['process_start_time'], 'Opened read handle start token differs', 'pid_reused')
        except BaseException as original:
            try:
                self.close()
            except BaseException as cleanup:
                raise RuntimeErrorDetail('process_read_failed', f'{original}; read handle cleanup also failed: {cleanup}') from original
            raise

    def close(self):
        if self.handle:
            handle, self.handle = self.handle, None
            require(self.kernel.CloseHandle(handle), f'CloseHandle error={ctypes.get_last_error()}', 'process_read_failed')

    def __enter__(self):
        return self

    def __exit__(self, _type, original, _traceback):
        try:
            self.close()
        except BaseException as cleanup:
            if original is not None:
                raise RuntimeErrorDetail('process_read_failed', f'{original}; read handle cleanup also failed: {cleanup}') from original
            raise

    def __call__(self, address, size):
        require(type(address) is int and 0 < address <= 0x7fffffffffff and 0 < size <= 8 * 1000000 and address + size <= 0x800000000000, 'Fixed lifecycle read extent invalid')
        data = ctypes.create_string_buffer(size)
        copied = ctypes.c_size_t()
        okay = self.kernel.ReadProcessMemory(self.handle, address, data, size, ctypes.byref(copied))
        require(okay and copied.value == size, f'ReadProcessMemory address={address} bytes={size} copied={copied.value} error={ctypes.get_last_error()}', 'process_read_failed')
        return data.raw


def campaign_boost_sample(get, ap, image, mode, state):
    """Copy current owners and local bindings. Never evaluate Campaign::IsActive."""
    def array(owner, offset, label, limit=512):
        data, count, capacity = get(owner, offset), get(owner, offset + 8, 'i'), get(owner, offset + 12, 'i')
        require(0 <= count <= capacity <= limit and (not count or data), f'{label} array bounds differ')
        members = [get(data, i * 8) for i in range(count)]
        require(all(members) and len(set(members)) == count, f'{label} member missing/duplicate')
        return {'data': data, 'count': count, 'capacity': capacity, 'members': members}

    marketing = get(state, 0x1d0)
    require(marketing and marketing == get(image, 0x13df2c0), 'Marketing current GameState singleton differs')
    ap(marketing, 0x16dfa60, 'Marketing')
    campaigns = array(marketing, 0x70, 'Campaign')
    awards = array(marketing, 0x40, 'Award')
    rows = []
    for campaign in campaigns['members']:
        ap(campaign, 0x16e3d50, 'Campaign')
        rows.append({'address': campaign, 'type': get(campaign, 0x50, 'i'),
                     'start_day': get(campaign, 0x54, 'i'), 'duration_days': get(campaign, 0x58, 'i')})
    hud = get(mode, 0x128)
    require(hud and hud == get(image, 0x13df8b0), 'Boost current Mode HUD singleton differs')
    ap(hud, 0x1742750, 'HUD')
    window = get(hud, 0xb8)
    out = {'marketing': marketing, 'campaigns': {**campaigns, 'rows': rows}, 'awards': awards,
           'hud': hud, 'window': window, 'window_present': bool(window)}
    if not window:
        return out
    ap(window, 0x16fa4d0, 'BoostWindow')
    ap(window + 0x508, 0x16e1770, 'BoostView store')
    views = array(window, 0x510, 'BoostView')
    panes = array(window, 0x300, 'BoostWindow pane')
    bindings = []
    for view in views['members']:
        ap(view, 0x16dd8a0, 'BoostView')
        campaign, award = get(view, 0x328), get(view, 0x320)
        # A stale binding is evidence. Only current owner members were dereferenced above.
        bindings.append({'address': view, 'campaign': campaign, 'award': award,
                         'campaign_member': bool(campaign and campaign in campaigns['members']),
                         'award_member': bool(award and award in awards['members']),
                         'pane_member': view in panes['members'],
                         'parent': get(view, 0x278), 'window': get(view, 0x280)})
    out.update({'views': {**views, 'bindings': bindings}, 'panes': panes,
                'width': get(window, 0x138, 'f'), 'height': get(window, 0x13c, 'f')})
    return out


def sample(read, image, serialized_id, include_scenario_vips=False, include_campaign_boosts=False):
    def get(p, off=0, fmt='Q'):
        require(p != 0, f'Null receiver at fixed field offset={off}')
        return struct.unpack('<' + fmt, read(p + off, struct.calcsize('<' + fmt)))[0]
    def ap(p, rva, label):
        require(get(p) == image + rva, f'{label} addresspoint differs required_rva={rva}')
    mode, game = get(image, 0x13df320), get(image, 0x13df8f0)
    ap(game, 0x1743740, 'Game'); ap(mode, 0x16e6730, 'Mode')
    require(get(game, 0x28) == mode and get(mode, 0x2c4, 'i') == 4, 'Active Mode owner/state differs')
    state = get(mode, 0x58); ap(state, 0x16de920, 'GameState')
    require(get(image, 0x13df2a0) == state, 'GameState singleton differs')
    world = get(state, 0x160); ap(world, 0x174aef0, 'World')
    map_ = get(world, 0x158); ap(map_, 0x1742860, 'Map')
    require(get(image, 0x13df9c0) == world and get(image, 0x13df8c0) == map_, 'World/Map singleton differs')
    root = map_ + 0x1c0; ap(root, 0x174ad20, 'Root Shard')
    require(get(root, 0x48, 'i') == 0, 'Root Shard type differs')
    manager = get(state, 0x1b8); ap(manager, 0x1725f40, 'SubscriberManager')
    require(get(image, 0x13df690) == manager, 'SubscriberManager singleton differs')
    data, count, capacity, next_id = get(manager, 0x40), get(manager, 0x48, 'i'), get(manager, 0x4c, 'i'), get(manager, 0x68, 'i')
    require(0 <= count <= 1000000 and count <= capacity <= 1000000 and next_id >= 0 and (not count or data), 'SubscriberManager array bounds differ')
    members, ids, selected = [], set(), 0
    for i in range(count):
        sub = get(data, i * 8); ap(sub, 0x16e75f0, 'Subscriber')
        actual = get(sub, 0x20, 'i')
        require(0 <= actual < next_id and actual not in ids and sub not in members, 'Subscriber cohort identity duplicated/invalid')
        ids.add(actual); members.append(sub)
        if actual == serialized_id:
            selected = sub
    require(selected, f'Subscriber serialized_id={serialized_id} absent')
    toon = get(selected, 0x80); ap(toon, 0x1744e90, 'Toon')
    require(get(toon, 0x940) == selected, 'Toon reciprocal Subscriber differs')
    uid = get(toon, 0x18); require(uid != 0, 'Toon UID absent')
    shard, region, last = get(toon, 0x170), get(toon, 0x178), get(toon, 0x9a8)
    flags = {name: get(toon, off, 'B') for name, off in [('generated', 0x824), ('logged_in', 0x825), ('on_map', 0x368), ('queued', 0x9ec), ('logging_out', 0x9ed)]}
    require(all(value in (0, 1) for value in flags.values()), 'Toon lifecycle boolean differs')
    region_data, region_count, region_capacity = get(map_, 0x358), get(map_, 0x360, 'i'), get(map_, 0x364, 'i')
    require(0 <= region_count <= region_capacity <= 16384 and (not region_count or region_data), 'Owned Region array bounds differ')
    regions = [get(region_data, i * 8) for i in range(region_count)]
    require(len(set(regions)) == len(regions) and all(regions), 'Owned Region receiver missing/duplicate')
    def region_fields(p):
        if not p:
            return {'present': False, 'address': 0}
        ap(p, 0x1787930, 'Region')
        return {'present': True, 'address': p, 'saved_id': get(p, 0x834, 'i'), 'shard': get(p, 0x70),
                'owned': get(p, 0x850, 'B'), 'root_member': p in regions, 'toon_manager': get(p, 0x9c0)}
    result = {'game': game, 'mode': mode, 'state': state, 'world': world, 'map': map_, 'root': root,
            'manager': manager, 'manager_data': data, 'manager_count': count, 'manager_capacity': capacity,
            'manager_next_id': next_id, 'cohort': [(p, get(p, 0x20, 'i')) for p in members],
            'region_members': regions, 'region_data': region_data, 'region_capacity': region_capacity,
            'serialized_id': serialized_id, 'subscriber': selected, 'toon': toon, 'toon_uid': uid,
            'actor': get(toon, 0x6f0), 'shard': shard, 'shard_is_root': shard == root,
            'region': region_fields(region), 'last_region': region_fields(last), 'last_equals_region': last == region, **flags}
    if include_scenario_vips:
        schedule = {'tags': get(selected, 0x54, 'I'), 'vip_logout_flag': get(selected, 0x90, 'B'),
                    'connect_hour': get(selected, 0x118, 'i'), 'disconnect_hour': get(selected, 0x11c, 'i')}
        require(schedule['vip_logout_flag'] in (0, 1) and
                all(0 <= schedule[k] <= 23 for k in ('connect_hour', 'disconnect_hour')),
                'Subscriber adjusted play hours or VIP logout boolean invalid')
        result['play_schedule'] = schedule
        log = get(state, 0x1e8); ap(log, 0x16fd7b0, 'ScenarioLog')
        require(get(image, 0x13df330) == log, 'ScenarioLog singleton differs')
        instance_data, instance_count, instance_capacity = get(log, 0x20), get(log, 0x28, 'i'), get(log, 0x2c, 'i')
        require(0 <= instance_count <= 64 and instance_capacity >= instance_count and (not instance_count or instance_data),
                'ScenarioLog current array bounds differ')
        instances = [get(instance_data, i * 8) for i in range(instance_count)]
        require(all(instances) and len(set(instances)) == len(instances), 'ScenarioLog current member missing/duplicate')
        cohort = dict(result['cohort'])
        vips = []
        for instance in instances:
            if get(instance) != image + 0x1730e90:
                continue
            require(get(image + 0x1730e90, 0xa0) == image + 0x4f85b0, 'VIP Update virtual slot differs')
            subscriber = get(instance, 0x68)
            require(not subscriber or subscriber in cohort, 'VIP Subscriber is outside actual current cohort')
            started, complete = get(instance, 0x98, 'B'), get(instance, 0x38, 'B')
            require(started in (0, 1) and complete in (0, 1), 'VIP lifecycle boolean differs')
            vips.append({'instance': instance, 'subscriber': subscriber,
                         'serialized_id': cohort[subscriber] if subscriber else None,
                         'started': started, 'complete': complete,
                         'start_day': get(instance, 0x90, 'i'), 'start_hour': get(instance, 0x94, 'i')})
        result['scenario_vips'] = {'log': log, 'data': instance_data, 'count': instance_count,
                                   'capacity': instance_capacity, 'members': instances, 'vips': vips}
    if include_campaign_boosts:
        result['campaign_boosts'] = campaign_boost_sample(get, ap, image, mode, state)
    return result


def modules_guard(manifest, modules):
    directory = Path(manifest['executable_path']).parent
    expected = manifest.get('installed_binary_sha256')
    require(isinstance(expected, dict) and expected.get('MT2.exe') == INSTALLED_EXE_SHA256, 'Exact supported executable manifest hash missing/different')
    rows = {}
    paths = framework_module_paths(manifest) if manifest.get('schema_version') == 2 else {name: directory / name for name in ('MT2.exe', 'MT2_Multiplayer.dll')}
    for name, path in paths.items():
        path = path.resolve()
        selected = [m for m in modules if Path(m['path']).resolve() == path]
        require(len(selected) == 1 and selected[0]['base'] and selected[0]['size'] > 0, f'Exact loaded module missing/duplicated: {name}')
        require(isinstance(expected.get(name), str) and len(expected[name]) == 64, f'Manifest module hash missing: {name}')
        with path.open('rb') as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        require(digest == expected[name], f'Loaded module file hash changed: {name}', 'module_changed')
        rows[name] = {**selected[0], 'sha256': digest}
    return rows


def read(runtime, arguments, *, opener=ProcessRead, modules=None):
    require(isinstance(arguments, dict) and {'serialized_id'} <= set(arguments) <= {'serialized_id', 'include_scenario_vips', 'include_campaign_boosts'} and
            type(arguments['serialized_id']) is int and 0 <= arguments['serialized_id'] <= 2147483647 and
            type(arguments.get('include_scenario_vips', False)) is bool and
            type(arguments.get('include_campaign_boosts', False)) is bool,
            'read_subscriber_lifecycle requires serialized_id int32 and optional include_scenario_vips bool and include_campaign_boosts bool', 'invalid_args')
    include_vips = arguments.get('include_scenario_vips', False)
    include_boosts = arguments.get('include_campaign_boosts', False)
    modules = modules or (lambda pid: windows_modules(pid, details=True))
    with runtime.mutex:
        manifest = dict(runtime._current())
        require(manifest.get('role') == 'game', 'Fixed lifecycle read requires an owned MPTest game manifest')
        def status_pin():
            status = runtime.status(); native = status.get('native_world', {}); clock = status.get('clock', {})
            require(status.get('in_game') is True and clock.get('paused') is True and clock.get('scale') == 0 and
                    native.get('mode_state') == 4 and native.get('saves') == 0 and native.get('edits') == 0 and
                    native.get('world_present') is True and native.get('map_task_pending') is False and native.get('quitting') is False and native.get('title_pending') is False,
                    'Fixed lifecycle read requires paused stable World/Save0/Edit0')
            origin = status.get('native_open_origin', {})
            require(origin.get('current_origin_available') is True and origin.get('fresh_current_world_readback') is True, 'NativeOpen current owner proof missing')
            require(isinstance(origin.get('uuid'), str) and len(origin['uuid']) == 36 and
                    isinstance(origin.get('sha256'), str) and len(origin['sha256']) == 64 and
                    all(type(origin.get(k)) is int and origin[k] > 0 for k in ('bytes', 'world_epoch', 'generation')),
                    'NativeOpen identity fields missing/invalid')
            return {k: origin.get(k) for k in ('uuid', 'sha256', 'bytes', 'world_epoch', 'generation')}
        origin = status_pin()
        bound_modules = modules_guard(manifest, modules(manifest['pid']))
        image = bound_modules['MT2.exe']['base']
        with opener(manifest) as memory:
            first = sample(memory, image, arguments['serialized_id'], include_vips, include_boosts)
            second = sample(memory, image, arguments['serialized_id'], include_vips, include_boosts)
            require(first == second, 'Fixed Subscriber lifecycle double read changed', 'lifecycle_read_drift')
        require(runtime._current() == manifest and status_pin() == origin, 'Bound runtime/NativeOpen changed during read', 'lifecycle_read_drift')
        require(modules_guard(manifest, modules(manifest['pid'])) == bound_modules, 'Loaded module identity changed during read', 'lifecycle_read_drift')
        return {'read_only': True, 'attempted': False, 'read_confirmed': True, 'native_callback_invoked': False,
                'run_id': manifest['run_id'], 'pid': manifest['pid'], 'process_start_time': manifest['process_start_time'],
                'native_origin': origin, 'modules': bound_modules, 'observed': second, 'double_read_equal': True}
