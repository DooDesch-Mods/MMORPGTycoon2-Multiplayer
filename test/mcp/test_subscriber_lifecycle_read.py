"""Fixed reader tests use byte-addressed fixtures, never a game process."""
from copy import deepcopy
import hashlib
import io
import json
from pathlib import Path
import struct
import sys
import threading
import unittest
from unittest.mock import Mock, patch

HERE = Path(__file__).resolve()
ROOT = next(p for p in HERE.parents if (p / 'tools/mcp/error_details.py').exists())
sys.path.insert(0, str(ROOT / 'tools/mcp'))
sys.path.insert(0, str(HERE.parents[2] / 'tools/mcp'))
import subscriber_lifecycle_read as reader
from runtime import RuntimeErrorDetail
import server
import run_job


class Memory:
    image = 0x140000000
    game, mode, state, world, map, manager = [0x100000 + 0x10000 * i for i in range(6)]
    sub, toon, region, other_region, data, regions = [0x200000 + 0x10000 * i for i in range(6)]

    def __init__(self):
        self.bytes = {}; self.reads = 0; self.closed = False
        self.root = self.map + 0x1c0
        for pointer, rva in [(self.game, 0x1743740), (self.mode, 0x16e6730), (self.state, 0x16de920),
                             (self.world, 0x174aef0), (self.map, 0x1742860), (self.root, 0x174ad20),
                             (self.manager, 0x1725f40), (self.sub, 0x16e75f0), (self.toon, 0x1744e90),
                             (self.region, 0x1787930), (self.other_region, 0x1787930)]:
            self.set(pointer, 0, self.image + rva)
        for off, value in [(0x13df8f0,self.game),(0x13df320,self.mode),(0x13df2a0,self.state),
                           (0x13df9c0,self.world),(0x13df8c0,self.map),(0x13df690,self.manager)]:
            self.set(self.image, off, value)
        for p, off, value in [(self.game,0x28,self.mode),(self.mode,0x58,self.state),(self.state,0x160,self.world),
                              (self.world,0x158,self.map),(self.state,0x1b8,self.manager),(self.manager,0x40,self.data),
                              (self.data,0,self.sub),(self.sub,0x80,self.toon),(self.toon,0x940,self.sub),
                              (self.toon,0x18,123),(self.toon,0x170,self.root),(self.toon,0x178,self.region),
                              (self.toon,0x9a8,0),(self.toon,0x6f0,0),(self.map,0x358,self.regions),
                              (self.regions,0,self.region)]: self.set(p,off,value)
        for p, off, value in [(self.mode,0x2c4,4),(self.root,0x48,0),(self.manager,0x48,1),
                              (self.manager,0x4c,4),(self.manager,0x68,115),(self.sub,0x20,0),
                              (self.map,0x360,1),(self.map,0x364,4)]: self.set(p,off,value,'i')
        for off, value in [(0x824,1),(0x825,0),(0x368,0),(0x9ec,0),(0x9ed,0)]: self.set(self.toon,off,value,'B')
        for p, id_ in [(self.region,67),(self.other_region,68)]:
            self.set(p,0x834,id_,'i'); self.set(p,0x70,self.root); self.set(p,0x850,1,'B'); self.set(p,0x9c0,0x700000)

    def set(self,p,off,value,fmt='Q'):
        for i,b in enumerate(struct.pack('<'+fmt,value)): self.bytes[p+off+i]=b

    def __call__(self,p,size):
        self.reads += 1
        try: return bytes(self.bytes[p+i] for i in range(size))
        except KeyError as exc: raise RuntimeErrorDetail('process_read_failed',f'fixture unreadable address={p} size={size}') from exc

    def __enter__(self): return self
    def __exit__(self,*_): self.closed=True


def runtime():
    r=Mock(); r.mutex=threading.RLock()
    r._current.return_value={'role':'game','pid':36472,'run_id':'run','process_start_time':'123','executable_path':'test/MT2.exe'}
    r.status.return_value={'in_game':True,'clock':{'paused':True,'scale':0},'native_world':{
        'mode_state':4,'saves':0,'edits':0,'world_present':True,'map_task_pending':False,'quitting':False,'title_pending':False},
        'native_open_origin':{'current_origin_available':True,'fresh_current_world_readback':True,
            'uuid':'d2a32dbd-fe16-40aa-849c-1430cc3d1f47','sha256':'a'*64,'bytes':246312,'world_epoch':1,'generation':1}}
    return r


