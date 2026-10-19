#define main scenery_protocol_fixture_main
#include "test_scenery_stroke_protocol.cpp"
#undef main
#include "scenery_stroke_state.h"
#include "scenery_stroke_native.h"
#include "multiplayer_world/world_state.h"
namespace state=mt2mp::game::scenery_stroke_state;
namespace original=mt2mp::game::scenery_stroke_native;
namespace world=mt2mp::world_state;
namespace {
struct NativePort {
    std::string fail_at;bool cleanup_failure=false;unsigned calls=0,finishes=0,destructors=0,restores=0;
    bool step(const char* name,std::string& e){if(fail_at==name){e=std::string("first ")+name+" cause";return false;}return true;}
    bool ready(const SceneryStrokeIntent&,std::string& e){return step("ready",e);}
    bool prepare(const SceneryStrokeIntent&,std::string& e){return step("prepare",e);}
    bool enter_context(bool& entered,std::string& e){entered=true;return step("context",e);}
    bool begin_edit(bool& opened,std::string& e){opened=true;return step("edit",e);}
    bool before_stamp(size_t,std::string& e){return step("before stamp",e);}
    bool add_paint(size_t,uintptr_t& graph,std::string& e){++calls;graph=1;return step("paint",e);}
    bool after_stamp(size_t,uintptr_t,std::string& e){return step("after stamp",e);}
    bool finish(uintptr_t&,bool& entered,bool& returned,std::string& e){++finishes;entered=true;returned=step("finish",e);return returned;}
    bool readback(uintptr_t,std::string& e){return step("readback",e);}
    bool restore_context(std::string& e){++restores;if(cleanup_failure){e="restore cause";return false;}return true;}
    bool finish_empty(std::string&){++finishes;return true;}
    bool restore_selection(std::string&){return true;}
    bool destroy_cursor(std::string&){++destructors;return true;}
    bool release_resources(std::string&){return true;}
};
bool hash(const std::string& source,std::string& out,std::string&){out=std::string(64,source.empty()?'a':'b');return true;}
}
int main()try{
    unsigned count=0;std::string e;auto packet=message(false);const auto& effect=*packet.evidence.scenery_stroke_result;
    state::Objects before,after;before.emplace(old(1),effect.removed[0]);before.emplace(effect.changed[0].before.key,effect.changed[0].before);
    after.emplace(effect.changed[0].after.key,effect.changed[0].after);auto untouched=object(old(3));before.emplace(untouched.key,untouched);after.emplace(untouched.key,untouched);
    SceneryStrokeEffect actual;need(state::difference(before,after,{},hash,actual,e)&&actual.removed==effect.removed&&actual.changed==effect.changed,e);++count;
    auto born=object("action:"+fixture_snapshot);after.emplace(born.key,born);
    need(!state::difference(before,after,{},hash,actual,e),"unobserved native creation accepted");++count;
    need(state::difference(before,after,{"action:"+fixture_action,born.key},hash,actual,e)&&actual.created==std::vector{born},e);++count;
    need(!state::difference(before,after,{born.key,born.key},hash,actual,e),"birth order duplicate accepted");++count;
    auto invalid=before;invalid.at(old(3)).family=DuplicateFamily::Building;
    need(!state::difference(invalid,{}, {},hash,actual,e),"Scenery diff removed a Building");++count;
    const auto brush=message().request.scenery_stroke;NativePort port;original::Receipt receipt;
    need(original::execute(port,brush,receipt,e)&&receipt.graph_committed&&port.calls==1&&port.finishes==1&&port.destructors==1&&port.restores==1,e);++count;
    need(!original::execute(port,brush,receipt,e)&&port.calls==1,"uncertain/native stroke was replayed");++count;
    for(const std::string step:{"prepare","context","edit","before stamp","paint","after stamp","finish","readback"}){
        port={};port.fail_at=step;receipt={};e.clear();need(!original::execute(port,brush,receipt,e)&&e=="first "+step+" cause",e);
        if(step=="paint"||step=="after stamp"||step=="finish")need(receipt.attempted&&port.destructors==0,"uncertain original graph owner was destroyed");
        else need(port.destructors==1,"safe native cursor cleanup was skipped");
        if(step=="edit"||step=="before stamp")need(port.finishes==1,"original empty edit Finish was skipped");count+=2;
    }
    port={};port.fail_at="paint";port.cleanup_failure=true;receipt={};e.clear();
    need(!original::execute(port,brush,receipt,e)&&e=="first paint cause; cleanup: native context restore: restore cause",e);++count;
    world::Table table;const world::Origin origin{fixture_snapshot,std::string(64,'a'),4,5};BuildingState building;building.type="Trees";building.variant="oak";building.alive=true;
    need(table.bind_snapshot(origin,{{old(1),old(1),building},{old(2),old(2),building}},0,e),e);++count;
    const world::LocalBinding created{origin,born.key,67,building};
    need(table.commit_scenery_stroke(origin,1,fixture_action,{old(1)},{created},e)==world::Admission::New&&table.revision()==1&&!table.rows().at(old(1)).state.alive&&table.rows().at(born.key).state.alive,e);++count;
    const auto prior=table.rows();auto bad_created=created;bad_created.object_key="action:"+fixture_action;bad_created.local_uid=0;
    need(table.commit_scenery_stroke(origin,2,fixture_action,{old(2)},{bad_created},e)==world::Admission::Rejected&&table.rows()==prior&&table.revision()==1,"mixed publication exposed partial removal");++count;
    need(table.commit_scenery_stroke(origin,2,fixture_action,{old(1)},{},e)==world::Admission::Rejected&&table.rows()==prior,"retired source was removed twice");++count;
    need(table.commit_scenery_stroke(origin,2,fixture_action,{old(2)},{},e)==world::Admission::New&&!table.rows().at(old(2)).state.alive,e);++count;
    const world::LocalBinding restored{origin,old(1),68,building};
    need(table.commit_scenery_history(origin,3,fixture_snapshot,{born.key},{restored},e)==world::Admission::New&&table.rows().at(old(1)).state.alive&&
         !table.rows().at(born.key).state.alive&&table.revision()==3,e);++count;
    const auto history_rows=table.rows();auto wrong=restored;wrong.object_key=old(2);wrong.state.x=12;
    need(table.commit_scenery_history(origin,4,fixture_snapshot,{old(1)},{wrong},e)==world::Admission::Rejected&&table.rows()==history_rows&&table.revision()==3,
         "mixed Scenery history exposed a removal before invalid restoration");++count;
    need(table.commit_scenery_history(origin,4,fixture_snapshot,{old(1)},{restored},e)==world::Admission::Rejected&&table.rows()==history_rows,
         "mixed Scenery history used the same live key for remove and restore");++count;
    need(table.commit_scenery_stroke(origin,4,fixture_snapshot,{old(1)},{created},e)==world::Admission::Rejected&&table.rows()==history_rows,
         "forward stroke silently restored an old action key");++count;
    need(table.commit_scenery_history(origin,4,fixture_snapshot,{old(1)},{created},e)==world::Admission::New&&table.rows().at(born.key).state.alive,e);++count;
    std::cout<<count<<" Scenery complete diff/native ownership/atomic publication cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
