#include "npc_quest_runtime_apply.h"
#include <iostream>
#include <map>
#include <stdexcept>
#include <cstring>
namespace q=mt2mp::npc_quest_runtime;
namespace {
unsigned checks=0;void need(bool b,const std::string& e){++checks;if(!b)throw std::runtime_error(e);}
struct Memory:mt2mp::npc_census::Memory {
    std::map<uintptr_t,uint8_t> bytes;
    template<class T>void put(uintptr_t p,const T& v){const auto* b=reinterpret_cast<const uint8_t*>(&v);for(size_t i=0;i<sizeof v;++i)bytes[p+i]=b[i];}
    bool copy(uintptr_t p,void* out,size_t size,std::string& e)override{auto* b=static_cast<uint8_t*>(out);for(size_t i=0;i<size;++i){auto it=bytes.find(p+i);if(it==bytes.end()){e="exact field unreadable";return false;}b[i]=it->second;}return true;}
};
q::Value target(){return {{std::bit_cast<uint32_t>(-0.5f),std::bit_cast<uint32_t>(1.5f),std::bit_cast<uint32_t>(0.0f)},true,-1,-7};}
struct Calls {
    q::Value current;unsigned guards=0,cas=0,ads=0,reads=0;unsigned fail_cas=0,fail_guard=0;bool ads_fail=false,readback_drift=false;
    bool guard(std::string& e){if(++guards==fail_guard){e="native owner generation changed";return false;}return true;}
    bool read(q::Value& out,std::string&){out=current;if(++reads>1&&readback_drift)out.gold_reward++;return true;}
    bool compare32(uintptr_t p,uint32_t old,uint32_t next,std::string& e){
        if(++cas==fail_cas){e="original scalar CAS cause";return false;}
        const auto off=p-1000;uint32_t* value=nullptr;
        if(off>=0x1f8&&off<=0x200)value=&current.rating_bits[(off-0x1f8)/4];
        uint32_t actual=0;if(value)actual=*value;else actual=std::bit_cast<uint32_t>(off==0x88?current.xp_reward:current.gold_reward);
        if(actual!=old){e="actual scalar before differs";return false;}
        if(value)*value=next;else if(off==0x88)current.xp_reward=std::bit_cast<int32_t>(next);else if(off==0x8c)current.gold_reward=std::bit_cast<int32_t>(next);else throw std::runtime_error("wrong scalar offset");return true;
    }
    bool compare8(uintptr_t p,uint8_t old,uint8_t next,std::string& e){
        need(p==1000+0x204,"flag wrote padding or wrong offset");if(++cas==fail_cas){e="original byte CAS cause";return false;}
        if(uint8_t(current.rating_set)!=old){e="actual byte before differs";return false;}current.rating_set=next!=0;return true;
    }
    bool advertise(std::string& e){++ads;if(ads_fail){e="original advertisement exception";return false;}return true;}
};
struct Completion {
    bool armed=true,client=true,member=true,ownership_ok=true;unsigned originals=0,ads=0,reads=0;bool mutate=false,ads_fail=false,owner_changed=false;
    q::Value current=target();
    bool active(bool& c){c=client;return armed;}
    bool owner(uintptr_t,bool& owned,std::string& e){owned=member;if(!ownership_ok||(owner_changed&&reads)){e="exact membership changed";return false;}return true;}
    bool read(uintptr_t,q::Value& out,std::string&){++reads;out=current;return true;}
    bool original(uintptr_t p,uintptr_t path,std::string&){need(p==1000&&path==2000,"rating original ABI differs");++originals;current=target();return true;}
    bool advertise(uintptr_t p,std::string& e){need(p==1000,"advertisement receiver differs");++ads;if(ads_fail){e="exact advertisement cause";return false;}if(mutate)++current.gold_reward;return true;}
};
}
int main(){try{std::string e;const auto wanted=target();
    Memory memory;constexpr uintptr_t image=0x100000,quest=1000;
    memory.put(quest,image+0x174ac10);memory.put(quest+0x1e0,image+0x1787870);
    for(size_t i=0;i<3;++i)memory.put(quest+0x1f8+i*4,wanted.rating_bits[i]);memory.put(quest+0x204,uint8_t(1));memory.put(quest+0x88,wanted.xp_reward);memory.put(quest+0x8c,wanted.gold_reward);
    q::Value read;need(q::read(memory,image,quest,read,e)&&read==wanted,e);
    for(int mode=0;mode<4;++mode){auto bad=memory;if(mode==0)bad.put(quest,image+1);if(mode==1)bad.put(quest+0x1e0,image+1);if(mode==2)bad.put(quest+0x204,uint8_t(2));if(mode==3)bad.put(quest+0x1f8,uint32_t(0x7fc00000));need(!q::read(bad,image,quest,read,e)&&!e.empty(),"actual reader admitted bad class/flag/nonfinite");}
    need(!q::read(memory,image,0,read,e)&&!e.empty(),"null reader cause lost");
    Calls good;auto receipt=q::apply(good,quest,{},wanted);need(receipt.confirmed&&!receipt.unknown&&receipt.stores==5&&receipt.advertisement_returned&&good.ads==1&&good.current==wanted,"complete scalar Apply failed: "+receipt.cause);
    Calls noop;noop.current=wanted;receipt=q::apply(noop,quest,wanted,wanted);need(receipt.confirmed&&!receipt.attempted&&!noop.cas&&!noop.ads,"no-op mutated");
    Calls stale;stale.current.gold_reward=9;receipt=q::apply(stale,quest,{},wanted);need(!receipt.confirmed&&!receipt.attempted&&!stale.cas&&!receipt.cause.empty(),"stale complete prior mutated");
    for(unsigned f=1;f<=5;++f){Calls c;c.fail_cas=f;receipt=q::apply(c,quest,{},wanted);need(receipt.unknown&&!receipt.confirmed&&receipt.stores==f-1&&receipt.cause.find("original")!=std::string::npos&&!c.ads,"partial scalar/flag failure lost Unknown/first cause");}
    for(unsigned f=1;f<=8;++f){Calls c;c.fail_guard=f;receipt=q::apply(c,quest,{},wanted);need(!receipt.confirmed&&receipt.unknown==(f>2)&&receipt.cause=="native owner generation changed","owner guard or Unknown wrong");}
    Calls ads;ads.ads_fail=true;receipt=q::apply(ads,quest,{},wanted);need(receipt.unknown&&receipt.advertisement_attempted&&!receipt.advertisement_returned&&receipt.cause=="original advertisement exception","advertisement failure looked confirmed");
    Calls drift;drift.readback_drift=true;receipt=q::apply(drift,quest,{},wanted);need(receipt.unknown&&!receipt.confirmed&&receipt.cause.find("readback")!=std::string::npos,"complete final readback not checked");
    for(uintptr_t caller:{image+0x4d8f13,image+0x4d92a4}){Completion c;auto r=q::complete(c,quest,2000,caller,image);need(r.confirmed&&r.suppressed&&c.ads==1&&!c.originals&&r.before==r.after,"normal Client Tick clone did not preserve advertisement and authoritative value");}
    Completion host;host.client=false;host.current={};auto r=q::complete(host,quest,2000,image+0x4d92a4,image);need(r.confirmed&&!r.suppressed&&host.originals==1&&!host.ads&&r.before!=r.after,"Host rating did not execute original once");
    for(unsigned mode=0;mode<5;++mode){Completion c;if(mode==0)c.ownership_ok=false;if(mode==1)c.owner_changed=true;if(mode==2)c.ads_fail=true;if(mode==3)c.mutate=true;r=q::complete(c,quest,2000,image+(mode==4?0x4d92a5:0x4d92a4),image);need(!r.confirmed&&!r.cause.empty()&&!c.originals,"wrong owner/caller/effect allowed Client original rating");}
    for(unsigned mode=0;mode<2;++mode){Completion c;if(mode)c.member=false;else c.armed=false;r=q::complete(c,quest,2000,0,image);need(r.confirmed&&c.originals==1&&!c.ads,"unarmed/foreign original path did not forward once");}
    std::cout<<checks<<" actual Quest runtime reader/scalar/completion checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
