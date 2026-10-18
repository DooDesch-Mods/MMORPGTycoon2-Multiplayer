#pragma once
#include "reference_map.h"
#include "append_calls.h"
#include "npc_probe.h"
#include "npc_census.h"
#include "npc_host_state_binding.h"
#include "region_saved_identity.h"
#include "native_open_runtime.h"
#include "session_provider.h"
#include "mt2mp/net/snapshot_sync.h"
#include <algorithm>
#include <map>
#include <set>
#include <Windows.h>

namespace mt2mp::game::subscriber_npc_references {
namespace ref=mt2mp::subscriber_lifecycle::first_append;
using World=building_mixed_identity::opened_file::CurrentWorld;
// Shared keys are collected before the drained SaveScope. Its native callbacks
// only use these local pins with fresh UID, Region and manager readbacks.
class Pins final:public ref::NpcReferences {
    struct QuestPin {uintptr_t object=0;mutable uintptr_t proxy=0;};
    struct Pin {
        uintptr_t object=0,region=0,manager=0;
        uint64_t uid=0;
        uintptr_t quest_manager=0;
        std::map<int32_t,QuestPin> quests;
        int32_t saved_region=-1,type=-1,character_slot=-1,character_type=-1;
    };
    class Memory final:public npc_census::Memory {
    public:
        bool copy(uintptr_t address,void* target,size_t size,std::string& e)override {
            if(!address||address>UINTPTR_MAX-size){e="Subscriber NPC reference memory extent invalid";return false;}
            SIZE_T copied=0;
            if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),target,size,&copied)||copied!=size){
                e="Subscriber NPC reference memory read failed error="+std::to_string(GetLastError());return false;
            }
            return true;
        }
    };
    uintptr_t image_=0;uint32_t thread_=0;
    World world_;
    action_session::Context context_;
    uint64_t common_epoch_=0;
    std::map<std::string,Pin> pins_;
    std::set<std::string> used_;
    std::set<std::pair<std::string,int32_t>> used_quests_;
    static bool fail(std::string& e,const char* cause){e=cause;return false;}
    bool current(std::string& e)const {
        World actual;action_session::Context context;
        if(!image_||GetCurrentThreadId()!=thread_||reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))!=image_)
            return fail(e,"Subscriber NPC reference image/Mainthread differs");
        if(!native_open_runtime::read_current_world(actual,e)||!action_session::selected_session().current(context,e))return false;
        return (context==context_&&net::current_snapshot_id()==context_.snapshot_uuid&&net::current_world_epoch()==common_epoch_&&
            actual.uuid==world_.uuid&&actual.sha256==world_.sha256&&actual.bytes==world_.bytes&&
            actual.world_epoch==world_.world_epoch&&actual.generation==world_.generation&&actual.game==world_.game&&
            actual.mode==world_.mode&&actual.world==world_.world&&actual.map==world_.map&&actual.state==world_.state)||
            fail(e,"Subscriber NPC reference current shared/local owner differs");
    }
    bool inspect(const Pin& pin,npc_census::State& census,npc_census::Row& row,std::string& e)const {
        Memory memory;uintptr_t registered=0,region=0;
        if(!current(e)||!ref::registry_lookup(image_,pin.uid,registered,e))return false;
        if(registered!=pin.object)return fail(e,"Subscriber NPC reference UID receiver changed");
        const auto read=[&](uintptr_t p,size_t offset,auto& value,std::string& error){return npc_census::field(memory,p,offset,value,error);};
        const auto map=reinterpret_cast<uintptr_t>(world_.map);
        if(!region_saved_identity::resolve(image_,map,map+0x1c0,pin.saved_region,pin.region,region,read,e)||
           !npc_census::read(memory,image_,region,census,e))return false;
        const auto found=std::find_if(census.rows.begin(),census.rows.end(),[&](const auto& value){return value.local_pointer==pin.object&&value.local_uid==pin.uid;});
        if(found==census.rows.end())return fail(e,"Subscriber NPC reference absent from current Region manager");
        row=*found;return current(e);
    }
    bool fresh(const Pin& pin,std::string& e)const {
        npc_census::State census;npc_census::Row row;
        if(!inspect(pin,census,row,e))return false;
        return (census.manager==pin.manager&&row.type==pin.type&&row.character_slot==pin.character_slot&&row.character_type==pin.character_type)||
            fail(e,"Subscriber NPC reference manager/type/CharacterType changed");
    }
    bool quest_proxy(uintptr_t object,uintptr_t& proxy,int32_t& refs,std::string& e)const {
        refs=0;Memory memory;uintptr_t ap=0,target=0;
        if(!npc_census::field(memory,object,0,ap,e)||ap!=image_+0x174ac10)
            return fail(e,e.empty()?"Subscriber Quest reference class differs":e.c_str());
        if(!npc_census::field(memory,object,0x20,proxy,e))return false;
        if(!proxy)return true;
        if(!npc_census::field(memory,proxy,0,target,e)||!npc_census::field(memory,proxy,8,refs,e))return false;
        if(object>UINTPTR_MAX-0x18||target!=object+0x18){
            e="Subscriber Quest reference weak target differs target_present="+std::to_string(target!=0)+
                " target_matches_owner=0 refs="+std::to_string(refs);return false;
        }
        if(refs<0){e="Subscriber Quest reference weak refcount invalid actual="+std::to_string(refs)+" required>=0";return false;}
        return true;
    }
    bool fresh_quest(const Pin& pin,int32_t sequence,const QuestPin& quest,std::string& e,bool require_reference=false)const {
        if(!fresh(pin,e))return false;
        Memory memory;std::vector<std::pair<int32_t,uintptr_t>> actual;uintptr_t manager=0;
        if(!npc_census::field(memory,image_,0x13df410,manager,e))return false;
        if(manager!=pin.quest_manager)return fail(e,"Subscriber Quest reference manager owner changed");
        if(!npc_probe::action_runtime::host_binding::quest_owners(memory,image_,pin.object,
            int32_t(pin.quests.size()),actual,e))return false;
        for(const auto& [id,object]:actual){const auto found=pin.quests.find(id);
            if(found==pin.quests.end()||found->second.object!=object)
                return fail(e,"Subscriber Quest reference sequence membership changed");
        }
        const auto selected=std::find(actual.begin(),actual.end(),std::pair{sequence,quest.object});
        if(selected==actual.end())return fail(e,"Subscriber Quest reference absent from current NPC list");
        uintptr_t proxy=0;int32_t refs=0;
        if(!quest_proxy(quest.object,proxy,refs,e))return false;
        if(quest.proxy&&proxy!=quest.proxy)return fail(e,"Subscriber Quest reference live weak proxy changed or absent");
        if(require_reference&&(!proxy||refs<=0)){
            e="Subscriber Quest reference selected weak holder absent refs="+std::to_string(refs)+" required>0";return false;
        }
        // Repeat owner membership after reading the selected weak target.
        std::vector<std::pair<int32_t,uintptr_t>> after;
        if(!npc_probe::action_runtime::host_binding::quest_owners(memory,image_,pin.object,
            int32_t(pin.quests.size()),after,e))return false;
        uintptr_t final_proxy=0,final_manager=0;int32_t final_refs=0;
        if(!quest_proxy(quest.object,final_proxy,final_refs,e)||!npc_census::field(memory,image_,0x13df410,final_manager,e))return false;
        if(actual!=after||final_proxy!=proxy||final_manager!=pin.quest_manager)
            return fail(e,"Subscriber Quest reference owner membership changed during read");
        if(require_reference&&(!final_proxy||final_refs<=0)){
            e="Subscriber Quest reference selected weak holder absent on final read refs="+std::to_string(final_refs)+" required>0";return false;
        }
        if(!fresh(pin,e))return false;
        // Original Weak Resolve allocates the first proxy lazily. Once observed,
        // that live target retains the same proxy even when its refcount is zero.
        if(!quest.proxy)quest.proxy=proxy;
        return true;
    }