class FixedLifecycle(unittest.TestCase):
    def test_offline_null_last_and_separate_region_fields(self):
        m=Memory(); out=reader.sample(m,m.image,0)
        self.assertEqual(out['last_region'],{'present':False,'address':0})
        self.assertTrue(out['shard_is_root']); self.assertFalse(out['last_equals_region'])
        self.assertEqual(out['region']['saved_id'],67); self.assertEqual(out['logged_in'],0)

    def test_online_and_mismatched_tuple_are_distinct_observations(self):
        m=Memory(); m.set(m.toon,0x9a8,m.region); m.set(m.toon,0x825,1,'B')
        self.assertTrue(reader.sample(m,m.image,0)['last_equals_region'])
        m.set(m.toon,0x9a8,m.other_region); m.set(m.toon,0x170,0)
        out=reader.sample(m,m.image,0)
        self.assertFalse(out['shard_is_root']); self.assertFalse(out['last_equals_region'])
        self.assertEqual(out['last_region']['saved_id'],68)

    def test_exact_owner_bounds_and_reciprocal_guards(self):
        cases=[('toon',0,0,'Q'),('toon',0x940,0,'Q'),('toon',0x18,0,'Q'),('toon',0x825,2,'B'),
               ('manager',0x48,5,'i'),('map',0x360,5,'i'),('mode',0x2c4,3,'i'),('region',0,0,'Q')]
        for owner,off,value,fmt in cases:
            with self.subTest(owner=owner,off=off):
                m=Memory();m.set(getattr(m,owner),off,value,fmt)
                with self.assertRaises(RuntimeErrorDetail):reader.sample(m,m.image,0)

    def test_duplicate_absent_and_original_read_failure(self):
        m=Memory();m.set(m.manager,0x48,2,'i');m.set(m.data,8,m.sub)
        with self.assertRaisesRegex(RuntimeErrorDetail,'duplicated'):reader.sample(m,m.image,0)
        m=Memory()
        with self.assertRaisesRegex(RuntimeErrorDetail,'absent'):reader.sample(m,m.image,114)
        del m.bytes[m.toon+0x9a8]
        with self.assertRaisesRegex(RuntimeErrorDetail,'fixture unreadable'):reader.sample(m,m.image,0)

    def invoke(self,r=None,m=None):
        r=r or runtime();m=m or Memory()
        with patch.object(reader,'modules_guard',return_value={'MT2.exe':{'base':m.image}}):
            result=reader.read(r,{'serialized_id':0},opener=lambda manifest:m,modules=lambda pid:[])
        r.request.assert_not_called();self.assertTrue(m.closed)
        return result

    def test_double_read_no_callbacks(self):
        result=self.invoke()
        self.assertTrue(result['read_confirmed']);self.assertTrue(result['double_read_equal'])
        self.assertFalse(result['attempted']);self.assertFalse(result['native_callback_invoked'])
        self.assertEqual(result['native_origin']['generation'],1)

    def test_double_read_drift_is_not_accepted(self):
        r=runtime();m=Memory();real=reader.sample
        def changing(*args):
            result=real(*args);m.set(m.toon,0x9a8,m.region);return result
        with patch.object(reader,'sample',side_effect=changing),self.assertRaisesRegex(RuntimeErrorDetail,'double read changed'):
            self.invoke(r,m)
        self.assertTrue(m.closed);r.request.assert_not_called()

    def test_status_idle_and_origin_guards(self):
        cases=[('clock','paused',False),('clock','scale',1),('native_world','saves',1),('native_world','edits',1),
               ('native_world','map_task_pending',True),('native_world','quitting',True),
               ('native_open_origin','fresh_current_world_readback',False),('native_open_origin','world_epoch',None)]
        for section,key,value in cases:
            with self.subTest(section=section,key=key):
                r=runtime();r.status.return_value[section][key]=value
                with self.assertRaises(RuntimeErrorDetail):self.invoke(r)
                r.request.assert_not_called()

    def test_origin_change_and_pid_reuse_stay_errors(self):
        r=runtime();before=deepcopy(r.status.return_value);after=deepcopy(before);after['native_open_origin']['generation']=2
        r.status.side_effect=[before,after]
        with self.assertRaisesRegex(RuntimeErrorDetail,'NativeOpen changed'):self.invoke(r)
        r=runtime();r._current.side_effect=RuntimeErrorDetail('pid_reused','original process start token differs')
        with self.assertRaisesRegex(RuntimeErrorDetail,'original process start token'):self.invoke(r)

    def test_exact_args_and_static_dispatch(self):
        for args in ({},{'serialized_id':True},{'serialized_id':-1},{'serialized_id':2147483648},{'serialized_id':0,'address':1}):
            with self.subTest(args=args),self.assertRaises(RuntimeErrorDetail):reader.read(runtime(),args)
        r=runtime()
        with patch.object(reader,'read',return_value={'read_only':True}) as call:
            self.assertEqual(server.dispatch(r,'read_subscriber_lifecycle',{'serialized_id':0}),{'read_only':True})
            call.assert_called_once_with(r,{'serialized_id':0})
        self.assertTrue(server.BASE['read_subscriber_lifecycle'][2]);r.request.assert_not_called()
        self.assertIn('read_subscriber_lifecycle',run_job.ALLOWED_TOOLS)

    def test_exact_loaded_module_file_hash(self):
        root=ROOT/'tools'/'test-only-module-path'; rows=[]; hashes={}; files={}
        for i,name in enumerate(('MT2.exe','MT2_Multiplayer.dll')):
            p=root/name;files[p]=name.encode();hashes[name]=hashlib.sha256(files[p]).hexdigest()
            rows.append({'path':str(p),'base':0x140000000+i*0x10000000,'size':0x2000000})
        manifest={'executable_path':str(root/'MT2.exe'),'installed_binary_sha256':hashes}
        with patch.object(reader,'INSTALLED_EXE_SHA256',hashes['MT2.exe']),patch.object(Path,'open',lambda p,mode:io.BytesIO(files[p])):
            self.assertEqual(reader.modules_guard(manifest,rows)['MT2.exe']['sha256'],hashes['MT2.exe'])
            with self.assertRaisesRegex(RuntimeErrorDetail,'missing/duplicated'):reader.modules_guard(manifest,rows+rows[:1])
            files[root/'MT2_Multiplayer.dll']=b'changed'
            with self.assertRaisesRegex(RuntimeErrorDetail,'hash changed'):reader.modules_guard(manifest,rows)

    def test_primary_and_cleanup_failures_preserved(self):
        r=reader.ProcessRead.__new__(reader.ProcessRead);r.handle=1;r.kernel=Mock();r.kernel.CloseHandle.return_value=0
        with patch.object(reader.ctypes,'get_last_error',return_value=5):
            with self.assertRaisesRegex(RuntimeErrorDetail,'original read failure.*cleanup also failed.*CloseHandle'):
                r.__exit__(RuntimeErrorDetail,RuntimeErrorDetail('original','original read failure'),None)
        self.assertIsNone(r.handle)


