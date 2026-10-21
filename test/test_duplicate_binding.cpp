#include "multiplayer_world/world_state.h"
#include <iostream>
#include <limits>
#include <stdexcept>
namespace w=mt2mp::world_state;
void need(bool b,const std::string& e){if(!b)throw std::runtime_error(e);}
#include "dungeon_building_table_cases.inc"
int main()try{
    const std::string id="11111111-1111-4111-8111-111111111111";const w::Origin origin{"22222222-2222-4222-8222-222222222222",std::string(64,'a'),3,4};
    w::Building state;state.type="SmallInn";state.variant="Original";state.alive=true;state.rotation={0,0,0,1};state.scale={1,1,1};
    const std::string first="action:"+id,second="action:33333333-3333-4333-8333-333333333333";
    std::vector<w::LocalBinding> rows{{origin,first,100,state},{origin,second,200,state}};
    unsigned checks=0;std::string e;w::Table table;need(table.bind_snapshot(origin,{},0,e),e);
    auto rejected=[&](auto change){w::Table t;need(t.bind_snapshot(origin,{},0,e),e);auto values=rows;change(values);
        need(t.commit_creates(origin,1,id,values,e)==w::Admission::Rejected&&t.rows().empty()&&t.revision()==0,"invalid batch partially committed");++checks;};
    rejected([](auto& values){values.back().local_uid=values.front().local_uid;});
    rejected([](auto& values){values.back().object_key=values.front().object_key;});
    rejected([](auto& values){values.back().origin.generation++;});
    rejected([](auto& values){values.back().state.alive=false;});
    rejected([](auto& values){values.back().local_uid=0;});
    rejected([](auto& values){values.back().state.type.clear();});
    rejected([](auto& values){values.back().object_key=std::string(64,'a')+":world:building:3";});
    need(table.commit_creates(origin,1,id,rows,e)==w::Admission::New&&table.rows().size()==2&&table.revision()==1,e);++checks;
    const auto stable=table.rows();need(table.commit_creates(origin,2,id,rows,e)==w::Admission::Rejected&&table.rows()==stable&&table.revision()==1,"create replay advanced revision");++checks;
    auto foreign=origin;foreign.generation++;need(table.commit_creates(foreign,2,id,rows,e)==w::Admission::Rejected&&table.rows()==stable,"foreign origin changed shared table");++checks;
    need(table.commit_creates(origin,1,id,{},e)==w::Admission::Rejected&&table.rows()==stable,"empty/repeated revision committed");++checks;
    const std::string undo="44444444-4444-4444-8444-444444444444",redo="55555555-5555-4555-8555-555555555555";
    need(table.retire_native_effects(origin,2,undo,{first,second},e),e);
    const auto tombstones=table.rows();
    auto bad_restore=[&](auto change){auto values=rows;change(values);
        need(!table.restore_native_buildings(origin,3,redo,values,e)&&table.rows()==tombstones&&table.revision()==2,"restore batch published partial state");++checks;};
    bad_restore([](auto& values){values.back().local_uid=values.front().local_uid;});
    bad_restore([](auto& values){values.back().object_key=values.front().object_key;});
    bad_restore([](auto& values){values.back().origin.generation++;});
    bad_restore([](auto& values){values.back().state.alive=false;});
    bad_restore([](auto& values){values.back().state.x=1;});
    bad_restore([](auto& values){values.back().state.variant="other";});
    bad_restore([](auto& values){values.back().local_uid=0;});
    bad_restore([](auto& values){values.back().object_key="action:66666666-6666-4666-8666-666666666666";});
    bad_restore([](auto& values){values.clear();});
    need(!table.restore_native_buildings(foreign,3,redo,rows,e)&&table.rows()==tombstones,"foreign restore origin accepted");++checks;
    need(!table.restore_native_buildings(origin,2,redo,rows,e)&&table.rows()==tombstones,"stale restore revision accepted");++checks;
    need(table.restore_native_buildings(origin,3,redo,rows,e)&&table.rows()==stable&&table.revision()==3,e);++checks;
    need(!table.restore_native_buildings(origin,4,redo,rows,e)&&table.rows()==stable&&table.revision()==3,"restoration replay advanced one graph revision");++checks;
    checks+=dungeon_building_table_cases();
    std::cout<<checks<<" actual atomic create-batch/full binding/no-partial cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