public:
    bool prepare(uintptr_t image,const World& world,std::string& e){
        pins_.clear();used_.clear();used_quests_.clear();image_=image;thread_=GetCurrentThreadId();world_=world;common_epoch_=net::current_world_epoch();
        // Standalone captures without shared bindings retain the existing kinds.
        // An NPC reference still requires a canonical key and fails in identify.
        if(net::current_snapshot_id().empty())return true;
        if(!action_session::selected_session().current(context_,e)||!current(e))return false;
        std::vector<npc_probe::action_runtime::StateOwner> owners;
        if(!npc_probe::action_runtime::state_owners(context_,{world.world_epoch,world.generation},owners,e))return false;
        Memory memory;std::set<uintptr_t> objects,quest_objects;std::set<uint64_t> uids;
        for(const auto& owner:owners){
            Pin pin;pin.object=owner.npc;
            if(!ref::valid_npc_key(owner.key)||!pin.object||pins_.contains(owner.key)||!objects.insert(pin.object).second)
                return fail(e,"Subscriber NPC reference shared key/receiver is not unique");
            if(!npc_census::field(memory,pin.object,0x18,pin.uid,e)||!npc_census::field(memory,pin.object,0x178,pin.region,e)||
               !npc_census::field(memory,pin.region,0x834,pin.saved_region,e))return false;
            if(!pin.uid||!uids.insert(pin.uid).second)return fail(e,"Subscriber NPC reference local UID is not unique");
            npc_census::State census;npc_census::Row row;
            if(!inspect(pin,census,row,e))return false;
            pin.manager=census.manager;pin.type=row.type;pin.character_slot=row.character_slot;pin.character_type=row.character_type;
            if(owner.quests.size()>100000||owner.quests.size()!=size_t(row.quest_count))
                return fail(e,"Subscriber Quest reference prepared owner count differs");
            for(const auto& [sequence,quest]:owner.quests){
                QuestPin q;q.object=quest;
                if(sequence<0||!quest||!quest_objects.insert(quest).second||pin.quests.contains(sequence))
                    return fail(e,"Subscriber Quest reference prepared identity duplicated/invalid");
                int32_t refs=0;if(!quest_proxy(quest,q.proxy,refs,e))return false;
                pin.quests.emplace(sequence,q);
            }
            if(!pin.quests.empty()){
                std::vector<std::pair<int32_t,uintptr_t>> actual;
                if(!npc_census::field(memory,image_,0x13df410,pin.quest_manager,e))return false;
                if(!npc_probe::action_runtime::host_binding::quest_owners(memory,image_,pin.object,row.quest_count,actual,e))return false;
                for(const auto& [sequence,quest]:actual){const auto found=pin.quests.find(sequence);
                    if(found==pin.quests.end()||found->second.object!=quest)
                        return fail(e,"Subscriber Quest reference prepared owner changed");
                }
            }
            pins_.emplace(owner.key,pin);
        }
        return current(e);
    }
    bool identify(uintptr_t object,std::string& key,std::string& e)override {
        key.clear();for(const auto& [name,pin]:pins_)if(pin.object==object){
            if(!fresh(pin,e))return false;key=name;used_.insert(name);return true;
        }
        return fail(e,"Subscriber NPC reference has no current canonical binding");
    }
    bool resolve(const std::string& key,uintptr_t& object,std::string& e)override {
        object=0;const auto found=pins_.find(key);
        if(found==pins_.end())return fail(e,"Subscriber NPC reference shared key absent or retired");
        if(!fresh(found->second,e))return false;object=found->second.object;used_.insert(key);return true;
    }
    bool identify_quest(uintptr_t object,std::string& key,int32_t& sequence,std::string& e)override {
        key.clear();sequence=-1;
        for(const auto& [name,pin]:pins_)for(const auto& [id,quest]:pin.quests)if(quest.object==object){
            if(!fresh_quest(pin,id,quest,e,true))return false;
            key=name;sequence=id;used_.insert(name);used_quests_.emplace(name,id);return true;
        }
        return fail(e,"Subscriber Quest reference has no current canonical binding");
    }
    bool resolve_quest(const std::string& key,int32_t sequence,uintptr_t& object,std::string& e)override {
        object=0;const auto found=pins_.find(key);
        if(found==pins_.end())return fail(e,"Subscriber Quest reference NPC key absent or retired");
        const auto quest=found->second.quests.find(sequence);
        if(quest==found->second.quests.end())return fail(e,"Subscriber Quest reference sequence absent or retired");
        if(!fresh_quest(found->second,sequence,quest->second,e))return false;
        object=quest->second.object;used_.insert(key);used_quests_.emplace(key,sequence);return true;
    }
    bool verify_quest_references(std::string& e)const {
        for(const auto& [key,sequence]:used_quests_){const auto& pin=pins_.at(key);
            if(!fresh_quest(pin,sequence,pin.quests.at(sequence),e,true))return false;
        }
        return true;
    }
    bool verify_used(std::string& e)const {
        for(const auto& key:used_)if(!fresh(pins_.at(key),e))return false;
        for(const auto& [key,sequence]:used_quests_){const auto& pin=pins_.at(key);
            if(!fresh_quest(pin,sequence,pin.quests.at(sequence),e))return false;
        }
        return true;
    }
};
}
