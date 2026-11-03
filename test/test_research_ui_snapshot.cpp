#include "general_project_reader.h"
#include "general_project_codec.h"
#include "tutorial_development.h"
#include "typed_action/action_session.h"
#include <atomic>
#include <iostream>
#include <stdexcept>
namespace p=mt2mp::game::general_project;namespace a=mt2mp::action_session;namespace tutorial_development=mt2mp::game::tutorial_development;namespace tutorial_replication=mt2mp::game::tutorial_replication;
namespace {
int checks=0;void need(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
a::Context context{77,2,1,3,a::Role::Client,"10000000-0000-0000-0000-000000000001"};a::Binding binding{31,39};bool fixture_ordered=true;
struct Session:a::Session{bool current(a::Context& out,std::string&)override{out=context;return true;}}session;
}
namespace mt2mp::action_session{Session& selected_session(){return ::session;}}
namespace typed_action_runtime{bool network_binding_with_mode(const std::string&,a::Binding& out,std::string&,bool* mode){out=binding;*mode=fixture_ordered;return true;}}
namespace native_open_runtime{bool wrong_origin=false;bool read_current_world(tutorial_development::detail::Origin& out,std::string&){out.world_epoch=wrong_origin?32:31;out.generation=39;return true;}}
namespace test {
struct Runtime{bool ready=true;uintptr_t image=0x100000;}runtime;std::atomic<Runtime*> published{&runtime};
p::Local fixture_local;p::State fixture_state;bool snapshot_failed=false;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool snapshot(Runtime&,const a::Context&,const a::Binding&,p::Local& owner,p::State& value,a::Ledger&,std::string& e,bool){if(snapshot_failed)return fail(e,"original snapshot failure");owner=fixture_local;value=fixture_state;return true;}
template<class T>bool at(Runtime&,uintptr_t address,size_t offset,T& out,std::string& e){if(!fixture_local.tech_keys.contains("parties")||address!=fixture_local.tech_keys.at("parties"))return fail(e,"foreign actual Tech pointer");if(offset==0x1e8)out=T(400);else if(offset==0xb4)out=T(3);else return fail(e,"unexpected metadata read");return true;}
bool project_proof(const a::Context&,const a::Binding&,const p::Source& source,const p::State& value,p::Proof& out,std::string& e){return p::proof(source,value,std::string(64,'c'),[](const std::string& s){return std::string(64,s.empty()?'a':'b');},out,e);}
#include "research_ui_snapshot_actual.inc"
void reset(){runtime.ready=true;fixture_ordered=true;native_open_runtime::wrong_origin=false;snapshot_failed=false;fixture_state={};fixture_local={};
    p::Catalogue row;row.source={0,0,"parties",""};row.valid=row.visible=row.choose=row.afford=true;row.tech_flags={1,0,0};fixture_state.catalogue={row};fixture_state.techs={{"parties",{1,0,0}}};fixture_state.progress={row.source,std::bit_cast<uint32_t>(2.f),3,4,1,1};
    fixture_local.lounge=100;fixture_local.current=200;fixture_local.catalogue={300};fixture_local.tech_keys={{"parties",500}};fixture_local.player=600;fixture_local.inventory=700;fixture_local.slots=800;fixture_local.items[17]=900;
}
void cases(){std::string e;tutorial_development::StartingpointSnapshot out;reset();
    need(research_ui_snapshot("parties",out,e),e);need(out.tech==500&&out.record==400&&out.creation&&out.creation->source==300&&out.creation->max_workers==3,"owned selected Tech metadata differs");
    need(out.row&&out.row->project_present&&out.row->work==2.f&&out.row->bugs_total==4&&out.items[17]==900&&out.creation->current->address==200,"actual current project/inventory lost");
    need(out.admission==p::encode(p::codec::proof(out.admission)),"complete generic proof did not roundtrip");
    fixture_state.progress={};fixture_local.current=0;need(research_ui_snapshot("parties",out,e)&&!out.row->project_present&&!out.project,"absent project invented effects");
    reset();need(!research_ui_snapshot("dungeons",out,e)&&!out.row,"foreign key supplied a partial snapshot");
    reset();fixture_state.catalogue.push_back(fixture_state.catalogue.front());fixture_local.catalogue.push_back(301);need(!research_ui_snapshot("parties",out,e)&&e.find("unique")!=std::string::npos,"ambiguous selected Tech admitted");
    reset();fixture_ordered=false;need(!research_ui_snapshot("parties",out,e)&&!out.row,"legacy snapshot entered fixture_ordered adapter");
    reset();native_open_runtime::wrong_origin=true;need(!research_ui_snapshot("parties",out,e)&&!out.row,"changed native Origin admitted");
    reset();snapshot_failed=true;need(!research_ui_snapshot("parties",out,e)&&e=="original snapshot failure","original snapshot cause lost");
}
}
int main(){try{test::cases();std::cout<<checks<<" generic Research UI snapshot assertions passed\n";return 0;}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
