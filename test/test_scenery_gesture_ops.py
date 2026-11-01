import copy
import importlib.util
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/mcp'))
import scenery_gesture_ops as ops

class Schema(unittest.TestCase):
    def args(self):
        return dict(token='30000000-0000-0000-0000-000000000001',snapshot_uuid='10000000-0000-0000-0000-000000000001',expected_world_epoch=2,expected_generation=3,kind='paint',region_id=7,points=[[1,2,3],[4,2,3]])
    def test_valid(self):
        for kind in ('paint','erase'):
            args=self.args();args['kind']=kind;ops.validate('native_scenery_gesture',args)
        ops.validate('read_scenery_gesture',{'token':self.args()['token']})
    def test_bad_fields(self):
        cases={'token':['x',None,12], 'snapshot_uuid':['x','10000000-0000-0000-0000-00000000000A'], 'kind':['modular',None], 'region_id':[-1,2**31,True], 'expected_world_epoch':[0,2**64,True], 'expected_generation':[-1,1.5], 'points':[[],[[1,2,3]],[[1,2,3]]*33,[[1,2],[1,2,3]],[[float('nan'),0,0],[1,2,3]],[[1e40,0,0],[1,2,3]],[[True,0,0],[1,2,3]]]}
        for key,values in cases.items():
            for value in values:
                args=self.args();args[key]=value
                with self.subTest(key=key,value=value),self.assertRaises(ops.RuntimeErrorDetail):ops.validate('native_scenery_gesture',args)
        for args in ({},dict(self.args(),extra=True)):
            with self.assertRaises(ops.RuntimeErrorDetail):ops.validate('native_scenery_gesture',args)
        with self.assertRaises(ops.RuntimeErrorDetail):ops.validate('read_scenery_gesture',self.args())

if __name__=='__main__':unittest.main()
