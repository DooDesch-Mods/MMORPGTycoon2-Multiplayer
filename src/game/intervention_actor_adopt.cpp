#include "intervention_actor_runtime.h"
#include "intervention_actor_adopt.h"
#include "intervention_actor_saved.h"
#include "intervention_actor_saved_adopt.h"
#include "gm_ticket_native.h"
#include "live_host_state/native_port.h"
#include "live_host_state/monster_apply.h"
#include "root_saved_building.h"
#include <Windows.h>

namespace mt2mp::game::interventions::actors {
namespace {
namespace live=live_host_state;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
struct Backend {
    action_session::Context context;action_session::Binding binding;
    uintptr_t image=uintptr_t(GetModuleHandleW(nullptr));live::NativePort port;
    live::Pin pin;State expected;Locals locals;
    Backend(const action_session::Context& c,const action_session::Binding& b):context(c),binding(b),port(c,b,nullptr){}
    template<class T>bool at(uintptr_t object,size_t offset,T& value,std::string& error){return root_saved_building::bytes(reinterpret_cast<void*>(object),offset,&value,sizeof value,error);}
    bool copy(uintptr_t address,void* value,size_t size,std::string& error){return root_saved_building::bytes(reinterpret_cast<void*>(address),0,value,size,error);}
    bool sample(State& state,std::string& error){return read_current(context,binding,state,locals,error);}
    bool begin(const State& before,const State& target,Receipt&,std::string& error){
        if(context.role!=action_session::Role::Client||!port.read(pin,error)||pin.selected!=0)return fail(error,error.empty()?"Actor authoritative adoption requires actual Client Clock0":error);
        for(const auto& row:target){const auto& local=locals.at(row.target.key);
            for(size_t a=0;a<animation_slots;++a)for(size_t b=0;b<a;++b)if(local.render.indexes[a]>=0&&local.render.indexes[a]==local.render.indexes[b]&&row.clips[a]!=row.clips[b])return fail(error,"Actor authoritative semantic aliases disagree on one local clip key="+row.target.key);}
        expected=before;return port.current_owners(pin,error);
    }
    Row* expected_row(const Row& row){const auto found=std::find_if(expected.begin(),expected.end(),[&](const auto& value){return value.target==row.target;});return found==expected.end()?nullptr:&*found;}
    bool point_read(const Local& owner,Row& actual,Local& physical,std::string& error){
        auto read=[&](uintptr_t p,size_t o,auto& value,std::string& cause){return at(p,o,value,cause);};auto bytes=[&](uintptr_t p,void* value,size_t n,std::string& cause){return copy(p,value,n,cause);};
        auto maximum=[&](uintptr_t p,int32_t n,float& value,std::string& cause){const auto exception=native::maximum(image,p,n,value);return !exception||fail(cause,"Actor adoption original maximum read SEH="+std::to_string(exception));};
        auto clip=[&](const native::Render& render,int32_t n,Clip& value,std::string& cause){const auto exception=native::clip(image,render,n,value);return !exception||fail(cause,"Actor adoption original semantic read SEH="+std::to_string(exception));};
        auto find=[&](uint64_t uid,uintptr_t& pointer,std::string& cause){const auto exception=native::registered(image,uid,pointer);return !exception||fail(cause,"Actor adoption original saved link/item full UID lookup SEH="+std::to_string(exception));};
        return read_row(read,bytes,maximum,clip,image,owner,actual,physical,error)&&read_saved(read,bytes,find,image,owner,actual,physical,error);
    }
    bool current(const Row& row,Row& actual,Local& physical,std::string& error){
        auto* required=expected_row(row);const auto found=locals.find(row.target.key);if(!required||found==locals.end())return fail(error,"Actor adoption current canonical owner absent");
        if(!port.current_owners(pin,error))return false;uintptr_t registered=0;const auto code=native::registered(image,found->second.uid,registered);
        if(code||registered!=found->second.object)return fail(error,"Actor adoption current full UID no longer registered SEH="+std::to_string(code));
        if(!point_read(found->second,actual,physical,error))return false;
        if(actual!=*required||physical!=found->second)return fail(error,"Actor adoption current complete row/render/member changed before native operation key="+row.target.key);
        return true;
    }
    bool after(const Row& row,const Row& wanted,const Local& prior,Receipt&,std::string& error,bool ready_change=false){
        auto* required=expected_row(row);if(!required)return fail(error,"Actor adoption expected row absent after native call");*required=wanted;
        const auto old=locals.at(row.target.key);Row actual;Local physical;
        // The one allowed ready operation may change native swap-last order.
        // Read the new receipt, then prove every unrelated member survives.
        if(ready_change){
            if(!point_read(old,actual,physical,error))return false;
            auto before_members=prior.ready_members,after_members=physical.ready_members;std::erase(before_members,prior.object);std::erase(after_members,prior.object);std::sort(before_members.begin(),before_members.end());std::sort(after_members.begin(),after_members.end());
            auto normalized=physical;normalized.ready_members=prior.ready_members;
            if(actual!=wanted||normalized!=prior||before_members!=after_members)return fail(error,"Actor original SetReady changed another native member/actor/value");
            for(auto& [key,local]:locals)if(local.target.family==Family::Monster&&local.scope.region==prior.scope.region)local.ready_members=physical.ready_members;
        }
        if(!current(row,actual,physical,error))return false;
        return port.current_owners(pin,error);
    }
    bool lifecycle(const Row& before,const Row&,Receipt&,std::string& error){Row actual;Local physical;return current(before,actual,physical,error);}
    bool on_map(const Row& first,const Row&,bool value,Receipt& receipt,std::string& error){
        Row before;Local physical;if(!current(first,before,physical,error))return false;auto wanted=before;wanted.on_map=value;
        receipt.attempted=true;++receipt.attempted_stores;const auto code=native::on_map(image,first.target.family,physical.object,value);
        if(code)return fail(error,"Actor original SetOnMap failed SEH="+std::to_string(code)+"; no replay");++receipt.stores;return after(first,wanted,physical,receipt,error);
    }
    bool ready(const Row& first,const Row&,bool value,Receipt& receipt,std::string& error){
        Row before;Local physical;if(!current(first,before,physical,error))return false;auto wanted=before;auto* monster=std::get_if<ZoneMonster>(&wanted.lifecycle);if(!monster)return fail(error,"Actor original SetReady requires natural Zone Monster");monster->ready=value;
        receipt.attempted=true;++receipt.attempted_stores;const auto code=native::ready(image,physical.object,value);
        if(code)return fail(error,"Actor original SetReady failed SEH="+std::to_string(code)+"; no replay");++receipt.stores;return after(first,wanted,physical,receipt,error,true);
    }
    bool transform(const Row& first,const Row& target,Receipt& receipt,std::string& error){
        Row before;Local physical;if(!current(first,before,physical,error))return false;if(first.target.family==Family::Npc)return fail(error,"Actor NPC pose belongs to same-frame Economy adoption");
        receipt.attempted=true;++receipt.attempted_stores;const auto code=native::transform(image,physical.object,target.pose);
        if(code)return fail(error,"Actor original transform failed SEH="+std::to_string(code)+"; no replay");++receipt.stores;
        struct Spatial {
            Backend& owner;Receipt& receipt;
            bool read_byte(uintptr_t address,uint8_t& value,std::string& cause){return owner.at(address,0,value,cause);}
            bool update_spatial(uintptr_t object,std::string& cause){++receipt.attempted_stores;const auto exception=native::spatial(owner.image,object);if(exception)return fail(cause,"Actor original spatial update SEH="+std::to_string(exception));++receipt.stores;return true;}
            bool compare_byte(uintptr_t address,uint8_t old,uint8_t value,std::string& cause){++receipt.attempted_stores;bool exchanged=false;const auto exception=gm_ticket::native::compare(address,1,old,value,exchanged);if(exception||!exchanged)return fail(cause,"Actor spatial dirty CAS failed SEH="+std::to_string(exception));++receipt.stores;return true;}
        } spatial{*this,receipt};
        if(!live::monsters::clear_spatial_dirty(spatial,physical.object,receipt.attempted,error))return false;
        auto wanted=before;wanted.pose=target.pose;return after(first,wanted,physical,receipt,error);
    }
    bool store(const Row& first,size_t offset,unsigned size,uint32_t old,uint32_t value,Receipt& receipt,std::string& error){
        if(old==value)return true;Row actual;Local physical;if(!current(first,actual,physical,error))return false;
        receipt.attempted=true;++receipt.attempted_stores;bool exchanged=false;const auto code=gm_ticket::native::compare(physical.object+offset,size,old,value,exchanged);
        if(code||!exchanged)return fail(error,"Actor current authoritative scalar CAS failed offset="+std::to_string(offset)+" SEH="+std::to_string(code)+"; no replay");++receipt.stores;return true;
    }
    bool refresh_maxima(const Row& first,std::string& error){
        auto* wanted=expected_row(first);if(!wanted)return fail(error,"Actor StatusEffectData expected row absent");const auto& local=locals.at(first.target.key);
        for(size_t n=0;n<3;++n){float value=0;const auto code=native::maximum(image,local.object,int32_t(n),value);if(code||!std::isfinite(value))return fail(error,"Actor native status-dependent maximum read failed SEH="+std::to_string(code));wanted->maxima[n]=std::bit_cast<uint32_t>(value);}
        Row actual;Local physical;return current(first,actual,physical,error);
    }
    bool status_effect(const Row& first,size_t n,const StatusEffect& target,Receipt& receipt,std::string& error){
        auto* wanted=expected_row(first);if(!wanted||n>=wanted->status_effects.size())return fail(error,"Actor StatusEffectData slot absent");auto& effect=wanted->status_effects[n];const size_t base=0x4e8+n*0x40;
        const auto word=[&](size_t offset,uint32_t& current,uint32_t value){if(current==value)return true;if(!store(first,base+offset,4,current,value,receipt,error))return false;current=value;return refresh_maxima(first,error);};
        const auto number=[&](size_t offset,int32_t& current,int32_t value){if(current==value)return true;if(!store(first,base+offset,4,std::bit_cast<uint32_t>(current),std::bit_cast<uint32_t>(value),receipt,error))return false;current=value;return refresh_maxima(first,error);};
        if(!number(0x18,effect.type,target.type)||!number(0x1c,effect.parameter_int,target.parameter_int)||!word(0x20,effect.parameter_float,target.parameter_float)||!word(0x24,effect.timer,target.timer)||!word(0x28,effect.duration,target.duration))return false;
        auto read=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return at(p,offset,value,cause);};
        auto find=[&](uint64_t uid,uintptr_t& pointer,std::string& cause){const auto code=native::registered(image,uid,pointer);return !code||fail(cause,"Actor status canonical link current UID lookup SEH="+std::to_string(code));};
        const auto& local=locals.at(first.target.key);if(!local.links)return fail(error,"Actor status complete canonical links absent");
        const auto user_pointer=[&](const std::optional<Target>& value,uintptr_t& out){out=0;if(!value)return true;const auto found=local.links->actor_keys.find({uint8_t(value->family),value->key});if(found==local.links->actor_keys.end())return fail(error,"Actor status authoritative user absent from current canonical actors");Target actual;if(!actor_link(read,find,image,*local.links,found->second,actual,error)||actual!=*value)return fail(error,error.empty()?"Actor status authoritative user reciprocal identity differs":error);out=found->second;return true;};
        const auto ability_pointer=[&](const std::string& value,uintptr_t& out){out=0;if(value.empty())return true;const auto found=local.links->ability_keys.find(value);if(found==local.links->ability_keys.end())return fail(error,"Actor status authoritative ability absent from current Character slots");std::string actual;uint64_t uid=0;if(!ability_link(read,find,image,*local.links,found->second,actual,uid,error)||actual!=value)return fail(error,error.empty()?"Actor status authoritative Ability reciprocal identity differs":error);out=found->second;return true;};
        const auto pointer=[&](size_t offset,uintptr_t old,uintptr_t value){Row actual;Local physical;if(!current(first,actual,physical,error))return false;receipt.attempted=true;++receipt.attempted_stores;bool exchanged=false;const auto code=native::compare_pointer(physical.object+base+offset,old,value,exchanged);if(code||!exchanged)return fail(error,"Actor original raw ObjectLink CAS failed SEH="+std::to_string(code)+"; no replay");++receipt.stores;return true;};
        if(effect.user!=target.user){uintptr_t old=0,value=0;if(!user_pointer(effect.user,old)||!user_pointer(target.user,value)||!pointer(0x30,old,value))return false;effect.user=target.user;if(!refresh_maxima(first,error))return false;}
        if(effect.ability!=target.ability){uintptr_t old=0,value=0;if(!ability_pointer(effect.ability,old)||!ability_pointer(target.ability,value)||!pointer(0x38,old,value))return false;effect.ability=target.ability;if(!refresh_maxima(first,error))return false;}
        return true;
    }
    bool maxima(const Row& first,const std::array<uint32_t,3>& target,std::string& error){
        Row actual;Local physical;if(!current(first,actual,physical,error))return false;return actual.maxima==target||fail(error,"Actor original status-dependent maxima differ from Host after complete StatusEffectData adoption");
    }
    bool item(const Row& first,size_t n,const Item& target,Receipt& receipt,std::string& error){
        Row actual;Local physical;if(!current(first,actual,physical,error))return false;auto* wanted=expected_row(first);
        if(n>=physical.items.size()||n>=wanted->abilities.size()||wanted->abilities[n].prototype!=target.prototype)return fail(error,"Actor original Item slot/prototype changed before authoritative write");
        auto& item=wanted->abilities[n];const auto& binding=physical.items[n];
        const auto word=[&](size_t offset,uint32_t old,uint32_t value){if(old==value)return true;Row again;Local current_binding;if(!current(first,again,current_binding,error)||n>=current_binding.items.size()||current_binding.items[n]!=binding)return fail(error,error.empty()?"Actor original Item full UID/weak owner changed":error);receipt.attempted=true;++receipt.attempted_stores;bool exchanged=false;const auto code=gm_ticket::native::compare(binding.pointer+offset,4,old,value,exchanged);if(code||!exchanged)return fail(error,"Actor current Item state/cooldown CAS failed SEH="+std::to_string(code)+"; no replay");++receipt.stores;return true;};
        if(!word(0x38,std::bit_cast<uint32_t>(item.state),std::bit_cast<uint32_t>(target.state)))return false;item.state=target.state;
        if(!word(0x3c,item.cooldown,target.cooldown))return false;item.cooldown=target.cooldown;
        return current(first,actual,physical,error);
    }
    bool scalars(const Row& first,const Row& target,Receipt& receipt,std::string& error){
        if(!adopt_saved(*this,first,target,receipt,error))return false;
        auto* wanted=expected_row(first);if(!wanted)return fail(error,"Actor scalar expected row absent");
        const auto word=[&](size_t offset,uint32_t& current,uint32_t value){if(!store(first,offset,4,current,value,receipt,error))return false;current=value;return true;};
        const auto number=[&](size_t offset,int32_t& current,int32_t value){if(!store(first,offset,4,std::bit_cast<uint32_t>(current),std::bit_cast<uint32_t>(value),receipt,error))return false;current=value;return true;};
        for(size_t n=0;n<3;++n)if(!word(0x750+n*0x28,wanted->values[n],target.values[n])||!word(0x754+n*0x28,wanted->recovery[n],target.recovery[n]))return false;
        if(!number(0x7f8,wanted->action,target.action)||!store(first,0x4ac,1,wanted->dead,target.dead,receipt,error))return false;wanted->dead=target.dead;
        const auto spawn=[&](size_t offset,std::array<float,3>& current,const std::array<float,3>& value){for(size_t n=0;n<3;++n){if(!store(first,offset+n*4,4,std::bit_cast<uint32_t>(current[n]),std::bit_cast<uint32_t>(value[n]),receipt,error))return false;current[n]=value[n];}return true;};
        if(auto* npc=std::get_if<Npc>(&wanted->lifecycle)){const auto& value=std::get<Npc>(target.lifecycle);if(!number(0x834,npc->state,value.state)||!number(0x838,npc->online_state,value.online_state)||!word(0x828,npc->respawn,value.respawn)||!word(0x824,npc->move_delay,value.move_delay)||!word(0x82c,npc->interact_timer,value.interact_timer)||!number(0x830,npc->tick_slot,value.tick_slot)||!spawn(0x818,npc->spawn,value.spawn))return false;}
        if(auto* boss=std::get_if<Boss>(&wanted->lifecycle)){const auto& value=std::get<Boss>(target.lifecycle);if(!number(0x968,boss->phase,value.phase)||!number(0x96c,boss->state,value.state)||!word(0x9ac,boss->respawn,value.respawn)||!spawn(0x970,boss->spawn,value.spawn))return false;}
        if(auto* monster=std::get_if<ZoneMonster>(&wanted->lifecycle)){const auto& value=std::get<ZoneMonster>(target.lifecycle);if(!number(0x824,monster->status,value.status)||!number(0x828,monster->ai,value.ai)||!word(0x910,monster->respawn,value.respawn)||!spawn(0x82c,monster->spawn,value.spawn))return false;}
        Row actual;Local physical;return current(first,actual,physical,error);
    }
    bool animation(const Row& first,const Row& target,size_t semantic,Receipt& receipt,std::string& error){
        Row before;Local physical;if(!current(first,before,physical,error))return false;if(before.clips[semantic]==target.clips[semantic])return true;auto wanted=before;wanted.clips[semantic]=target.clips[semantic];
        if(physical.render.indexes[semantic]<0)return fail(error,"Actor authoritative active semantic clip absent in current native asset");
        receipt.attempted=true;++receipt.attempted_stores;const auto code=native::clip(image,physical.render,int32_t(semantic),static_cast<const Clip&>(wanted.clips[semantic]));
        if(code)return fail(error,"Actor original semantic phase set failed SEH="+std::to_string(code)+"; no replay");++receipt.stores;
        // One local clip can have several semantic names. Preflight requires
        // their complete target phases to agree; one native call changes all.
        for(size_t n=0;n<animation_slots;++n)if(physical.render.indexes[n]==physical.render.indexes[semantic])wanted.clips[n]=target.clips[n];
        return after(first,wanted,physical,receipt,error);
    }
    bool present(const Row& first,const Row&,Receipt& receipt,std::string& error){
        Row before;Local physical;if(!current(first,before,physical,error))return false;uintptr_t slot=0,screen=0,array=0,expected_scene=0,scene=0,camera=0;int32_t count=-1;
        if(!at(image,0x164b420,slot,error)||!slot||!at(slot,0,screen,error)||!screen||!at(screen,0x30,count,error)||count<=0||count>1048576||!at(screen,0x10,array,error)||!array||!at(array,0,expected_scene,error)||!expected_scene)return fail(error,error.empty()?"Actor presentation current local Screen/Scene absent":error);
        auto code=native::scene(image,screen,scene);if(code||scene!=expected_scene||!at(scene,0x40,camera,error)||!camera)return fail(error,error.empty()?"Actor presentation actual Scene/camera differs SEH="+std::to_string(code):error);
        receipt.attempted=true;++receipt.attempted_stores;code=native::present(image,first.target.family,physical.object,physical.render);
        if(code)return fail(error,"Actor original local presentation failed SEH="+std::to_string(code)+"; no replay");++receipt.stores;
        uint8_t visible=2,distance=2;if(!at(physical.skeleton,0xe4,visible,error)||visible>1||!at(physical.object,0x808,distance,error)||distance>1||bool(visible)!=(before.on_map&&bool(distance)))return fail(error,error.empty()?"Actor original local camera visibility readback differs":error);
        return after(first,before,physical,receipt,error);
    }
    bool finish(const State&,const State&,Receipt&,std::string& error){return port.current_owners(pin,error);}
};
}
bool adopt_current(const action_session::Context& context,const action_session::Binding& binding,const State& target,Receipt& receipt,std::string& error){
    receipt={};Backend backend(context,binding);
    try{if(!adopt(backend,target,receipt,error)){receipt.cause=error;receipt.unknown=receipt.attempted;return false;}receipt.confirmed=true;error.clear();return true;}
    catch(const std::exception& exception){receipt.cause=error=std::string("Actor authoritative adoption exception: ")+exception.what();receipt.unknown=receipt.attempted;return false;}
    catch(...){receipt.cause=error="Actor authoritative adoption nonstandard exception";receipt.unknown=receipt.attempted;return false;}
}
}
