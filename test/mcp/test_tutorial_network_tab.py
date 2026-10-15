from pathlib import Path
import sys
from unittest.mock import Mock
r=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(r/'tools/mcp'))
from server import dispatch,tools_for
from runtime import OPS,MUTATIONS,RuntimeErrorDetail
good={'token':'b473438a-510a-4f85-8337-b4e154ea15d6','snapshot_uuid':'77e1531e-709c-4355-b079-465f53cb61a2','world_epoch':1,'world_generation':2,'current_step_index':56}
count=0
for op,index in [('tutorial_network_tab',56),('tutorial_welcome_ok',4),('tutorial_finish_ok',103)]:
 rt=Mock();rt.available.return_value={op};tool=next(t for t in tools_for(rt) if t.name=='game_'+op)
 assert op in OPS and op in MUTATIONS and not tool.annotations.readOnlyHint
 assert tool.inputSchema['properties']['current_step_index']['const']==index
 receipt={'attempted':True,'returned':False,'confirmed':False,'unknown':True,'cause':'original Press exception'};rt.request.return_value=receipt
 assert dispatch(rt,'game_'+op,{**good,'current_step_index':index}) is receipt
 rt.request.assert_called_once_with(op,{**good,'current_step_index':index});count+=1
for bad in ({},{**good,'current_step_index':103},{**good,'current_step_index':4},{**good,'current_step_index':True},{**good,'world_epoch':0},{**good,'world_generation':True},{**good,'token':'repeat'},{**good,'tab':'Grid'},{**good,'command':'anything'},{**good,'button':42}):
 rt=Mock()
 try:dispatch(rt,'game_tutorial_network_tab',bad)
 except RuntimeErrorDetail:pass
 else:raise AssertionError(bad)
 rt.request.assert_not_called();count+=1
rt=Mock();rt.request.side_effect=RuntimeErrorDetail('TUTORIAL_NETWORK_TAB_REJECTED','original cause')
try:dispatch(rt,'game_tutorial_network_tab',good)
except RuntimeErrorDetail as e:assert 'original cause' in str(e)
else:raise AssertionError('cause lost')
count+=1
for index in (2,4,6,9,11,13,14):
 op='tutorial_leveltwo_message_ok';rt=Mock();rt.available.return_value={op};tool=next(t for t in tools_for(rt) if t.name=='game_'+op)
 assert op in OPS and op in MUTATIONS and not tool.annotations.readOnlyHint
 assert tool.inputSchema['properties']['current_step_index']['enum']==[2,4,6,9,11,13,14]
 args={**good,'current_step_index':index};receipt={'attempted':True,'confirmed':False,'unknown':True};rt.request.return_value=receipt
 assert dispatch(rt,'game_'+op,args) is receipt;rt.request.assert_called_once_with(op,args);count+=1
for bad in ({},{**good,'current_step_index':-1},{**good,'current_step_index':0},{**good,'current_step_index':3},{**good,'current_step_index':15},{**good,'current_step_index':103},{**good,'current_step_index':True},{**good,'current_step_index':2,'world_epoch':0},{**good,'current_step_index':2,'world_generation':True},{**good,'current_step_index':2,'scenario':'initial_tutorial'},{**good,'current_step_index':2,'message':'welcome'},{**good,'current_step_index':2,'token':'repeat'}):
 rt=Mock()
 try:dispatch(rt,'game_tutorial_leveltwo_message_ok',bad)
 except RuntimeErrorDetail:pass
 else:raise AssertionError(bad)
 rt.request.assert_not_called();count+=1
rt=Mock();rt.request.side_effect=RuntimeErrorDetail('TUTORIAL_LEVELTWO_MESSAGE_REJECTED','original LevelTwo cause')
try:dispatch(rt,'game_tutorial_leveltwo_message_ok',{**good,'current_step_index':2})
except RuntimeErrorDetail as e:assert 'original LevelTwo cause' in str(e)
else:raise AssertionError('LevelTwo cause lost')
count+=1
assert 'tutorial_leveltwo_welcome_ok' not in OPS and 'tutorial_leveltwo_welcome_ok' not in MUTATIONS
print(f'PASS {count} actual SDK adapter cases')
