"""Freeze the first executable coop placement candidate and measured DLL."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]
names = ['src/core/dll_main.cpp', 'src/hooks/hook_engine.cpp', 'src/game/ui_build_runtime.h',
         'src/game/ui_build_runtime.cpp', 'src/game/typed_action_runtime.h', 'src/game/typed_action_runtime.cpp',
         'src/game/remove_diagnostics.h', 'src/game/remove_diagnostics.cpp', 'include/mt2mp/game/native_engine.h',
         'src/game/native_engine.cpp', 'src/game/ui_remove_runtime.cpp', 'src/game/ui_remove/ui_capture.cpp', 'src/game/ui_remove/ui_capture.h',
         'src/game/ui_remove/diagnostic/ui_diagnostic.cpp', 'src/game/ui_remove/diagnostic/ui_diagnostic.h',
         'src/game/ui_remove/diagnostic/process_calls.cpp', 'src/game/ui_remove/diagnostic/scenery_ui_contract_bytes.h', 'src/game/typed_action/action_session.h',
         'src/game/typed_action/action_session.cpp', 'tools/mcp/runtime.py', 'tools/mcp/server.py',
         'tools/mcp/placement_ui_ops.py', 'src/game/ui_move_runtime.h', 'src/game/ui_move_runtime.cpp',
         'src/game/move_contract.h', 'src/game/move_contract.cpp', 'tools/mcp/move_ui_ops.py', 'tools/mcp/native_tool_prepare_ops.py',
         'src/game/object_identity/remove_runtime/owned_undo_open.h',
         'src/game/object_identity/remove_runtime/remove_runtime.h',
         'src/game/object_identity/remove_runtime/remove_runtime.cpp',
         'src/game/ui_region_runtime.h', 'src/game/ui_region_runtime.cpp', 'src/game/region_ui_readback.h', 'src/game/typed_action/region_action.h',
         'src/game/placement_ui_probe/tool_reader.cpp', 'src/game/ui_terrain_runtime.h', 'src/game/ui_terrain_runtime.inc',
         'src/game/typed_action/terrain_action.h', 'src/game/typed_action/terrain_codec.inc',
         'src/game/native_polyshape.h', 'src/game/terrain_codeguards.h', 'src/game/region_styles_runtime.inc', 'src/game/region_binding.h']
for directory in ('src/game/placement_kiss', 'src/game/multiplayer_world', 'src/game/placement_ui_probe', 'src/game/placement_ui_diagnostics', 'src/game/move_kiss', 'src/game/move_ui_probe', 'src/game/native_tool_prepare', 'src/game/scenery_kiss', 'src/game/live_host_state', 'src/game/live_host_state/day_rollover', 'src/game/native_refund'):
    names.extend(str(path.relative_to(ROOT)).replace('\\', '/') for path in (ROOT / directory).iterdir() if path.suffix in {'.h', '.cpp'})
names.extend(row['target'] for row in json.loads((ROOT / 'tools/lobby_ui/current_followup/copyplan.json').read_text(encoding='utf-8'))['files'])
names.extend(row['path'] for row in json.loads((ROOT / 'tools/lobby_ui/local_leave_fix/copyplan.json').read_text(encoding='utf-8'))['files'])
names.append('tools/local_test.py')
names.append('tools/mcp/subscriber_capture_ops.py')
names.append('tools/mcp/subscriber_append_ops.py')
names.append('tools/mcp/release_ops.py')
names.extend(str(path.relative_to(ROOT)).replace('\\', '/') for path in (ROOT / 'src/game/subscriber_lifecycle').iterdir() if path.suffix in {'.h', '.cpp'})
names.extend(str(path.relative_to(ROOT)).replace('\\', '/') for path in (ROOT / 'src/game/initial_release').iterdir() if path.suffix in {'.h', '.cpp'})
names.extend(json.loads((ROOT / 'artifacts/path_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/network_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/network_admission_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/node_observer_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/network_cable_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/node_frame_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/node_activation_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/node_activation_receipt_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/pipe_state_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/connection_state_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/chat_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/chat_integration/ui_receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/building_history_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_additions_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/snapshot_binding_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/desktop_app_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/region_properties_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/building_history_completion_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/region_unowned_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_nextid_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/path_delete_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_motion_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_online_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/price_actions_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/price_owner_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_capture_diagnostics_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_capture_motion_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/price_pane_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/economy_frame_budget_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/terrain_height_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_capture_owner_diagnostics_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_startup_quaternion_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/terrain_height_driver_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/terrain_height_baseline_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_capture_pause_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_quaternion_bounds_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/terrain_height_extent_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_needs_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_query_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_needs_refresh_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/region_level_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/region_level_ap_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/frame_timing_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/fps_controller_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_activity_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_ui_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_ui_context_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_store_guard_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_place_probe_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_scalar_phase_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_character_order_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_bulk_read_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/apply_timing_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_fresh_read_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_action_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_action_mcp_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_region_sample_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_cursor_gate_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/network_noop_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_release_capture_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_capture_order_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_canonical_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_host_prepare_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/network_scalar_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_retirement_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/subscriber_retirement_runner_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/motion_sample_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/query_timing_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/page_read_scope_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_remove_protocol_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_remove_native_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_remove_window_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_remove_replay_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_move_protocol_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_move_native_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_move_visibility_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_visual_diag_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_local_visibility_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_quest_records_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_quest_zone_key_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_quest_protocol_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_quest_targets_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_quest_native_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_quest_target_runtime_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(json.loads((ROOT / 'artifacts/npc_quest_retirement_records_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(('src/game/npc_quest_visitor_reader.h', 'src/game/npc_quest_visitors.inc'))
names.extend(('src/game/subscriber_quest_state.h', 'src/game/npc_host_state_binding.h',
              'src/game/npc_quest_daily_apply.h',
              'src/game/live_host_state/day_rollover/economy/native_quest_state.inc',
              'src/game/live_host_state/day_rollover/economy/state_difference.h',
              'src/game/live_host_state/day_rollover/economy/quest_codec.inc'))
names.extend(json.loads((ROOT / 'artifacts/npc_quest_runner_integration/receipt.json').read_text(encoding='utf-8'))['source_sha256'])
names.extend(('tools/npc_placement_slice/accept_npc_coop.py', 'tools/npc_placement_slice/accept_original_probe.py'))
for manifest_path in ('tools/live_host_state/day_rollover/economy/current_followup/manifest.json',
                      'tools/live_host_state/day_rollover/economy/native_gate/integration/manifest.json'):
    manifest = json.loads((ROOT / manifest_path).read_text(encoding='utf-8'))
    names.extend(row['target'] for row in manifest['patches'] + manifest['copies'])
stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
target = ROOT / 'artifacts/placement_kiss' / stamp / 'freeze.json'
target.parent.mkdir(parents=True, exist_ok=False)
value = {'candidate': 'TWO-PEER-COOP', 'scope': 'Building Undo/Redo cash scope, Region name/type/purchase and automatic subscriber additions candidate. Native acceptance remains open. Existing confirmed actions, local lobby, Desktop, chat, cash, clock and first Day proofs carried',
         'files': {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in names},
         'dll_sha256': hashlib.sha256((ROOT / 'build/bin/Release/MT2_Multiplayer.dll').read_bytes()).hexdigest()}
target.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')
print(str(target))