class FixedVipWitness(unittest.TestCase):
    def memory(self, linked=True):
        m=Memory()
        m.log,m.instances,m.vip,m.other_instance,m.vip_sub = 0x810000,0x820000,0x830000,0x840000,0x850000
        for p,off,value in [(m.state,0x1e8,m.log),(m.image,0x13df330,m.log),
                            (m.log,0,m.image+0x16fd7b0),(m.log,0x20,m.instances),
                            (m.instances,0,m.vip),(m.instances,8,m.other_instance),
                            (m.vip,0,m.image+0x1730e90),(m.other_instance,0,m.image+0x1234),
                            (m.image+0x1730e90,0xa0,m.image+0x4f85b0),
                            (m.vip,0x68,m.vip_sub if linked else 0),
                            (m.data,8,m.vip_sub),(m.vip_sub,0,m.image+0x16e75f0)]:m.set(p,off,value)
        for p,off,value in [(m.log,0x28,2),(m.log,0x2c,4),(m.manager,0x48,2),(m.manager,0x68,144),
                            (m.vip_sub,0x20,143),(m.vip,0x90,2),(m.vip,0x94,0)]:m.set(p,off,value,'i')
        m.set(m.vip,0x98,int(linked),'B');m.set(m.vip,0x38,0,'B')
        m.set(m.sub,0x54,0x800,'I');m.set(m.sub,0x90,0,'B')
        m.set(m.sub,0x118,22,'i');m.set(m.sub,0x11c,4,'i')
        return m

    def test_default_observation_is_unchanged(self):
        m=Memory()
        self.assertEqual(reader.sample(m,m.image,0),reader.sample(m,m.image,0,False))
        self.assertNotIn('scenario_vips',reader.sample(m,m.image,0))

    def test_exact_link_identifies_vip_not_selected_ordinary_subscriber(self):
        m=self.memory();out=reader.sample(m,m.image,0,True)
        self.assertEqual(out['serialized_id'],0)
        self.assertEqual(out['play_schedule'],{'tags':0x800,'vip_logout_flag':0,
                                              'connect_hour':22,'disconnect_hour':4})
        self.assertEqual(out['scenario_vips']['vips'],[{'instance':m.vip,'subscriber':m.vip_sub,
            'serialized_id':143,'started':1,'complete':0,'start_day':2,'start_hour':0}])
        self.assertEqual(out['scenario_vips']['members'],[m.vip,m.other_instance])

    def test_schedule_bounds_and_double_read_drift_are_errors(self):
        for off,value,fmt in ((0x118,-1,'i'),(0x118,24,'i'),(0x11c,-1,'i'),
                              (0x11c,24,'i'),(0x90,2,'B')):
            with self.subTest(off=off,value=value):
                m=self.memory();m.set(m.sub,off,value,fmt)
                with self.assertRaisesRegex(RuntimeErrorDetail,'adjusted play hours'):
                    reader.sample(m,m.image,0,True)
        m=self.memory();real=reader.sample
        def change(*args):
            result=real(*args);m.set(m.sub,0x11c,5,'i');return result
        with patch.object(reader,'sample',side_effect=change),self.assertRaisesRegex(RuntimeErrorDetail,'double read changed'):
            self.invoke(m)
        self.assertTrue(m.closed)

    def test_prebirth_null_is_explicit_and_empty_log_is_not_birth(self):
        m=self.memory(False)
        self.assertIsNone(reader.sample(m,m.image,0,True)['scenario_vips']['vips'][0]['serialized_id'])
        m.set(m.log,0x28,0,'i');m.set(m.log,0x20,0)
        self.assertEqual(reader.sample(m,m.image,0,True)['scenario_vips']['vips'],[])

    def test_owner_class_membership_and_bounds_fail(self):
        cases=[('log',0,0,'Q','addresspoint'),('image',0x13df330,0,'Q','singleton'),
               ('log',0x28,-1,'i','bounds'),('log',0x28,65,'i','bounds'),('log',0x2c,1,'i','bounds'),
               ('log',0x20,0,'Q','bounds'),('instances',0,0,'Q','missing'),
               ('vip',0x68,0x999000,'Q','outside actual current cohort'),('vip',0x98,2,'B','boolean'),
               ('vip',0x38,2,'B','boolean')]
        for owner,off,value,fmt,cause in cases:
            with self.subTest(owner=owner,off=off):
                m=self.memory();m.set(getattr(m,owner),off,value,fmt)
                with self.assertRaisesRegex(RuntimeErrorDetail,cause):reader.sample(m,m.image,0,True)
        m=self.memory();m.set(m.instances,8,m.vip)
        with self.assertRaisesRegex(RuntimeErrorDetail,'duplicate'):reader.sample(m,m.image,0,True)
        m=self.memory();m.set(m.image+0x1730e90,0xa0,m.image+0x111)
        with self.assertRaisesRegex(RuntimeErrorDetail,'virtual slot'):reader.sample(m,m.image,0,True)

    def invoke(self, m):
        r=runtime()
        with patch.object(reader,'modules_guard',return_value={'MT2.exe':{'base':m.image}}):
            result=reader.read(r,{'serialized_id':0,'include_scenario_vips':True},opener=lambda manifest:m,modules=lambda pid:[])
        r.request.assert_not_called()
        return result

    def test_optional_double_read_uses_existing_guards_no_mailbox(self):
        m=self.memory();result=self.invoke(m)
        self.assertTrue(result['read_confirmed'] and result['double_read_equal'] and m.closed)
        self.assertFalse(result['attempted'] or result['native_callback_invoked'])
        self.assertEqual(result['observed']['scenario_vips']['vips'][0]['serialized_id'],143)

    def test_vip_link_drift_and_unreadable_pointer_keep_cause(self):
        m=self.memory();real=reader.sample
        def change(*args):
            result=real(*args);m.set(m.vip,0x68,0);return result
        with patch.object(reader,'sample',side_effect=change),self.assertRaisesRegex(RuntimeErrorDetail,'double read changed'):
            self.invoke(m)
        self.assertTrue(m.closed)
        m=self.memory();del m.bytes[m.vip+0x68]
        with self.assertRaisesRegex(RuntimeErrorDetail,'fixture unreadable'):self.invoke(m)
        self.assertTrue(m.closed)

    def test_optional_schema_and_fixed_dispatch(self):
        schema=server.BASE['read_subscriber_lifecycle'][1]
        self.assertEqual(schema['properties']['include_scenario_vips'],{'type':'boolean','default':False})
        self.assertEqual(schema['required'],['serialized_id'])
        args={'serialized_id':0,'include_scenario_vips':True}
        r=runtime()
        with patch.object(reader,'read',return_value={'read_only':True}) as call:
            server.dispatch(r,'read_subscriber_lifecycle',args);call.assert_called_once_with(r,args)
        r.request.assert_not_called()
        for value in (0,1,'true',None):
            with self.subTest(value=value),self.assertRaisesRegex(RuntimeErrorDetail,'optional include_scenario_vips bool'):
                reader.read(r,{'serialized_id':0,'include_scenario_vips':value})


