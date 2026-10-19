#include "party_live.h"
#include "dungeon_instance_lifecycle.h"
#include <iostream>
#include <stdexcept>

namespace typed=mt2mp::action_session;
namespace live=mt2mp::game::party_live;
using World=mt2mp::game::dungeon_scene_identity_runtime::World;
struct ActorState {
    typed::Context context;typed::Binding binding;
    uint64_t epoch=0,origin=0;uintptr_t root=0;std::string native_snapshot;
};
struct SceneState {live::Raw baseline;};
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
namespace life=mt2mp::game::dungeon_instance_lifecycle;
#include "scope_bodies.inc"

namespace {
unsigned scope_checks=0;
void check(bool result,const char* cause){++scope_checks;if(!result)throw std::runtime_error(cause);}
const std::string host_native="113d3d9d-8848-49a7-b0c3-c23124fa283b";
const std::string shared="8aa73b94-2fae-4293-9f6c-705b0aa015b8";
typed::Context context(typed::Role role){typed::Context value;value.session=1;value.host_peer=1;
    value.local_peer=role==typed::Role::Host?1:2;value.peer_mask=3;value.snapshot_uuid=shared;value.role=role;return value;}
World world(const std::string& uuid){World value;value.uuid=uuid;value.world_epoch=1;value.generation=1;value.map=reinterpret_cast<void*>(0xe40);return value;}
live::Raw raw(const World& value){live::Raw result;result.world=value;result.scope={value.uuid,value.world_epoch,value.generation,uintptr_t(value.map)+0x1c0};return result;}
void diagnostic(const std::string& error,const std::string& actual){
    check(error.find("session_snapshot="+shared)!=std::string::npos,"Missing shared Session UUID diagnostic");
    check(error.find("native_snapshot="+actual)!=std::string::npos,"Missing actual native UUID diagnostic");
    check(error.find("binding_epoch=1")!=std::string::npos&&error.find("binding_generation=1")!=std::string::npos,"Missing required native lifetime diagnostic");
}
}
int main(){try{
    const typed::Binding binding{1,1};std::string error;
    for(const auto role:{typed::Role::Host,typed::Role::Client}){
        const auto ctx=context(role);const auto native=world(role==typed::Role::Host?host_native:shared);
        const auto census=raw(native);SceneState scene{census};ActorState actor;
        check(scope_cases::party_native(census,ctx,binding,error),"Party Native rejected a real shared/local UUID pair");
        check(scope_cases::party_frame(ctx,ctx,binding,binding,native,census,error),"Party Frame rejected a real shared/local UUID pair");
        check(scope_cases::actor_bind(actor,ctx,binding,native,7,9,error),"Actor bind rejected a real shared/local UUID pair");
        check(actor.native_snapshot==native.uuid&&actor.context==ctx&&actor.binding==binding&&actor.root==0x1000&&actor.epoch==7&&actor.origin==9,"Actor did not cache actual native identity separately");
        check(scope_cases::actor_current(actor,native,error),"Actor callback rejected bound native world");
        check(scope_cases::dungeon_bind(ctx,binding,native,error),"Instance bind rejected a real shared/local UUID pair");
        check(scope_cases::scene_bind(ctx,binding,&scene,error),"Scene bind rejected a real shared/local UUID pair");
    }
    const auto ctx=context(typed::Role::Host);const auto native=world(host_native);const auto census=raw(native);
    for(unsigned field=0;field<8;++field){auto changed=census;
        if(field==0)changed.scope.snapshot=shared;
        if(field==1)++changed.scope.epoch;
        if(field==2)++changed.scope.generation;
        if(field==3)++changed.scope.root;
        if(field==4)changed.world.uuid=shared;
        if(field==5)++changed.world.world_epoch;
        if(field==6)++changed.world.generation;
        if(field==7)changed.world.map=reinterpret_cast<void*>(0xe48);
        error.clear();check(!scope_cases::party_native(changed,ctx,binding,error),"Party Native admitted a foreign scope");
        diagnostic(error,changed.world.uuid);
    }
    ActorState actor;check(scope_cases::actor_bind(actor,ctx,binding,native,7,9,error),"Actor setup failed");
    for(unsigned field=0;field<4;++field){auto changed=native;
        if(field==0)changed.uuid=shared;
        if(field==1)++changed.world_epoch;
        if(field==2)++changed.generation;
        if(field==3)changed.map=reinterpret_cast<void*>(0xe48);
        error.clear();check(!scope_cases::party_frame(ctx,ctx,binding,binding,changed,census,error),"Party Frame admitted replacement native world");
        diagnostic(error,changed.uuid);
        check(error.find("bound_native_snapshot="+host_native)!=std::string::npos,"Party Frame omitted bound native UUID");
        error.clear();check(!scope_cases::actor_current(actor,changed,error),"Actor admitted replacement native world");
        if(field<3){diagnostic(error,changed.uuid);check(error.find("bound_native_snapshot="+host_native)!=std::string::npos,"Actor omitted bound native UUID");}
        else check(error.find("cached current Root differs")!=std::string::npos,"Actor root failure cause changed");
    }
    auto foreign_context=ctx;foreign_context.snapshot_uuid=host_native;
    check(!scope_cases::party_frame(foreign_context,ctx,binding,binding,native,census,error),"Shared Session UUID guard was removed");
    foreign_context=ctx;++foreign_context.session;
    check(!scope_cases::party_frame(foreign_context,ctx,binding,binding,native,census,error),"Session identity guard was removed");
    auto foreign_binding=binding;++foreign_binding.native_generation;
    check(!scope_cases::party_frame(ctx,ctx,foreign_binding,binding,native,census,error),"Frame Binding identity guard was removed");
    for(unsigned field=0;field<3;++field){auto changed=native;
        if(field==0)changed.uuid.clear();
        if(field==1)++changed.world_epoch;
        if(field==2)++changed.generation;
        ActorState rejected;SceneState scene{raw(changed)};
        error.clear();check(!scope_cases::actor_bind(rejected,ctx,binding,changed,7,9,error),"Actor bind accepted missing or foreign native lifetime");diagnostic(error,changed.uuid);
        check(rejected.native_snapshot.empty()&&rejected.root==0,"Failed Actor bind published native identity");
        error.clear();check(!scope_cases::dungeon_bind(ctx,binding,changed,error),"Instance bind accepted missing or foreign native lifetime");diagnostic(error,changed.uuid);
        error.clear();check(!scope_cases::scene_bind(ctx,binding,&scene,error),"Scene bind accepted missing or foreign native lifetime");diagnostic(error,changed.uuid);
    }
    std::cout<<"PASS "<<scope_checks<<" Party Frame native scope checks\n";return 0;
}catch(const std::exception& exception){std::cerr<<"FAIL: "<<exception.what()<<'\n';return 1;}}
