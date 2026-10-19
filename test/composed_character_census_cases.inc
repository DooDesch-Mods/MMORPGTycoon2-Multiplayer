namespace composed_character_fixture {
namespace action_session {struct Context {};struct Binding {};}
namespace character_definition_identity {struct Census {int value=0;};}
namespace character_membership_reader {struct Users {int value=0;};}
struct World {
    void* game=nullptr;void* mode=nullptr;void* state=nullptr;void* world=nullptr;void* map=nullptr;
    uint64_t generation=8,world_epoch=7;std::string uuid="world",sha256="sha";
};
inline int character_calls=0,world_calls=0,fail_character=0;
inline bool change_world=false,fail_pool=false,fail_registry=false;
inline bool fail(std::string& error,const std::string& cause){error=cause;return false;}
namespace npc_probe::character_actor_records {
inline bool read_current(const action_session::Context&,const action_session::Binding&,character_definition_identity::Census& definitions,character_membership_reader::Users& users,std::string& error){
    ++character_calls;if(character_calls==fail_character)return fail(error,"original Character current census refused");
    definitions.value=character_calls;users.value=character_calls;return true;
}}
namespace native_open_runtime {inline bool read_current_world(World& world,std::string&){world={};++world_calls;if(change_world&&world_calls==2)++world.generation;return true;}}
namespace boss_identity {
struct Scope {std::string snapshot;uint64_t epoch=0,generation=0;uintptr_t root=0,manager=0;bool operator==(const Scope&)const=default;};
struct Entry {Scope scope;uintptr_t pointer=0;uint64_t uid=0;bool retired=false;};
struct Registry {std::vector<std::pair<std::string,Entry>> rows;
    bool enumerate(const Scope&,std::vector<std::pair<std::string,Entry>>& out,std::string& error){out=rows;return !fail_registry||fail(error,"original Boss registry refused");}
};
inline Registry registry;
}
struct Member {uintptr_t pointer=0;uint64_t uid=0;};
struct Pool {boss_identity::Scope scope;struct Native {std::vector<Member> members;} native;};
struct State {World world;std::vector<Pool> pools;std::vector<std::pair<std::string,boss_identity::Entry>> live;};
inline std::vector<Pool> observed_pools;
inline bool pools(const World&,std::vector<Pool>& out,std::string& error){out=observed_pools;return !fail_pool||fail(error,"original complete Boss pool refused");}
#include "composed_character_bodies.inc"
namespace boss_current {
inline bool read_with_characters(const action_session::Context& context,const action_session::Binding& binding,State& bosses,
    character_definition_identity::Census& definitions,character_membership_reader::Users& users,std::string& error){
    return composed_character_fixture::read_with_characters(context,binding,bosses,definitions,users,error);
}}
struct Census {State bosses;character_definition_identity::Census definitions;character_membership_reader::Users users;};
#include "composed_character_edge.inc"
void reset(){character_calls=world_calls=fail_character=0;change_world=fail_pool=fail_registry=false;boss_identity::registry.rows.clear();observed_pools.clear();}
void checks(){
    std::string error;State state;character_definition_identity::Census definitions;character_membership_reader::Users users;Census out;
    reset();need(npc_probe::character_actor_records::read_current({}, {},definitions,users,error)&&read({}, {},state,error)&&character_calls==3,"old separate Character plus Boss path requires three complete Character admissions");
    reset();need(actual_intervention_edge({}, {},out,error)&&character_calls==2&&world_calls==2,"composed current census retains both Boss admissions without third Character traversal");
    need(out.definitions.value==1&&out.users.value==1,"Intervention publishes Character identities from initial Boss admission");
    reset();fail_character=1;need(!actual_intervention_edge({}, {},out,error)&&character_calls==1&&world_calls==0&&error=="original Character current census refused","initial full Character failure preserved");
    reset();fail_character=2;need(!actual_intervention_edge({}, {},out,error)&&character_calls==2&&out.definitions.value==0&&out.users.value==0&&error=="original Character current census refused","final Character failure refuses all composed output");
    reset();change_world=true;error.clear();need(!actual_intervention_edge({}, {},out,error)&&error=="Boss current bound NativeOpen changed before readback","final actual World drift remains rejected");
    reset();fail_pool=true;need(!actual_intervention_edge({}, {},out,error)&&character_calls==1&&error=="original complete Boss pool refused","complete pool failure stays visible before final admission");
    reset();fail_registry=true;need(!actual_intervention_edge({}, {},out,error)&&error=="original Boss registry refused","canonical registry failure stays visible");
    for(int fault=0;fault<5;++fault){reset();error.clear();Pool pool;pool.scope={"world",7,8,0x1c0,0x100};pool.native.members={{0x200,0x123456789abcdef0}};
        boss_identity::Entry entry{pool.scope,0x200,0x123456789abcdef0,false};boss_identity::registry.rows={{"boss",entry}};
        if(fault==1)++boss_identity::registry.rows.front().second.uid;
        if(fault==2)++boss_identity::registry.rows.front().second.scope.manager;
        if(fault==3)pool.native.members.clear();
        if(fault==4)pool.native.members.push_back(pool.native.members.front());
        observed_pools={pool};const bool result=actual_intervention_edge({}, {},out,error);
        need(result==(fault==0),"composed census preserves exact full UID/scope/completeness/uniqueness guards");
        if(fault==0)need(out.bosses.live.size()==1&&out.bosses.live.front().first=="boss","composed census returns original canonical Boss key");
        else need(!error.empty()&&out.definitions.value==0&&out.users.value==0,"canonical Boss mismatch cannot publish partial Character output");
    }
    reset();need(actual_intervention_edge({}, {},out,error)&&actual_intervention_edge({}, {},out,error)&&character_calls==4,"each call captures both fresh Character censuses");
}
}
