#include "dungeon_building_fixture.h"
#include "typed_action/ordered_building_family.h"
#include "dungeon_scene_key.h"
#include <iostream>
#include <memory>
#include <set>
namespace action_session=mt2mp::action_session;
using json=nlohmann::json;
namespace fixture {
namespace dto=dungeon_building_fixture;
action_session::Context current_context;
action_session::Binding current_binding{1,2};
action_session::Request current_request;
bool ordered=true,valid_scope=true;
unsigned publications=0,checks=0;
json controller;
void need(bool okay,const std::string& error){++checks;if(!okay)throw std::runtime_error(error);}
}
unsigned GetCurrentThreadId(){return 7;}
namespace mt2mp::action_session::tick_commands::authority {
bool active(){return fixture::ordered;}
bool matches(const Context& c,const Binding& b,const Request& r,std::string& e){
    if(fixture::valid_scope&&c==fixture::current_context&&b==fixture::current_binding&&r==fixture::current_request)return true;
    e="Consumed scene command differs";return false;
}
}
namespace typed_action_runtime {json controller_status(){return fixture::controller;}}
namespace mt2mp::game::dungeon_scene_identity_runtime {struct Census {};}
namespace mt2mp::game::scene_clone {
namespace gizmo=dungeon_scene_identity_runtime;
struct Raw {unsigned marker=0;};
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
bool current(const action_session::Context& c,const action_session::Binding& b,std::string& error){
    return (c==fixture::current_context&&b==fixture::current_binding)||fail(error,"Current scene Context/Binding changed");
}
class Runtime {
public:
    struct State {
        struct TemplateHistory {action_session::Action kind;std::string exterior;Raw before,after;uintptr_t held_node=0;};
        bool prepared=true,locked=false;unsigned thread=7;
        action_session::Context context;action_session::Binding binding;Raw baseline;
        std::set<std::string> operations;
        std::map<std::string,TemplateHistory> template_history;
    };
    std::unique_ptr<State> state_=std::make_unique<State>();
    bool finish_fixture(const std::string& operation,Mapping& out,bool& attempted){
        ++fixture::publications;attempted=true;out.operation=operation;
        ++state_->baseline.marker;state_->operations.insert(operation);return true;
    }
    bool confirm_impl(const action_session::Context&,const action_session::Binding&,const std::string&,
        const Mapping*,Mapping&,gizmo::Census&,bool&,std::string&,const std::string&,const Raw*,uintptr_t,bool ordered_template=false);
    bool confirm_template(const action_session::Context&,const action_session::Binding&,const action_session::Request&,
        const Mapping*,Mapping&,gizmo::Census&,bool&,std::string&,uintptr_t held_node=0);
};
#include "dungeon_scene_guard_actual.inc"
#include "dungeon_scene_template_actual.inc"
}
namespace scene=mt2mp::game::scene_clone;
scene::Runtime reset(bool client){
    fixture::current_context={7,client?uint64_t(2):uint64_t(1),1,3,client?action_session::Role::Client:action_session::Role::Host,fixture::dto::snapshot_id};
    fixture::current_request=fixture::dto::request(action_session::Action::PlaceBuilding,true);
    fixture::ordered=fixture::valid_scope=true;fixture::publications=0;
    fixture::controller={{"ordered",true},{"started",true},{"locked",false},{"pending",true},{"action_uuid",fixture::current_request.action_uuid}};
    scene::Runtime runtime;runtime.state_->context=fixture::current_context;runtime.state_->binding=fixture::current_binding;return runtime;
}
int main()try{
    for(int edit=0;edit<4;++edit)for(bool client:{false,true}){
        const bool portal=edit!=0;
        auto runtime=reset(client);scene::Mapping mapping;mt2mp::game::dungeon_scene_identity_runtime::Census admitted;bool attempted=false;std::string error;
        if(portal)fixture::current_request=fixture::dto::request(edit==1?action_session::Action::PlaceBuilding:
            edit==2?action_session::Action::MoveBuilding:action_session::Action::RemoveSnapshotBuilding,false);
        fixture::need(runtime.confirm_template(fixture::current_context,fixture::current_binding,fixture::current_request,nullptr,mapping,admitted,attempted,error),error);
        fixture::need(attempted&&fixture::publications==1&&runtime.state_->baseline.marker==1&&
            (portal?runtime.state_->template_history.empty():
            (runtime.state_->template_history.size()==1&&runtime.state_->template_history.begin()->second.before.marker==0&&runtime.state_->template_history.begin()->second.after.marker==1)),
            "Ordered template lost exact original before/after history or independent publication");
        fixture::need(!runtime.confirm_template(fixture::current_context,fixture::current_binding,fixture::current_request,nullptr,mapping,admitted,attempted,error)&&fixture::publications==1,"Template replay republished Scene aliases");
    }
    for(int mode=0;mode<11;++mode){
        auto runtime=reset(true);scene::Mapping mapping,host;mt2mp::game::dungeon_scene_identity_runtime::Census admitted;bool attempted=false;std::string error;
        if(mode==0)fixture::valid_scope=false;if(mode==1)fixture::ordered=false;if(mode==2)fixture::current_request.building.type="DungeonPortal";
        if(mode==3)fixture::current_request.dungeon_scene="foreign/template";if(mode==4)fixture::controller["pending"]=false;
        if(mode==5)runtime.state_->prepared=false;if(mode==6)runtime.state_->locked=true;
        if(mode>=8){fixture::current_request=fixture::dto::request(action_session::Action::PlaceBuilding,false);
            if(mode==8)fixture::current_request.dungeon_scene=std::string("action:")+fixture::dto::source_id+"/instance/0";
            if(mode==9)fixture::current_request.action=action_session::Action::UndoBuilding;
            if(mode==10)fixture::current_request.building.type="Inn";}
        fixture::need(!runtime.confirm_template(fixture::current_context,fixture::current_binding,fixture::current_request,mode==7?&host:nullptr,mapping,admitted,attempted,error)&&
            fixture::publications==0&&!attempted&&!error.empty(),"Unscoped/foreign/unprepared/Host-mapping Scene publication accepted");
    }
    for(bool client:{false,true})for(bool host_mapping:{false,true}){
        auto runtime=reset(client);fixture::ordered=false;fixture::controller["ordered"]=false;
        scene::Mapping mapping,host;mt2mp::game::dungeon_scene_identity_runtime::Census admitted;bool attempted=false;std::string error;
        const bool okay=runtime.confirm_impl(fixture::current_context,fixture::current_binding,fixture::current_request.action_uuid,host_mapping?&host:nullptr,mapping,admitted,attempted,error,{},nullptr,0);
        fixture::need(okay==(client==host_mapping),"Legacy generic Scene authority guard changed");
    }
    std::cout<<fixture::checks<<" actual Scene template/confirmation admission checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