class FixedCampaignBoostWitness(unittest.TestCase):
    def memory(self):
        m=Memory()
        m.marketing,m.campaigns,m.campaign,m.awards,m.award,m.hud,m.boost,m.views,m.view,m.panes = range(0x910000,0x9b0000,0x10000)
        for p,rva in [(m.marketing,0x16dfa60),(m.campaign,0x16e3d50),(m.hud,0x1742750),
                       (m.boost,0x16fa4d0),(m.boost+0x508,0x16e1770),(m.view,0x16dd8a0)]:m.set(p,0,m.image+rva)
        for p,off,value in [(m.state,0x1d0,m.marketing),(m.image,0x13df2c0,m.marketing),
                            (m.mode,0x128,m.hud),(m.image,0x13df8b0,m.hud),(m.hud,0xb8,m.boost),
                            (m.campaigns,0,m.campaign),(m.awards,0,m.award),(m.views,0,m.view),
                            (m.panes,0,m.view),(m.view,0x328,m.campaign),(m.view,0x320,0),
                            (m.view,0x278,m.boost),(m.view,0x280,m.boost)]:m.set(p,off,value)
        for p,off,data in [(m.marketing,0x70,m.campaigns),(m.marketing,0x40,m.awards),
                           (m.boost,0x510,m.views),(m.boost,0x300,m.panes)]:
            m.set(p,off,data);m.set(p,off+8,1,'i');m.set(p,off+12,4,'i')
        for off,value in [(0x50,4),(0x54,2),(0x58,3)]:m.set(m.campaign,off,value,'i')
        m.set(m.boost,0x138,400.0,'f');m.set(m.boost,0x13c,45.0,'f')
        return m

    def observation(self,m):
        return reader.sample(m,m.image,0,include_campaign_boosts=True)['campaign_boosts']

    def invoke(self,m,r=None):
        r=r or runtime()
        with patch.object(reader,'modules_guard',return_value={'MT2.exe':{'base':m.image}}):
            result=reader.read(r,{'serialized_id':0,'include_campaign_boosts':True},opener=lambda manifest:m,modules=lambda pid:[])
        r.request.assert_not_called()
        return result

    def test_current_campaign_and_local_binding(self):
        m=self.memory();out=self.observation(m)
        self.assertEqual(out['campaigns']['rows'],[{'address':m.campaign,'type':4,'start_day':2,'duration_days':3}])
        self.assertEqual(out['views']['bindings'],[{'address':m.view,'campaign':m.campaign,'award':0,
            'campaign_member':True,'award_member':False,'pane_member':True,'parent':m.boost,'window':m.boost}])
        result=self.invoke(m)
        self.assertTrue(result['read_confirmed'] and result['double_read_equal'] and m.closed)
        self.assertFalse(result['attempted'] or result['native_callback_invoked'])

    def test_absent_campaign_and_stale_pane_are_distinct_without_foreign_dereference(self):
        m=self.memory();m.set(m.marketing,0x78,0,'i');m.set(m.marketing,0x70,0)
        out=self.observation(m)
        self.assertEqual(out['campaigns']['rows'],[])
        self.assertFalse(out['views']['bindings'][0]['campaign_member'])
        m=self.memory();m.set(m.view,0x328,0xdeadbeef)
        out=self.observation(m)
        self.assertEqual(out['views']['bindings'][0]['campaign'],0xdeadbeef)
        self.assertFalse(out['views']['bindings'][0]['campaign_member'])
        m=self.memory();m.set(m.boost,0x518,0,'i');m.set(m.boost,0x308,0,'i')
        out=self.observation(m)
        self.assertEqual(out['campaigns']['count'],1);self.assertEqual(out['views']['count'],0)

    def test_award_pane_and_membership_mismatch_are_observed(self):
        m=self.memory();m.set(m.view,0x328,0);m.set(m.view,0x320,m.award)
        m.set(m.boost,0x308,0,'i');m.set(m.view,0x278,0);m.set(m.view,0x280,0)
        out=self.observation(m)['views']['bindings'][0]
        self.assertTrue(out['award_member']);self.assertFalse(out['campaign_member'] or out['pane_member'])
        self.assertEqual(out['parent'],0);self.assertEqual(out['window'],0)

    def test_missing_window_and_default_option(self):
        m=self.memory();m.set(m.hud,0xb8,0)
        out=self.observation(m)
        self.assertFalse(out['window_present']);self.assertNotIn('views',out)
        m=Memory();self.assertNotIn('campaign_boosts',reader.sample(m,m.image,0))

    def test_current_owners_classes_and_array_guards(self):
        cases=[('image',0x13df2c0,0,'Q','singleton'),('state',0x1d0,0,'Q','singleton'),
               ('image',0x13df8b0,0,'Q','singleton'),('mode',0x128,0,'Q','singleton'),
               ('marketing',0,0,'Q','addresspoint'),('campaign',0,0,'Q','addresspoint'),
               ('hud',0,0,'Q','addresspoint'),('boost',0,0,'Q','addresspoint'),
               ('boost',0x508,0,'Q','addresspoint'),('view',0,0,'Q','addresspoint'),
               ('marketing',0x78,-1,'i','bounds'),('marketing',0x7c,513,'i','bounds'),
               ('marketing',0x7c,0,'i','bounds'),('marketing',0x70,0,'Q','bounds'),
               ('boost',0x518,513,'i','bounds'),('boost',0x30c,0,'i','bounds'),
               ('views',0,0,'Q','missing')]
        for owner,off,value,fmt,cause in cases:
            with self.subTest(owner=owner,off=off):
                m=self.memory();m.set(getattr(m,owner),off,value,fmt)
                with self.assertRaisesRegex(RuntimeErrorDetail,cause):self.observation(m)
        for owner,offset,data in [('marketing',0x78,'campaigns'),('boost',0x518,'views')]:
            m=self.memory();m.set(getattr(m,owner),offset,2,'i')
            p=getattr(m,data);m.set(p,8,struct.unpack('<Q',m(p,8))[0])
            with self.assertRaisesRegex(RuntimeErrorDetail,'duplicate'):self.observation(m)

    def test_campaign_binding_and_owner_drift_preserve_failure_and_close(self):
        for owner,off,value,fmt in [('campaign',0x58,4,'i'),('view',0x328,0,'Q'),('boost',0x308,0,'i')]:
            with self.subTest(owner=owner):
                m=self.memory();real=reader.sample
                def change(*args):
                    result=real(*args);m.set(getattr(m,owner),off,value,fmt);return result
                with patch.object(reader,'sample',side_effect=change),self.assertRaisesRegex(RuntimeErrorDetail,'double read changed'):
                    self.invoke(m)
                self.assertTrue(m.closed)
        m=self.memory();del m.bytes[m.campaign+0x58]
        with self.assertRaisesRegex(RuntimeErrorDetail,'fixture unreadable'):self.invoke(m)
        self.assertTrue(m.closed)

    def test_optional_schema_dispatch_and_strict_boolean(self):
        schema=server.BASE['read_subscriber_lifecycle'][1]
        self.assertEqual(schema['properties']['include_campaign_boosts'],{'type':'boolean','default':False})
        self.assertEqual(schema['required'],['serialized_id'])
        args={'serialized_id':0,'include_campaign_boosts':True};r=runtime()
        with patch.object(reader,'read',return_value={'read_only':True}) as call:
            server.dispatch(r,'read_subscriber_lifecycle',args);call.assert_called_once_with(r,args)
        r.request.assert_not_called()
        for value in (0,1,'true',None):
            with self.subTest(value=value),self.assertRaisesRegex(RuntimeErrorDetail,'include_campaign_boosts bool'):
                reader.read(r,{'serialized_id':0,'include_campaign_boosts':value})



if __name__=='__main__':unittest.main()
