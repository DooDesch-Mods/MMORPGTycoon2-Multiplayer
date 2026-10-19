#include "control_native_runtime.h"
#include "../intervention_census.h"
#include "../intervention_actor_native.h"
#include "../live_host_state/native_port.h"
#include "../session_provider.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/core/hash.h"
#include <Windows.h>

namespace mt2mp::control_input {
namespace {
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
template<class T>bool read(::placement_ui_probe::ProcessMemory& memory,uintptr_t pointer,size_t offset,T& out,std::string& error){
    if(!pointer||pointer>UINTPTR_MAX-offset)return fail(error,"Control native field extent invalid");
    return memory.copy(pointer+offset,&out,sizeof out,error);
}
}
bool NativePort::checked(uint32_t code,const char* operation,std::string& error){return !code||fail(error,std::string("Control original ")+operation+" failed SEH="+std::to_string(code));}
bool NativePort::code(std::string& error){
    struct Pin{uintptr_t rva;size_t bytes;const char* sha;};
    static constexpr Pin pins[]={
        {0x9ffd80,368,"e778dfc7316eeebf93de7805a5aec4f913318db2323f16739ee202b09366aa95"},
        {0x350710,32,"c7ef1dc8f5cd463666e34fa2884d3541d619455b05ae71c26b7c2476134065b9"},
        {0x298a30,304,"29bfd42ab93c955e0115ce9310a37103116375eb3be2304f111648fc679ea344"},
        {0x298de0,32,"f21145ab32faa7461485a10c32ab77dea5cbaaf9c453a15b20eb3b1c02b76a86"},
        {0x32ab80,48,"5da4b673fa8742073f3af60317e3f5ddab22a1f7206186b2dfae0359be586bab"},
        {0x39cb50,128,"6dd347e5a3295fa5a152781dc02ea7590a5036f1dbb626a1f6503c392e80a7f5"},
        {0x9fb4d0,48,"043e23cdfd2b075bbec932d06dbbe95e946e8c30d4b2e7c4989c00272ca9e8d7"},
        {0x3b0530,176,"5c9539bf6fe56a935d6706c8eec81eb39aad0dd0a18e96ca89c8987e43fa527a"},
        {0x3b07d0,176,"59feed58a5d1dbbf9c2201bcf2d639cf5f0b4c3ffc2c4fe538b2c4547f105161"}};
    for(const auto& pin:pins){std::vector<unsigned char> bytes(pin.bytes);
        if(!memory_.copy(image_+pin.rva,bytes.data(),bytes.size(),error))return false;
        if(mt2mp::sha256(bytes)!=pin.sha)return fail(error,"Control original leaf fingerprint differs RVA="+std::to_string(pin.rva));}
    return true;
}
bool NativePort::resolve(const Target& target,LocalBinding& out,uint64_t& uid,std::string& error){
    action_session::Context current;uint32_t thread{};std::string sha;
    if(!action_session::selected_session().current(current,error)||current!=context_||(!replica_&&context_.role!=action_session::Role::Host)||
       !game::native::read_resolver_receipt(image_,thread,sha,error)||thread!=GetCurrentThreadId()||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(error,error.empty()?"Control requires current Host/Mainthread/exact executable":error);
    game::interventions::Census census;if(!game::interventions::census(context_,binding_,census,error))return false;
    const auto found=std::find_if(census.actors.begin(),census.actors.end(),[&](const auto& row){return row.target==target;});
    if(found==census.actors.end())return fail(error,"Control canonical actor is absent from current census key="+target.key);
    const auto actor=found->object;uintptr_t registered{};
    if(!checked(game::interventions::actors::native::registered(image_,found->uid,registered),"FindUID",error)||registered!=actor)return fail(error,error.empty()?"Control current full UID differs":error);
    using Family=game::interventions::Family;
    const uintptr_t ap=target.family==Family::Toon?0x1744e90:target.family==Family::Npc?0x1742980:target.family==Family::Boss?0x1743040:0x16c0770;
    const uintptr_t transform=target.family==Family::Toon?0x1745378:target.family==Family::Npc?0x1742e58:target.family==Family::Boss?0x1743530:0x16c0c58;
    const uintptr_t notify=target.family==Family::Toon?0x3e82a0:target.family==Family::Npc?0x3c2de0:0x39e750;
    uintptr_t actual{},embedded{},callback{},controlled{},character{},model{};uint64_t actual_uid{};
    if(!::mt2mp::control_input::read(memory_,actor,0,actual,error)||actual!=image_+ap||!::mt2mp::control_input::read(memory_,actor,0x18,actual_uid,error)||actual_uid!=found->uid||
       !::mt2mp::control_input::read(memory_,actor,0x30,embedded,error)||embedded!=image_+transform||!::mt2mp::control_input::read(memory_,embedded,0x38,callback,error)||callback!=image_+notify||
       !::mt2mp::control_input::read(memory_,actual,0x3f0,controlled,error)||controlled!=image_+(target.family==Family::Toon?0x3e4d30:0x39aa40)||
       !::mt2mp::control_input::read(memory_,actor,0x6f0,character,error)||!character||!::mt2mp::control_input::read(memory_,character,0,model,error)||!model)return fail(error,error.empty()?"Control actor class/full UID/model/transform callback differs":error);
    if(!census.bosses.world.map)return fail(error,"Control current Map singleton absent");
    out={image_,actor,uintptr_t(census.bosses.world.map)};uid=found->uid;return code(error);
}
bool NativePort::read(const Target& target,ActorState& out,std::string& error){
    out={};LocalBinding local;if(!resolve(target,local,out.uid,error))return false;out.pointer=local.actor;
    uint8_t controlled=2,moving=2,dead=2,on_map=2;uintptr_t live_target{};
    if(!checked(sample_native(local,out.sample),"sample",error)||
       !::mt2mp::control_input::read(memory_,local.actor,0x6c1,controlled,error)||controlled>1||
       !::mt2mp::control_input::read(memory_,local.actor,0x7f4,moving,error)||moving>1||
       !::mt2mp::control_input::read(memory_,local.actor,0x4ac,dead,error)||dead>1||
       !::mt2mp::control_input::read(memory_,local.actor,0x368,on_map,error)||on_map>1)return fail(error,error.empty()?"Control original actor boolean outside domain":error);
    const auto exception=read_target_native(local,live_target);if(!checked(exception,"target read",error))return false;
    if(live_target){game::interventions::Census census;if(!game::interventions::census(context_,binding_,census,error))return false;
        const auto found=std::find_if(census.actors.begin(),census.actors.end(),[&](const auto& row){return row.object==live_target;});
        if(found==census.actors.end())return fail(error,"Control live target lacks current canonical identity");out.target=found->target;}
    out.controlled=controlled;out.moving=moving;out.dead=dead;out.on_map=on_map;
    return finite(out.sample.position)&&std::isfinite(out.sample.native_speed)&&out.sample.native_speed>=0&&std::isfinite(out.sample.action_timer)||fail(error,"Control actor sample is nonfinite");
}
bool NativePort::target_exists(const Target& target,std::string& error){LocalBinding local;uint64_t uid{};return resolve(target,local,uid,error);}
bool NativePort::scaled_dt(float frame_dt,float& out,std::string& error){
    if(context_.role!=action_session::Role::Host)return fail(error,"Control movement requires Host Clock authority");
    game::live_host_state::NativePort port(context_,binding_,nullptr);game::live_host_state::Pin pin;
    if(!port.read(pin,error))return false;float first{},second{};
    if(!::mt2mp::control_input::read(memory_,pin.clock,0x5c,first,error)||!::mt2mp::control_input::read(memory_,pin.clock,0x58,second,error))return false;
    out=(first*second)*frame_dt;
    return std::isfinite(out)&&out>=0||fail(error,"Control Host Clock product is invalid");
}
bool NativePort::propose(const Sample& sample,const Motion& motion,const BasisInput& input,float dt,Proposal& out,uint32_t& exception,std::string& error){
    const bool accepted=propose_native(LocalBinding{image_},sample,motion,input,dt,out,exception);
    return accepted||fail(error,"Control original-normalized motion proposal failed SEH="+std::to_string(exception));
}
bool NativePort::jump(const Target& target,uint32_t& exception,std::string& error){LocalBinding local;uint64_t uid{};if(!resolve(target,local,uid,error))return false;exception=jump_native(local);return checked(exception,"jump",error);}
bool NativePort::constrain(const Target& target,Vec3& position,float& height,uint32_t& exception,std::string& error){LocalBinding local;uint64_t uid{};if(!resolve(target,local,uid,error))return false;exception=constrain_native(local,position,height);return checked(exception,"NavMesh and height",error);}
bool NativePort::position(const Target& target,const Vec3& position,uint32_t& exception,std::string& error){LocalBinding local;uint64_t uid{};if(!resolve(target,local,uid,error))return false;exception=position_native(local,position);return checked(exception,"position",error);}
bool NativePort::facing(const Target& target,const Motion& motion,uint32_t& exception,std::string& error){LocalBinding local;uint64_t uid{};if(!resolve(target,local,uid,error))return false;exception=facing_native(local,motion);return checked(exception,"orientation/moving",error);}
bool NativePort::set_target(const Target& target,const std::optional<Target>& selected,uint32_t& exception,std::string& error){
    LocalBinding local,other;uint64_t uid{},target_uid{};if(!resolve(target,local,uid,error)||(selected&&!resolve(*selected,other,target_uid,error)))return false;
    uintptr_t actual{};exception=target_native(local,other.actor,actual);
    return checked(exception,"target assignment",error)&&(actual==other.actor||fail(error,"Control original target assignment readback differs"));
}
bool NativePort::controlled(const Target& target,bool value,StepReceipt& receipt,std::string& error){
    receipt={};LocalBinding local;uint64_t uid{};ActorState before;
    if(!read(target,before,error)||!resolve(target,local,uid,error)){receipt.cause=error;return false;}
    if(before.controlled==value){receipt.cause=error="Control acquire/release before state differs";return false;}
    receipt.attempted=true;receipt.exception=controlled_native(local,value);ActorState after;
    if(!checked(receipt.exception,"controlled state",error)||!read(target,after,error)||after.pointer!=before.pointer||after.uid!=before.uid||after.controlled!=value){receipt.unknown=true;receipt.cause=error.empty()?"Control original acquire/release identity/readback differs":error;error=receipt.cause;return false;}
    receipt.confirmed=true;error.clear();return true;
}
bool NativePort::replica_flags(const Target& target,bool controlled,bool moving,StepReceipt& receipt,std::string& error){
    receipt={};if(!replica_||context_.role!=action_session::Role::Client)return fail(error,"Control flag adoption requires Client authoritative Frame scope");
    ActorState before;if(!read(target,before,error))return false;
    for(const auto field:std::array<std::pair<size_t,bool>,2>{{{0x6c1,controlled},{0x7f4,moving}}}){
        LocalBinding local;uint64_t uid{};if(!resolve(target,local,uid,error)){receipt.unknown=receipt.attempted;receipt.cause=error;return false;}
        const bool prior=field.first==0x6c1?before.controlled:before.moving;if(prior==field.second)continue;
        uint8_t observed{};if(!::mt2mp::control_input::read(memory_,local.actor,field.first,observed,error)||observed!=uint8_t(prior)){receipt.unknown=receipt.attempted;receipt.cause=error.empty()?"Control Frame flag changed before store":error;error=receipt.cause;return false;}
        receipt.attempted=true;SIZE_T written{};const uint8_t value=uint8_t(field.second);
        if(!WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(local.actor+field.first),&value,1,&written)||written!=1){receipt.unknown=true;receipt.cause=error="Control Frame native flag store failed";return false;}
    }
    ActorState after;if(!read(target,after,error)||after.pointer!=before.pointer||after.uid!=before.uid||after.controlled!=controlled||after.moving!=moving){receipt.unknown=receipt.attempted;receipt.cause=error.empty()?"Control Frame complete flag readback differs":error;error=receipt.cause;return false;}
    receipt.confirmed=true;error.clear();return true;
}
namespace {
uint32_t invoke_item(uintptr_t image,uintptr_t item,bool activate)noexcept{
    __try{reinterpret_cast<void(*)(uintptr_t)>(image+(activate?0x207820:0x2078a0))(item);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
namespace {
struct AbilityBinding {
    uintptr_t actor{},data{},item{},type{},proxy{};
    uint64_t actor_uid{},item_uid{};int32_t count{},capacity{},state{};uint32_t cooldown{};
};
bool ability_binding(::placement_ui_probe::ProcessMemory& memory,uintptr_t image,const game::interventions::Census& census,const LocalBinding& local,uint64_t uid,int32_t ordinal,const std::string& prototype,AbilityBinding& out,std::string& error){
    auto at=[&](uintptr_t pointer,size_t offset,auto& value){return read(memory,pointer,offset,value,error);};
    uintptr_t set_ap{},owner{},item_ap{},weak_ap{},cast{},slot{},registered{};int32_t references{};
    out.actor=local.actor;out.actor_uid=uid;
    if(!at(local.actor,0x7a8,set_ap)||set_ap!=image+0x16e3c30||!at(local.actor,0x7d8,owner)||owner!=local.actor||
       !at(local.actor,0x7b0,out.data)||!at(local.actor,0x7b8,out.count)||!at(local.actor,0x7bc,out.capacity)||out.count<0||out.count>10000||out.capacity<out.count||ordinal<0||ordinal>=out.count||
       !out.data||out.data>UINTPTR_MAX-size_t(out.count)*8||!at(out.data,size_t(ordinal)*8,out.item)||!at(out.item,0,item_ap)||item_ap!=image+0x17437f0||
       !at(out.item,0x18,out.item_uid)||!out.item_uid||!at(out.item,0x80,owner)||owner!=local.actor||!at(out.item,0x40,weak_ap)||weak_ap!=image+0x17117b0||
       !at(out.item,0x68,out.type)||!at(out.item,0x30,cast)||cast!=out.type||!at(out.item,0x70,out.proxy)||!at(out.proxy,0,slot)||slot!=out.type+0x38||!at(out.proxy,8,references)||references<1||
       !at(out.item,0x38,out.state)||out.state<0||!at(out.item,0x3c,out.cooldown)||!std::isfinite(std::bit_cast<float>(out.cooldown)))return fail(error,error.empty()?"Control actual AbilitySet/Item/weak lifetime/scalar domain differs":error);
    const auto exception=game::interventions::actors::native::registered(image,out.item_uid,registered);
    if(exception||registered!=out.item)return fail(error,"Control actual Item full UID differs SEH="+std::to_string(exception));
    const auto found=census.owners.find(out.type);if(found==census.owners.end()||found->second!=prototype)return fail(error,"Control current canonical Ability prototype differs");
    return true;
}
}
bool NativePort::ability(const Target& target,int32_t ordinal,const std::string& prototype,bool activate,StepReceipt& receipt,std::string& error){
    receipt={};LocalBinding local;uint64_t uid{};if(!resolve(target,local,uid,error))return false;
    game::interventions::Census census;if(!game::interventions::census(context_,binding_,census,error))return false;
    AbilityBinding before;if(!ability_binding(memory_,image_,census,local,uid,ordinal,prototype,before,error))return false;
    receipt.attempted=true;receipt.exception=invoke_item(image_,before.item,activate);
    if(!checked(receipt.exception,"ability activation",error)||!resolve(target,local,uid,error)||!game::interventions::census(context_,binding_,census,error)){receipt.unknown=true;receipt.cause=error;return false;}
    AbilityBinding after;if(!ability_binding(memory_,image_,census,local,uid,ordinal,prototype,after,error)||before.actor!=after.actor||before.actor_uid!=after.actor_uid||before.data!=after.data||before.count!=after.count||before.capacity!=after.capacity||before.item!=after.item||before.item_uid!=after.item_uid||before.type!=after.type||before.proxy!=after.proxy){receipt.unknown=true;receipt.cause=error.empty()?"Control original ability complete identity/readback differs":error;error=receipt.cause;return false;}
    receipt.confirmed=true;error.clear();return true;
}
}
