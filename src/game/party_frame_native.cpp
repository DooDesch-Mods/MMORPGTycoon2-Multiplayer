#include "party_frame_native.h"
#include "party_capture_references.h"
#include "live_host_state/apply_timing.h"
#include "scene_clone/runtime.h"
#include "party_live_native.h"
#include "dungeon_instance_native.h"
#include "dungeon_instance_runtime.h"
#include "intervention_census.h"
#include "remove_diagnostics.h"
#include "native_open_runtime.h"
#include "subscriber_lifecycle/runtime_append.h"
#include "subscriber_lifecycle/payload.h"
#include "session_provider.h"
#include "typed_action_runtime.h"
#include "mt2mp/game/native_engine.h"
#include <Windows.h>
#include <charconv>
#include <random>

namespace mt2mp::game::party_frame {
namespace {
namespace life=dungeon_instance_lifecycle;
namespace journal=marketing_authority::notice;
namespace append=subscriber_lifecycle_append;
namespace local=mt2mp::subscriber_lifecycle;
struct Owner {uintptr_t object=0,stamp=0;uint64_t uid=0;bool prop=false;};
bool read(uintptr_t object,size_t offset,void* value,size_t bytes,std::string& error){
    if(!object||offset>UINTPTR_MAX-object||bytes>UINTPTR_MAX-(object+offset))return fail(error,"native field span invalid");
    SIZE_T actual=0;if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(object+offset),value,bytes,&actual)||actual!=bytes)
        return fail(error,"native read failed error="+std::to_string(GetLastError()));return true;
}
template<class T>bool at(uintptr_t object,size_t offset,T& value,std::string& error){return read(object,offset,&value,sizeof(value),error);}
DWORD find_uid(uintptr_t image,uint64_t uid,uintptr_t& found){
    __try{found=reinterpret_cast<uintptr_t(*)(uint64_t)>(image+0x47ce20)(uid);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD restore_pause(const live_host_state::Pin& pin){
    __try{reinterpret_cast<void(*)(uintptr_t,float)>(pin.image+0x259b70)(pin.clock,pin.pause);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
std::string operation(){
    std::random_device random;std::array<unsigned char,16> bytes{};for(auto& byte:bytes)byte=static_cast<unsigned char>(random());
    bytes[6]=static_cast<unsigned char>((bytes[6]&15u)|64u);bytes[8]=static_cast<unsigned char>((bytes[8]&63u)|128u);
    constexpr char hex[]="0123456789abcdef";std::string out;out.reserve(36);
    for(size_t i=0;i<bytes.size();++i){if(i==4||i==6||i==8||i==10)out+='-';out+=hex[bytes[i]>>4];out+=hex[bytes[i]&15];}return out;
}
bool toon_id(const std::string& key,int32_t& id){
    if(!key.starts_with("toon:"))return false;const auto* begin=key.data()+5;const auto* end=key.data()+key.size();
    const auto parsed=std::from_chars(begin,end,id);return parsed.ec==std::errc{}&&parsed.ptr==end&&id>=0&&key=="toon:"+std::to_string(id);
}
class Native final:public live_host_state::PartyPort {
    live_host_state::Port& authority_;
    party_live_native::NativePort native_;
    scene_clone::Runtime scenes_;
    std::optional<action_session::Request> dungeon_building_;
    std::optional<scene_clone::Mapping> scene_mapping_;
    party_live::Adapter parties_{native_};
    dungeon_instance_native::NativePort instances_;
    std::unique_ptr<life::Executor> executor_;
    action_session::Context context_;action_session::Binding binding_;
    uint64_t epoch_=0,prepared_revision_=0,applied_revision_=0,capture_revision_=0;
    uintptr_t image_=0;uint32_t thread_=0;
    bool bound_=false,suspended_=true,locked_=false,held_=false;
    std::string cause_,capture_operation_;
    uint32_t birth_ordinal_=0;
    journal::Origin origin_;std::optional<journal::Origin> source_;
    journal::Host host_;journal::Client client_;
    party_live::Raw prior_;
    std::map<std::string,Owner> owners_;
    std::map<uintptr_t,std::string> keys_;
    std::map<std::string,party_live::Reservation> reserved_;
    append::PartyToonBatch* pending_=nullptr;
    std::optional<State> prepared_;
    std::map<uint64_t,std::set<std::string>> client_actions_;
    std::set<std::string> host_actions_;
    nlohmann::json last_=nullptr;
    bool reject(std::string& error,const std::string& cause,bool unknown=false){
        if(unknown){locked_=true;if(cause_.empty())cause_=cause;}return fail(error,cause_.empty()?cause:cause_);
    }
    bool current(std::string& error){
        if(!bound_||suspended_||locked_||GetCurrentThreadId()!=thread_)return reject(error,"native owner suspended/locked/outside Mainthread");
        action_session::Context context;action_session::Binding binding;building_mixed_identity::opened_file::CurrentWorld world;
        if(!action_session::selected_session().current(context,error)||!typed_action_runtime::read_binding(context_.snapshot_uuid,binding,error)||
           !native_open_runtime::read_current_world(world,error))return false;
        return (context==context_&&binding==binding_&&world.uuid==prior_.scope.snapshot&&world.world_epoch==binding_.native_epoch&&
            world.generation==binding_.native_generation&&uintptr_t(world.map)+0x1c0==prior_.scope.root)||fail(error,"actual Session/native world owner changed: session_snapshot="+context.snapshot_uuid+
                " bound_session_snapshot="+context_.snapshot_uuid+" native_snapshot="+world.uuid+" bound_native_snapshot="+prior_.scope.snapshot+
                " native_epoch="+std::to_string(world.world_epoch)+" binding_epoch="+std::to_string(binding_.native_epoch)+
                " native_generation="+std::to_string(world.generation)+" binding_generation="+std::to_string(binding_.native_generation)+
                " native_root="+std::to_string(uintptr_t(world.map)+0x1c0)+" bound_root="+std::to_string(prior_.scope.root));
    }
    bool checked(const Owner& row,std::string& error){
        if(!current(error))return false;uintptr_t found=0,stamp=0;
        if(row.prop){const auto key=keys_.find(row.object);
            if(key==keys_.end())return fail(error,"Party prop owner lost its canonical key");
            if(!remove_diagnostics::party_identity_current(context_.snapshot_uuid,key->second,row.uid,row.object,row.stamp,error))return false;}
        if(row.uid){const auto code=find_uid(image_,row.uid,found);if(code||found!=row.object)return fail(error,"canonical external full UID changed SEH="+std::to_string(code));}
        return at(row.object,0,stamp,error)&&(stamp==row.stamp||fail(error,"canonical external owner header changed"));
    }
    bool prop_pins(std::map<std::string,Owner>& owners,std::map<uintptr_t,std::string>& keys,std::string& error){
        nlohmann::json props;if(!remove_diagnostics::party_identity_bindings(props,error))return false;
        for(const auto& prop:props){
            if(prop.at("snapshot_id")!=context_.snapshot_uuid)return fail(error,"Party prop alias snapshot differs from current Session");
            const auto key=prop.at("record_key").get<std::string>();
            const Owner row{prop.at("object").get<uintptr_t>(),prop.at("stamp").get<uintptr_t>(),prop.at("local_uid").get<uint64_t>(),true};
            const auto pointer=keys.find(row.object);const auto named=owners.find(key);
            if((pointer!=keys.end()&&pointer->second!=key)||(named!=owners.end()&&
               (named->second.object!=row.object||named->second.stamp!=row.stamp||(named->second.uid&&named->second.uid!=row.uid))))
                return fail(error,"Party prop alias conflicts with a current external owner");
            keys[row.object]=key;owners[key]=row;
        }
        return true;
    }
    bool pins(std::string& error){
        if(held_)return fail(error,"complete external owner census requires SaveIdle");
        interventions::Census census;if(!current(error)||!interventions::actor_census(context_,binding_,census,error))return false;
        std::map<uintptr_t,uint64_t> uids;for(const auto& actor:census.actors)uids.emplace(actor.object,actor.uid);
        for(const auto& definition:census.definitions.definitions)if(definition.uid)uids.emplace(definition.object,definition.uid);
        party_live::Raw raw;if(!parties_.raw_current(context_,binding_,raw,error))return false;
        for(const auto& party:raw.members)uids.emplace(party.pointer,party.uid);
        std::map<std::string,Owner> owners;std::map<uintptr_t,std::string> keys;
        for(const auto& [pointer,key]:census.owners){Owner row;row.object=pointer;if(!at(pointer,0,row.stamp,error))return false;
            const auto uid=uids.find(pointer);if(uid!=uids.end())row.uid=uid->second;
            if(!owners.emplace(key,row).second||!keys.emplace(pointer,key).second)return fail(error,"complete canonical external owners repeat");}
        if(!prop_pins(owners,keys,error)||!current(error))return false;
        owners_=std::move(owners);keys_=std::move(keys);return true;
    }
    bool capture_pins(std::string& error,bool incoming_parties=false,bool incoming_subscribers=false){
        if(held_)return fail(error,"complete external owner census requires SaveIdle");
        return capture_reference_pins<party_live::Raw>(
            [&](party_live::Raw& raw,std::string& cause){return current(cause)&&parties_.raw_current(context_,binding_,raw,cause);},
            [&](std::string& cause){return pins(cause);},
            [&]{owners_.clear();keys_.clear();},error,incoming_parties,incoming_subscribers);
    }
    bool any_identify(uintptr_t pointer,std::string& key,std::string& error){
        if(!current(error))return false;
        for(const auto& [name,row]:reserved_)if(row.member.pointer==pointer){uintptr_t found=0;const auto code=find_uid(image_,row.member.uid,found);
            if(code||found!=pointer)return fail(error,"reserved original Party full UID changed");key=name;return true;}
        if(pending_){for(const auto& [name,row]:pending_toons_){if(row.toon!=pointer)continue;append::PendingToon fresh;
            if(!append::pending_toon(*pending_,row.subscriber_id,fresh,error)||fresh!=row)return false;key=name;return true;}}
        const auto found=keys_.find(pointer);if(found==keys_.end()){
            uintptr_t ap=0;if(!at(pointer,0,ap,error))return false;
            if(ap==image_+dungeon_party_census::party_ap)return identify(pointer,key,error);
            return fail(error,"original Record external owner has no current canonical identity");}
        const auto owner=owners_.find(found->second);if(owner==owners_.end()||!checked(owner->second,error))return false;key=found->second;return true;
    }
    bool any_resolve(const std::string& key,uintptr_t& pointer,std::string& error){
        pointer=0;if(!current(error))return false;
        const auto reserved=reserved_.find(key);if(reserved!=reserved_.end()){
            uintptr_t found=0;const auto code=find_uid(image_,reserved->second.member.uid,found);
            if(code||found!=reserved->second.member.pointer)return fail(error,"reserved Party lifetime changed");pointer=found;return true;}
        int32_t id=-1;if(pending_&&toon_id(key,id)){
            const auto found=pending_toons_.find(key);if(found!=pending_toons_.end()){append::PendingToon fresh;
                if(!append::pending_toon(*pending_,id,fresh,error)||fresh!=found->second)return false;pointer=fresh.toon;return true;}}
        const auto found=owners_.find(key);if(found==owners_.end()&&party_identity::valid_key(key))return resolve(key,pointer,error);
        if(found==owners_.end()||!checked(found->second,error))return fail(error,error.empty()?"original Record canonical external owner is absent":error);
        pointer=found->second.object;return true;
    }
    std::map<std::string,append::PendingToon> pending_toons_;
    party_live::References references(){return {[this](uintptr_t p,std::string& key,std::string& e){return any_identify(p,key,e);},
        [this](const std::string& key,uintptr_t& p,std::string& e){return any_resolve(key,p,e);}};}
    bool host_batch(const std::function<bool(std::string&)>& body,std::string& error){
        live_host_state::Pin before;if(!authority_.read(before,error))return false;
        const bool okay=native_.batch(context_,binding_,[&](std::string& e){held_=true;return body(e);},error);held_=false;
        if(!okay)return false;live_host_state::Pin after;if(!authority_.read(after,error))return false;
        if(before.pause==1&&after.pause==0){auto compared=before;compared.pause=0;
            if(compared!=after)return fail(error,"original Party capture changed native Clock/ledger/owners before pause restore");
            constexpr std::array<unsigned char,10> expected{0x56,0x53,0x48,0x83,0xec,0x48,0x0f,0x2f,0x49,0x5c};std::array<unsigned char,10> actual{};
            if(!read(before.image,0x259b70,actual.data(),actual.size(),error)||actual!=expected)return fail(error,error.empty()?"original pause setter changed":error);
            const auto code=restore_pause(before);if(code)return fail(error,"original pause restoration SEH="+std::to_string(code));
            if(!authority_.read(after,error)||after!=before)return fail(error,error.empty()?"original pause restoration readback differs":error);
        }
        return true;
    }
    bool births(std::string& error){
        party_live::Raw raw;if(!parties_.raw_current(context_,binding_,raw,error))return false;
        std::vector<std::pair<std::string,party_identity::Entry>> entries;if(!party_identity::registry.enumerate(raw.scope,entries,error))return false;
        for(const auto& row:raw.members){const auto old=std::find_if(entries.begin(),entries.end(),[&](const auto& entry){return entry.second.uid==row.uid;});
            if(old!=entries.end()){if(old->second.retired||old->second.pointer!=row.pointer)return fail(error,"Host current Party reused a known native lifetime");continue;}
            if(birth_ordinal_==INT32_MAX)return fail(error,"Host Party birth ordinal exhausted");party_live::Receipt receipt;
            if(!parties_.bind_host_birth(context_,binding_,prior_,capture_operation_,birth_ordinal_++,row,receipt,error))return false;
        }
        for(const auto& [key,row]:entries)if(!row.retired&&std::none_of(raw.members.begin(),raw.members.end(),[&](const auto& member){return member.pointer==row.pointer&&member.uid==row.uid;})){
            party_live::Receipt receipt;if(!parties_.host_retired(context_,binding_,key,row,receipt,error))return false;}
        prior_=std::move(raw);return true;
    }
    bool context(const action_session::Context& context,const action_session::Binding& binding,std::string& error){
        if(context!=context_||binding!=binding_)return fail(error,"caller Context/Binding differs");
        return current(error);
    }
public:
    explicit Native(live_host_state::Port& authority):authority_(authority){}
    bool bind(const action_session::Context& context,const action_session::Binding& binding,uint64_t epoch,std::string& error)override{
        if(bound_&&!suspended_)return fail(error,"native Party provider cannot replace an active world");
        live_host_state::Pin pin;if(!authority_.read(pin,error)||pin.context!=context||pin.binding!=binding||pin.common_epoch!=epoch)return fail(error,error.empty()?"actual economic provider binding differs":error);
        context_=context;binding_=binding;epoch_=epoch;image_=pin.image;thread_=pin.thread;locked_=false;cause_.clear();
        if(!parties_.raw_current(context_,binding_,prior_,error))return false;
        origin_={context.session,epoch,binding.native_generation,context.snapshot_uuid};
        if(!dungeon_instance_native::bind(context,binding,origin_,error)||!scenes_.bind(context,binding,error))return false;
        scene_mapping_.reset();
        if(context.role==action_session::Role::Host&&!host_.bind(origin_,uint32_t(context.peer_mask&~(1u<<(context.local_peer-1))),error))return false;
        bound_=true;suspended_=false;source_.reset();client_={};executor_=std::make_unique<life::Executor>(instances_);
        prepared_revision_=applied_revision_=capture_revision_=0;prepared_.reset();reserved_.clear();pending_toons_.clear();pending_=nullptr;
        host_actions_.clear();client_actions_.clear();return pins(error);
    }
    bool suspend(std::string& error)override{
        suspended_=true;scenes_.reset();scene_mapping_.reset();dungeon_building_.reset();const bool okay=dungeon_instance_native::reset(error);if(!okay)return false;
        reserved_.clear();pending_toons_.clear();pending_=nullptr;prepared_.reset();owners_.clear();keys_.clear();error.clear();return true;
    }
    bool identify(uintptr_t pointer,std::string& key,std::string& error)override{
        if(context_.role==action_session::Role::Host&&!births(error))return false;
        uintptr_t ap=0;if(!at(pointer,0,ap,error)||ap!=image_+dungeon_party_census::party_ap)return fail(error,error.empty()?"shared Party class differs":error);
        const auto pending=std::find_if(reserved_.begin(),reserved_.end(),[&](const auto& row){return row.second.member.pointer==pointer;});
        if(pending!=reserved_.end())return any_identify(pointer,key,error);
        party_live::Raw raw;if(!parties_.raw_current(context_,binding_,raw,error))return false;
        const auto member=std::find_if(raw.members.begin(),raw.members.end(),[&](const auto& row){return row.pointer==pointer;});
        if(member==raw.members.end())return fail(error,"shared Party absent from actual current membership");
        if(!party_identity::registry.identify(raw.scope,pointer,member->uid,key,error))return false;
        owners_[key]={pointer,ap,member->uid};keys_[pointer]=key;return true;
    }
    bool resolve(const std::string& key,uintptr_t& pointer,std::string& error)override{
        if(!party_identity::valid_key(key))return fail(error,"shared Party canonical key invalid");
        if(!reserved_.contains(key)){
            party_live::Raw raw;party_identity::Entry entry;
            if(!parties_.raw_current(context_,binding_,raw,error)||!party_identity::registry.lookup(raw.scope,key,entry,error))return false;
            const auto found=std::find_if(raw.members.begin(),raw.members.end(),[&](const auto& row){return row.pointer==entry.pointer&&row.uid==entry.uid;});
            if(found==raw.members.end())return fail(error,"resolved Party absent from actual current membership");
            uintptr_t stamp=0;if(!at(entry.pointer,0,stamp,error))return false;
            owners_[key]={entry.pointer,stamp,entry.uid};keys_[entry.pointer]=key;
        }
        if(!any_resolve(key,pointer,error))return false;uintptr_t ap=0;
        return at(pointer,0,ap,error)&&(ap==image_+dungeon_party_census::party_ap||fail(error,"resolved shared Party class differs"));
    }
    bool resolve_held_dungeon_destination(uintptr_t pointer,std::string& key,bool& matched,std::string& error,std::string* scene)override{
        return scenes_.resolve_retained_destination(pointer,key,matched,error,scene);
    }
    bool begin_dungeon_building(const action_session::Context& context,const action_session::Binding& binding,
        const action_session::Request& request,std::string& error)override{
        using A=action_session::Action;
        const bool place=request.action==A::PlaceBuilding;
        const bool move=request.action==A::MoveBuilding;
        const bool other=request.action==A::RemoveSnapshotBuilding||request.action==A::UndoBuilding||request.action==A::RedoBuilding;
        const auto controller=typed_action_runtime::controller_status();
        if(dungeon_building_||!request.dungeon_building||(!place&&!move&&!other)||
           request.snapshot_uuid!=context.snapshot_uuid||!dungeon_scene_key::building_key(request.canonical_record_key)||
           !controller.value("started",false)||controller.value("locked",true)||!controller.value("pending",false)||
           controller.value("action_uuid",std::string{})!=request.action_uuid||!this->context(context,binding,error))
            return fail(error,error.empty()?"Dungeon Building scene phase has no exact pending original action":error);
        if(!scenes_.prepare(context,binding,error))return false;
        dungeon_building_=request;return true;
    }
    bool finish_dungeon_building(const action_session::Context& context,const action_session::Binding& binding,
        const action_session::Request& request,const scene_clone::Mapping* authority,
        scene_clone::Mapping& mapping,bool& attempted,std::string& error,uintptr_t held_node)override{
        attempted=false;mapping={};
        if(!dungeon_building_||*dungeon_building_!=request||!this->context(context,binding,error)||
           bool(authority)!=(context.role==action_session::Role::Client))
            return fail(error,error.empty()?"Dungeon Building scene completion differs from prepared action":error);
        // The original Building path has completed its queued maker before this
        // registry publication. Keep the same retained baseline used by Frames.
        dungeon_scene_identity_runtime::Census admitted;
        if(!scenes_.confirm_template(context,binding,request,authority,mapping,admitted,attempted,error,held_node))
            return reject(error,error,attempted);
        scene_mapping_=mapping;dungeon_building_.reset();return true;
    }
    bool cancel_dungeon_building(const action_session::Context& context,const action_session::Binding& binding,
        const action_session::Request& request,std::string& error)override{
        if(!dungeon_building_||*dungeon_building_!=request||!this->context(context,binding,error))
            return fail(error,error.empty()?"Dungeon Building cancellation differs from exact prepared action":error);
        if(!scenes_.cancel_prepare(context,binding,error))return reject(error,error,true);
        dungeon_building_.reset();return true;
    }
    bool prepare_capture(const action_session::Context& context,const action_session::Binding& binding,uint64_t revision,std::string& error)override{
        if(dungeon_building_)return fail(error,"Party Frame capture waits for the exact Dungeon Building scene phase");
        if(!this->context(context,binding,error)||context.role!=action_session::Role::Host||!revision)return false;
        if(!([&]{live_host_state::apply_timing::Scope measured(live_host_state::apply_timing::Part::HostPartyScenePrepare);return scenes_.prepare(context,binding,error);}())||!([&]{live_host_state::apply_timing::Scope measured(live_host_state::apply_timing::Part::HostPartyNativeBatch);return host_batch([](std::string& e){e.clear();return true;},error);}()))return false;
        capture_operation_=operation();capture_revision_=revision;birth_ordinal_=0;
        std::vector<life::Event> events;std::vector<life::Pin> pools;if(!([&]{live_host_state::apply_timing::Scope measured(live_host_state::apply_timing::Part::HostPartyDrain);return dungeon_instance_native::drain(capture_operation_,events,pools,error);}()))return false;
        for(const auto& event:events){if(!life::append(host_,origin_,event,error))return false;if(event.action)host_actions_.insert(event.operation);}
        scene_clone::Mapping mapping;dungeon_scene_identity_runtime::Census admitted;bool attempted=false;
        if(!([&]{live_host_state::apply_timing::Scope measured(live_host_state::apply_timing::Part::HostPartySceneConfirm);return scenes_.confirm(context,binding,operation(),nullptr,mapping,admitted,attempted,error);}()))return reject(error,error,attempted);
        scene_mapping_=std::move(mapping);return ([&]{live_host_state::apply_timing::Scope measured(live_host_state::apply_timing::Part::HostPartyBirths);return births(error);}())&&([&]{live_host_state::apply_timing::Scope measured(live_host_state::apply_timing::Part::HostPartyPins);return capture_pins(error);}());
    }
    bool capture(const action_session::Context& context,const action_session::Binding& binding,const live_host_state::Frame& frame,State& out,std::string& error)override{
        out={};if(!this->context(context,binding,error)||context.role!=action_session::Role::Host||capture_revision_!=frame.revision)return false;
        if(!births(error)||!capture_pins(error))return false;std::vector<party_live::Record> records;
        if(!scenes_.prepare(context,binding,error)||!host_batch([&](std::string& e){return parties_.current_records(context,binding,references(),records,e);},error))return false;
        scene_clone::Mapping mapping;dungeon_scene_identity_runtime::Census admitted;bool attempted=false;
        if(!scenes_.confirm(context,binding,operation(),nullptr,mapping,admitted,attempted,error))return reject(error,error,attempted);
        scene_mapping_=std::move(mapping);State ready;ready.scenes=scene_mapping_;for(const auto& record:records)ready.parties.push_back(encode_record(record).dump());
        dungeon_instance_live::Census values;if(!dungeon_instance_live::take_current(context,binding,values,error))return false;
        for(const auto& row:values.rows)ready.instances.push_back(row.state);
        std::sort(ready.instances.begin(),ready.instances.end(),[](const auto& a,const auto& b){return a.scene<b.scene;});
        std::vector<life::Event> events;std::vector<life::Pin> pools;if(!dungeon_instance_native::drain(capture_operation_,events,pools,error))return false;
        for(const auto& event:events){if(!life::append(host_,origin_,event,error))return false;if(event.action)host_actions_.insert(event.operation);}
        for(const auto& pool:pools)ready.pools.push_back({pool.key,pool.counter,life::indices(pool)});
        std::sort(ready.pools.begin(),ready.pools.end(),[](const auto& a,const auto& b){return a.exterior<b.exterior;});
        ready.lifecycle=host_.pending();if(!valid(ready,error))return false;out=std::move(ready);return true;
    }
    bool prepare_apply(const action_session::Context& context,const action_session::Binding& binding,const live_host_state::Frame& frame,live_host_state::PartyReceipt& receipt,std::string& error)override{
        receipt={};if(!this->context(context,binding,error)||context.role!=action_session::Role::Client||!frame.party||!valid(*frame.party,error))return false;
        const journal::Origin source{frame.session,frame.epoch,frame.source_generation,frame.snapshot};
        if(!source_){if(!client_.bind(source,error))return false;source_=source;}else if(*source_!=source)return fail(error,"Host source changed before snapshot rebind");
        if(prepared_revision_==frame.revision){if(!prepared_||*prepared_!=*frame.party)return fail(error,"repeated prepared Frame differs");receipt.confirmed=true;return true;}
        if(!frame.party->scenes)return fail(error,"Host Party Frame omitted complete scene clone assignments");
        std::vector<journal::Row> work;if(![&](){std::optional<live_host_state::apply_timing::Scope> measured;if(live_host_state::apply_timing::client_party_prepare_depth)measured.emplace(live_host_state::apply_timing::Part::ClientPartyJournalPrepare);return client_.prepare(source,frame.party->lifecycle,work,error);}()||![&](){std::optional<live_host_state::apply_timing::Scope> measured;if(live_host_state::apply_timing::client_party_prepare_depth)measured.emplace(live_host_state::apply_timing::Part::ClientPartyScenePrepare);return scenes_.prepare(context,binding,error);}())return false;
        const bool okay=[&](){std::optional<live_host_state::apply_timing::Scope> measured;if(live_host_state::apply_timing::client_party_prepare_depth)measured.emplace(live_host_state::apply_timing::Part::ClientPartyNativeBatch);return native_.batch(context,binding,[&](std::string& e){held_=true;
            for(const auto& row:work){const auto event=life::event(row);life::Receipt actual;const bool applied=executor_->apply(event,actual,e);
                receipt.attempted=receipt.attempted||actual.attempted;receipt.unknown=receipt.unknown||actual.unknown;receipt.calls+=actual.calls;
                if(!applied)return false;if(event.action)client_actions_[frame.revision].insert(event.operation);}
            std::vector<std::pair<std::string,int32_t>> counters;for(const auto& pool:frame.party->pools)counters.emplace_back(pool.exterior,pool.counter);
            life::Receipt adopted;const bool applied=dungeon_instance_native::adopt_counters(counters,adopted,e);
            receipt.attempted=receipt.attempted||adopted.attempted;receipt.stores+=adopted.counter_stores;receipt.unknown=receipt.unknown||adopted.unknown;
            if(!applied)return false;
            for(const auto& pool:frame.party->pools){life::Pin actual;if(!instances_.current(pool.exterior,actual,e)||life::indices(actual)!=pool.indices||actual.counter!=pool.counter)return fail(e,e.empty()?"complete original Instance membership/counter differs":e);}
            return true;
        },error);}();held_=false;
        if(!okay){receipt.unknown=receipt.unknown||receipt.attempted;return reject(error,error,receipt.unknown);}
        scene_clone::Mapping mapping;dungeon_scene_identity_runtime::Census admitted;bool published=false;
        if(![&](){std::optional<live_host_state::apply_timing::Scope> measured;if(live_host_state::apply_timing::client_party_prepare_depth)measured.emplace(live_host_state::apply_timing::Part::ClientPartySceneConfirm);return scenes_.confirm(context,binding,frame.party->scenes->operation,&*frame.party->scenes,mapping,admitted,published,error);}()){receipt.attempted=receipt.attempted||published;receipt.unknown=receipt.attempted;return reject(error,error,receipt.unknown);}
        receipt.attempted=receipt.attempted||published;scene_mapping_=std::move(mapping);
        if(!client_.confirm(source,frame.party->lifecycle,work,error))return reject(error,error,receipt.attempted);
        prepared_revision_=frame.revision;prepared_=*frame.party;receipt.confirmed=true;return true;
    }
    bool apply(const action_session::Context&,const action_session::Binding&,const live_host_state::Frame&,
               const std::vector<nlohmann::json>&,nlohmann::json&,std::string&)override;
    bool sent(const live_host_state::Frame& frame,std::string& error)override{
        if(!frame.party||!host_.sent(origin_,frame.revision,frame.party->lifecycle,error))return false;return release_host(error);
    }
    bool release_host(std::string& error){
        std::set<std::string> retained;for(const auto& row:host_.pending().rows){const auto event=life::event(row);if(event.action)retained.insert(event.operation);}
        for(auto it=host_actions_.begin();it!=host_actions_.end();)if(!retained.contains(*it)){if(!dungeon_instance_native::release_action(*it,error))return false;it=host_actions_.erase(it);}else ++it;
        error.clear();return true;
    }
    bool acknowledge(uint64_t peer,uint64_t revision,std::string& error)override{
        return host_.acknowledge(origin_,uint32_t(peer),revision,error)&&release_host(error);
    }
    bool committed(const live_host_state::Frame& frame,std::string& error)override{
        if(frame.revision!=applied_revision_)return fail(error,"Frame ACK lacks exact confirmed Party batch");
        std::set<std::string> completed;
        for(const auto& [revision,actions]:client_actions_)if(revision<=frame.revision)completed.insert(actions.begin(),actions.end());
        for(const auto& operation:completed)if(!dungeon_instance_native::release_action(operation,error))return false;
        for(auto it=client_actions_.begin();it!=client_actions_.end();)if(it->first<=frame.revision)it=client_actions_.erase(it);else ++it;
        error.clear();return true;
    }
    bool admit(const live_host_state::Frame& frame,const live_host_state::Frame* previous,std::string& error)override{
        if(!frame.party||!valid(*frame.party,error))return false;const journal::Origin source{frame.session,frame.epoch,frame.source_generation,frame.snapshot};
        if(!source_){if(!client_.bind(source,error))return false;source_=source;}else if(*source_!=source)return fail(error,"Host source changed without snapshot rebind");
        return client_.supersede(source,frame.party->lifecycle,previous&&previous->party?&previous->party->lifecycle:nullptr,error);
    }
    nlohmann::json status()const override{return {{"bound",bound_},{"suspended",suspended_},{"locked",locked_},{"cause",cause_},
        {"prepared_revision",prepared_revision_},{"applied_revision",applied_revision_},{"pending_lifecycle",host_.pending().rows.size()},
        {"committed_lifecycle",client_.committed()},{"last",last_}};}
};
bool Native::apply(const action_session::Context& context,const action_session::Binding& binding,
    const live_host_state::Frame& frame,const std::vector<nlohmann::json>& graphs,nlohmann::json& receipt,std::string& error){
    receipt={{"attempted",false},{"confirmed",false},{"unknown",false},{"rows",nlohmann::json::array()}};
    if(!this->context(context,binding,error)||context.role!=action_session::Role::Client||!frame.party||
       prepared_revision_!=frame.revision||!prepared_||*prepared_!=*frame.party)
        return fail(error,error.empty()?"Party application lacks exact prepared Host Frame":error);
    if(applied_revision_==frame.revision){
        if(!graphs.empty())return fail(error,"confirmed Party Frame cannot repeat Subscriber construction");
        receipt["confirmed"]=true;return true;
    }
    bool attempted=false,unknown=false;
    nlohmann::json native_receipts=nlohmann::json::array();
    struct Cleanup {Native& owner;~Cleanup(){owner.held_=false;owner.pending_=nullptr;owner.pending_toons_.clear();owner.reserved_.clear();}} cleanup{*this};
    const auto finish_failure=[&](){
        receipt["attempted"]=attempted;receipt["unknown"]=unknown||attempted;
        receipt["native_receipts"]=native_receipts;receipt["cause"]=error;
        return reject(error,error.empty()?"Party/Subscriber batch failed":error,unknown||attempted);
    };
    const auto remember=[&](const party_live::Receipt& row){
        attempted=attempted||row.attempted;unknown=unknown||row.unknown;
        native_receipts.push_back({{"kind","party"},{"attempted",row.attempted},{"confirmed",row.confirmed},
            {"unknown",row.unknown},{"factory",row.calls.factory},{"load",row.calls.load},{"context",row.calls.context},
            {"toon_leave",row.calls.toon_leave},{"toon_bind",row.calls.toon_bind},{"release",row.calls.release},{"remove",row.calls.remove},{"cause",row.cause}});
    };
    try{
        std::vector<party_live::Record> targets;std::set<std::string> wanted;
        for(const auto& encoded:frame.party->parties){auto row=record(encoded);wanted.insert(row.key);targets.push_back(std::move(row));}
        const auto refresh_pins=[&](){return capture_pins(error,!targets.empty(),!graphs.empty());};
        building_mixed_identity::opened_file::CurrentWorld world;
        if(!native_open_runtime::read_current_world(world,error)||!refresh_pins())return finish_failure();
        const append::FrameBinding owner{context,binding,epoch_,world.sha256};
        std::vector<nlohmann::json> online;std::vector<int32_t> ids;
        for(const auto& graph:graphs){local::first_append::Payload payload;
            if(!local::first_append::parse_payload(graph,payload,error))return finish_failure();ids.push_back(payload.id);
            if(payload.online){online.push_back(graph);continue;}
            nlohmann::json actual;const bool okay=append::apply_frame(graph,world.uuid,owner,*this,actual,error);
            attempted=attempted||actual.value("subscriber_creation_attempted",false);
            unknown=unknown||actual.value("state",std::string{})=="unknown";
            native_receipts.push_back(actual);
            if(!okay||actual.value("state",std::string{})!="confirmed"){
                if(error.empty())error="offline Subscriber original append lacks confirmation";return finish_failure();}
        }
        if(!refresh_pins())return finish_failure();
        std::shared_ptr<append::PartyToonInputs> inputs;
        if(!online.empty()&&!append::prepare_party_toons(online,world.uuid,owner,inputs,error))return finish_failure();
        party_live::Raw before;if(!parties_.raw_current(context,binding,before,error))return finish_failure();
        std::vector<std::pair<std::string,party_identity::Entry>> entries;
        if(!party_identity::registry.enumerate(before.scope,entries,error))return finish_failure();
        std::map<std::string,party_live::Member> existing;
        for(const auto& [key,entry]:entries){if(entry.retired){if(wanted.contains(key)){error="Host targets retired Party lifetime";return finish_failure();}continue;}
            const auto found=std::find_if(before.members.begin(),before.members.end(),[&](const auto& row){return row.pointer==entry.pointer&&row.uid==entry.uid;});
            if(found==before.members.end()){error="current Party alias absent from original native census";return finish_failure();}
            existing.emplace(key,*found);
        }
        const auto birth_parts=[&](const std::string& key,std::string& op,uint32_t& ordinal,std::string& e){
            if(!key.starts_with("action:")||!party_identity::valid_key(key))return fail(e,"new Party lacks Host operation identity");
            op=key.substr(7,36);const auto text=std::string_view(key).substr(50);const auto parsed=std::from_chars(text.data(),text.data()+text.size(),ordinal);
            return (parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size())||fail(e,"new Party ordinal invalid");
        };
        const auto reserve=[&](std::string& e){
            for(const auto& target:targets)if(!existing.contains(target.key)){
                std::string op;uint32_t ordinal=0;if(!birth_parts(target.key,op,ordinal,e))return false;
                party_live::Reservation reservation;party_live::Receipt actual;
                const bool okay=parties_.reserve_birth(context,binding,op,ordinal,target,reservation,actual,e);remember(actual);
                if(!okay)return false;reserved_.emplace(target.key,std::move(reservation));}
            return true;
        };
        const auto prepare_parties=[&](const std::vector<append::PendingToon>& toons,std::string& e){
            for(const auto& toon:toons)if(!pending_toons_.emplace("toon:"+std::to_string(toon.subscriber_id),toon).second)
                return fail(e,"pending original Toon identity repeats");
            for(const auto& target:targets){party_live::Receipt actual;const auto found=existing.find(target.key);
                const bool okay=found==existing.end()?native_.prepare_reserved(target,references(),actual,e):native_.prepare_record(found->second,target,references(),actual,e);
                remember(actual);if(!okay)return false;}
            return true;
        };
        const auto finalize=[&](std::string& e){
            for(const auto& target:targets){party_live::Receipt actual;bool okay=false;
                if(existing.contains(target.key))okay=parties_.client_adopt(context,binding,target.key,target,references(),actual,e);
                else{std::string op;uint32_t ordinal=0;if(!birth_parts(target.key,op,ordinal,e))return false;
                    okay=parties_.client_birth(context,binding,op,ordinal,target,references(),actual,e);}
                remember(actual);if(!okay)return false;
            }
            for(const auto& [key,member]:existing)if(!wanted.contains(key)){party_live::Receipt actual;
                const bool okay=parties_.client_retire(context,binding,key,actual,e);remember(actual);if(!okay)return false;}
            dungeon_instance_live::Receipt actual;
            const bool adopted=dungeon_instance_live::adopt_held(context,binding,frame.party->instances,actual,e);
            attempted=attempted||actual.attempted;unknown=unknown||actual.unknown;
            native_receipts.push_back({{"kind","instances"},{"attempted",actual.attempted},{"confirmed",actual.confirmed},
                {"unknown",actual.unknown},{"stores",actual.stores},{"party_calls",actual.party_calls},{"cause",actual.cause}});
            if(!adopted)return false;
            std::vector<party_live::Record> actual_records;if(!parties_.current_records(context,binding,references(),actual_records,e))return false;
            if(actual_records.size()!=targets.size())return fail(e,"complete Party membership differs after original batch");
            for(size_t i=0;i<targets.size();++i)if(actual_records[i].key!=targets[i].key||actual_records[i].sha!=targets[i].sha)
                return fail(e,"complete Party original Record readback differs");
            return true;
        };
        const bool okay=native_.batch(context,binding,[&](std::string& e){held_=true;
            if(!reserve(e))return false;
            if(online.empty())return finalize(e);
            local::SaveScopeReceipt scope;
            const bool applied=append::with_party_toon_edits_held(world.uuid,owner,[&](append::PartyToonBatch& batch,std::string& cause){
                pending_=&batch;nlohmann::json actual;
                const bool done=append::append_party_toons(batch,inputs,*this,prepare_parties,
                    [&](const append::PendingToon& toon,int32_t id,void* object_context,std::string& reason){party_live::Receipt row;
                        const bool rearmed=native_.rearm_toon_party(toon.toon,toon.uid,id,object_context,row,reason);remember(row);return rearmed;},finalize,actual,cause);
                attempted=attempted||actual.value("subscriber_creation_attempted",false);
                unknown=unknown||actual.value("state",std::string{})=="unknown";native_receipts.push_back(actual);return done;
            },[&](std::string& cause){return current(cause);},scope,e);
            unknown=unknown||scope.unknown;return applied;
        },error);held_=false;
        if(!okay)return finish_failure();
        pending_=nullptr;pending_toons_.clear();reserved_.clear();
        if(!current(error)||!refresh_pins())return finish_failure();
        for(const auto id:ids)receipt["rows"].push_back({{"id",id},{"state","confirmed"}});
        receipt["attempted"]=attempted;receipt["confirmed"]=true;receipt["native_receipts"]=native_receipts;
        applied_revision_=frame.revision;last_=receipt;error.clear();return true;
    }catch(const std::exception& exception){error=std::string("Party/Subscriber batch exception: ")+exception.what();return finish_failure();}
    catch(...){error="Party/Subscriber batch unknown exception";unknown=true;return finish_failure();}
}
}
std::unique_ptr<live_host_state::PartyPort> make_native(live_host_state::Port& authority){return std::make_unique<Native>(authority);}
}
