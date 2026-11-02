from pathlib import Path
import argparse
p = argparse.ArgumentParser()
p.add_argument("--source", type=Path, required=True)
p.add_argument("--output", type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)

root = a.output
source = (a.source / 'src/game/building_move_settlement.inc').read_text(encoding='utf-8')
validation = source[source.index('template<class Ground>bool validate_moved_height('):source.index('\n}\nbool settle_building_move(')]
batch = source[source.index('bool settle_selection_buildings('):source.rfind('\n}')]
prefix = r'''#include "typed_action/action_session.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
using json=nlohmann::json;namespace action=mt2mp::action_session;
struct SelectionBuildingSettlement {std::string key;uint64_t uid=0;action::BuildingState immediate;};
struct Runtime {std::atomic<bool> ready{true};unsigned thread=7;uintptr_t image=0;struct {void*(*find)(uint64_t)=nullptr;} api;};Runtime runtime;std::atomic<Runtime*> published{&runtime};
unsigned GetCurrentThreadId(){return 7;}bool scope_ok=true,drain_ok=true,ground_ok=true,normal=true,box_ok=true;unsigned drains=0,reads=0;bool alter_x=false,late_fail=false,changed_object=false;
void* fake_find(uint64_t uid){return reinterpret_cast<void*>(uintptr_t(uid+100+(changed_object&&drains?1:0)));}
uint32_t selection_building_lookup(void*(*find)(uint64_t),uint64_t uid,uintptr_t& object){object=uintptr_t(find(uid));return 0;}
bool fail(std::string& e,const std::string& why){e=why;return false;}
namespace mt2mp::action_session::tick_commands::authority {bool matches(const Binding&,const Request&,std::string& e){if(!::scope_ok)e="scope mismatch";return ::scope_ok;}}
namespace host {struct Origin {int value=1;bool operator==(const Origin&)const=default;};struct WorldState {Origin origin;uintptr_t root=99;};bool same_world(const Origin& a,const Origin& b){return a==b;}}
bool world(Runtime&,host::WorldState& out,std::string&){out={};return true;}
namespace native {struct BuildingState {struct {uint64_t uid=0;} prop;bool dungeon_membership=false,dungeon_instance_membership=false;action::BuildingState value;};}
std::map<std::string,uint64_t> ids;std::map<uint64_t,native::BuildingState> actors;
namespace remove_diagnostics {struct LocalTarget {bool normal_building=true;uint64_t uid=0;uintptr_t object=0;};
bool current_registry_world(const std::string&,uint64_t,uint64_t,std::string&){return true;}
bool resolve_record_target(const std::string&,const std::string& key,LocalTarget& out,std::string& e){if(drains)return fail(e,"stale alias after publication");auto it=ids.find(key);if(it==ids.end())return fail(e,"target absent");out={normal,it->second,uintptr_t(it->second+100)};return true;}}
namespace scenery_kiss {bool read_action_prop(uint64_t uid,native::BuildingState& out,std::string& e){++reads;if(late_fail&&drains)return fail(e,"late read failure");auto it=actors.find(uid);if(it==actors.end())return fail(e,"actor absent");out=it->second;return true;}}
namespace world_state {action::BuildingState building_state(const native::BuildingState& x){return x.value;}}
namespace terrain_scenery_diagnostic {struct Row {std::array<uint32_t,3> pose{};};}
namespace terrain_history_height {struct PublicationScope {bool contains(const terrain_scenery_diagnostic::Row&)const{return ::box_ok;}};
bool publication_scope(const json&,PublicationScope&,std::string&){return true;}}
uint32_t moved_building_ground(uintptr_t,uintptr_t,const std::array<float,3>& position,float& height){height=position[1]+(ground_ok?0.f:1.f);return 0;}
namespace terrain_settlement {bool enable(Runtime&,std::string&){return true;}bool run(Runtime&,const std::string&,bool,json& receipt,std::string& e){++drains;if(!drain_ok)return fail(e,"publication failure");for(auto& [id,a]:actors){(void)id;a.value.y+=1;if(alter_x)a.value.x+=1;}receipt={{"confirmed",true}};return true;}}
'''
main = r'''
void seed(action::Request& request,std::vector<SelectionBuildingSettlement>& inputs){ids.clear();actors.clear();drains=reads=0;scope_ok=drain_ok=ground_ok=normal=box_ok=true;alter_x=late_fail=changed_object=false;runtime.api.find=fake_find;request={};request.action=action::Action::MoveSelection;request.action_uuid="source";request.snapshot_uuid="snapshot";inputs.clear();
 for(unsigned i=0;i<2;++i){auto key="building"+std::to_string(i);action::BuildingState a;a.alive=true;a.type="SmallInn";a.variant="base";a.x=float(i);a.y=10;auto before=a;before.x-=1;request.selection_move.rows.push_back({key,before,a});inputs.push_back({key,100+i,a});ids[key]=100+i;native::BuildingState n;n.prop.uid=100+i;n.value=a;actors[100+i]=n;}}
int main(){action::Request request;std::vector<SelectionBuildingSettlement> input;std::vector<action::BuildingState> settled;json receipt;std::string e;unsigned cases=0;
 seed(request,input);if(!settle_selection_buildings({2,3},request,input,settled,receipt,e)||drains!=1||settled.size()!=2||settled[0].y!=11||settled[1].y!=11||receipt["buildings"].size()!=2)return 1;++cases;
 seed(request,input);input[1].uid=input[0].uid;e.clear();if(settle_selection_buildings({2,3},request,input,settled,receipt,e)||drains)return 2;++cases;
 seed(request,input);request.selection_move.rows[0].after.x+=1;e.clear();if(settle_selection_buildings({2,3},request,input,settled,receipt,e)||drains)return 3;++cases;
 seed(request,input);scope_ok=false;e.clear();if(settle_selection_buildings({2,3},request,input,settled,receipt,e)||drains||e!="scope mismatch")return 4;++cases;
 seed(request,input);actors[100].value.x+=1;e.clear();if(settle_selection_buildings({2,3},request,input,settled,receipt,e)||drains)return 5;++cases;
 seed(request,input);drain_ok=false;e.clear();if(settle_selection_buildings({2,3},request,input,settled,receipt,e)||drains!=1||e!="publication failure")return 6;++cases;
 seed(request,input);alter_x=true;e.clear();if(settle_selection_buildings({2,3},request,input,settled,receipt,e)||drains!=1||!settled.empty())return 7;++cases;
 seed(request,input);ground_ok=false;e.clear();if(settle_selection_buildings({2,3},request,input,settled,receipt,e)||drains!=1||!settled.empty())return 8;++cases;
 seed(request,input);late_fail=true;e.clear();if(settle_selection_buildings({2,3},request,input,settled,receipt,e)||drains!=1||e!="late read failure")return 9;++cases;
 seed(request,input);normal=false;e.clear();if(settle_selection_buildings({2,3},request,input,settled,receipt,e)||drains)return 10;++cases;
 seed(request,input);box_ok=false;e.clear();if(settle_selection_buildings({2,3},request,input,settled,receipt,e)||drains!=1||!settled.empty())return 11;++cases;
 seed(request,input);changed_object=true;e.clear();if(settle_selection_buildings({2,3},request,input,settled,receipt,e)||drains!=1||!settled.empty())return 12;++cases;
 seed(request,input);request.action=action::Action::UndoBuilding;request.target_action_uuid="source";request.canonical_record_key="selection:other";e.clear();if(settle_selection_buildings({2,3},request,input,settled,receipt,e)||drains)return 13;++cases;
 seed(request,input);request.action=action::Action::UndoBuilding;request.target_action_uuid="source";request.canonical_record_key="selection:source";e.clear();if(!settle_selection_buildings({2,3},request,input,settled,receipt,e)||drains!=1)return 14;++cases;
 std::cout<<cases<<" actual batch settlement cases passed\n";return 0;}
'''
(root / 'test_batch_settlement.cpp').write_text(prefix + validation + '\n' + batch + '\n' + main, encoding='utf-8')
print('actual settlement bodies extracted')
