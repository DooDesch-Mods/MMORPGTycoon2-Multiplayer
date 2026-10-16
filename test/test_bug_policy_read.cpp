#include "developer_staff/bug_policy_read.h"
#include <iostream>
#include <map>
#include <stdexcept>
namespace policy=mt2mp::game::developer_staff::bug_policy;
using mt2mp::game::developer_staff::Capture;
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;uintptr_t failure=0;int leaf_overreads=0;
    template<class T>void put(uintptr_t p,const T& input){const auto* data=reinterpret_cast<const unsigned char*>(&input);for(size_t i=0;i<sizeof input;++i)bytes[p+i]=data[i];}
    void text(uintptr_t p,const std::string& input,uintptr_t data){put(p,data);put(p+8,uint64_t(input.size()));for(size_t i=0;i<=input.size();++i)bytes[data+i]=i<input.size()?static_cast<unsigned char>(input[i]):0;}
    bool copy(uintptr_t p,void* out,size_t n,std::string& e){
        if(p==failure){e="original ReadMemory failure";return false;}
        if(p==0x80000+0x2d0)++leaf_overreads;
        for(size_t i=0;i<n;++i){auto found=bytes.find(p+i);if(found==bytes.end()){e="unmapped synthetic native address="+std::to_string(p+i);return false;}static_cast<unsigned char*>(out)[i]=found->second;}return true;
    }
};
Memory memory;Capture capture;
struct Address {static constexpr uintptr_t image=0x40000000,mode=0x20000,window=0x30000,tabs=0x40000,view=0x50000,page=0x60000,inner=0x70000,checkbox=0x80000,value=0x90000,lounge=0xb0000,server=0xc0000;};
int cases=0;void need(bool b,const std::string& e){++cases;if(!b)throw std::runtime_error(e);}
void reset(){
    memory={};capture={};capture.image=Address::image;capture.pin.origin.mode=reinterpret_cast<void*>(Address::mode);capture.local.lounge=Address::lounge;
    memory.put(Address::image+0x13df290,Address::lounge);memory.put(Address::image+0x13df430,Address::server);
    memory.put(Address::lounge,Address::image+0x16de6c0);memory.put(Address::lounge+0x38,Address::image+0x16de7a0);memory.put(Address::server,Address::image+0x1702a40);
    for(size_t i=0;i<6;++i)memory.put(Address::lounge+0x50+i,uint8_t(0));memory.put(Address::server+0x18,int32_t(3));memory.put(Address::server+0x1c,int32_t(3));
    memory.put(Address::mode+0xa0,uintptr_t(0xd0000));memory.put(Address::mode+0xa8,int32_t(1));memory.put(Address::mode+0xac,int32_t(1));memory.put(0xd0000,Address::window);
    memory.put(Address::window,Address::image+0x1722e60);memory.text(Address::window+0x1b0,"development",0xe0000);memory.put(Address::window+0x318,uintptr_t(0));
    memory.put(Address::window+0x540,Address::tabs);memory.put(Address::window+0x45f,uint8_t(1));memory.put(Address::window+0x460,uint8_t(0));
    memory.put(Address::tabs,Address::image+0x17015b0);memory.put(Address::tabs+0x3b8,Address::view);memory.put(Address::tabs+0x280,Address::window);
    memory.put(Address::view,Address::image+0x16dfe90);memory.put(Address::view+0x280,Address::window);memory.text(Address::view+0x3e8,"{dev_tab_policies}",0xe0100);
    memory.put(Address::view+0x410,Address::page);memory.put(Address::view+0x418,Address::page);memory.put(Address::view+0x408,int32_t(0));
    for(const auto p:{Address::page,Address::inner,Address::checkbox})memory.put(p+0x280,Address::window);
    memory.put(Address::page,Address::image+0x1705ee0);memory.text(Address::page+0x1b0,"{dev_tab_policies}",0xe0200);memory.put(Address::page+0x278,Address::view);
    memory.put(Address::page+0x2d0,uintptr_t(0xd0100));memory.put(Address::page+0x2d8,int32_t(1));memory.put(Address::page+0x2dc,int32_t(1));memory.put(0xd0100,Address::inner);
    memory.put(Address::inner,Address::image+0x1745a80);memory.put(Address::inner+0x278,Address::page);memory.put(Address::inner+0x2d0,uintptr_t(0xd0200));memory.put(Address::inner+0x2d8,int32_t(1));memory.put(Address::inner+0x2dc,int32_t(2));memory.put(0xd0200,Address::checkbox);
    memory.put(Address::checkbox,Address::image+0x16d98c0);memory.put(Address::checkbox+0x278,Address::inner);memory.put(Address::checkbox+0x3c8,Address::value);memory.put(Address::checkbox+0x3c0,uintptr_t(0));memory.put(Address::checkbox+0x2c8,uintptr_t(0));
    memory.put(Address::checkbox+0x328,uintptr_t(0xf0000));memory.put(Address::checkbox+0x320,uintptr_t(0xf0100));memory.put(Address::checkbox+0x174,uint8_t(1));memory.put(Address::checkbox+0x345,uint8_t(1));
    memory.put(Address::checkbox+0x344,uint8_t(0));memory.put(Address::checkbox+0x346,uint8_t(0));memory.put(Address::checkbox+0x350,uint64_t(0));memory.put(Address::checkbox+0x3b8,int32_t(13));
    memory.text(Address::value+0x18,"fixCBugsOnline",0xe0300);memory.put(Address::value+8,Address::lounge+0x38);memory.put(Address::value+0x38,Address::lounge+0x52);memory.put(Address::value+0x40,uint8_t(0));memory.put(Address::value+0x10,uint8_t(0));
}
int main(){try{
    std::string e;policy::Snapshot state;policy::Control out;
    const auto copy=[](auto p,auto target,auto n,auto& error){return memory.copy(p,target,n,error);};
    reset();need(policy::read_policy(copy,capture,state,e),"actual policy sample failed: "+e);
    const bool bound=policy::read_control(copy,state,policy::Scope::Online,out,false,e);
    need(bound&&out.checkbox==Address::checkbox&&out.field==Address::lounge+0x52,"actual bound online control failed: "+e);
    need(memory.leaf_overreads==0,"checkbox was treated as a View and read through its label");
    reset();memory.text(Address::value+0x18,"fixCBugsOffline",0xe0300);memory.put(Address::value+0x38,Address::lounge+0x55);need(policy::read_policy(copy,capture,state,e)&&policy::read_control(copy,state,policy::Scope::Offline,out,false,e),"offline control not bound to exact offline field");
    for(int kind=0;kind<14;++kind){reset();need(policy::read_policy(copy,capture,state,e),"base sample");
        if(kind==0)memory.put(Address::checkbox+0x280,uintptr_t(1));if(kind==1)memory.put(Address::checkbox+0x278,Address::page);if(kind==2)memory.put(Address::value+8,uintptr_t(1));if(kind==3)memory.put(Address::value+0x38,Address::lounge+0x55);
        if(kind==4)memory.put(Address::checkbox+0x345,uint8_t(0));if(kind==5)memory.put(Address::checkbox+0x344,uint8_t(1));if(kind==6)memory.put(Address::checkbox+0x350,uint64_t(8));if(kind==7)memory.put(Address::checkbox+0x2c8,uintptr_t(1));
        if(kind==8)memory.put(Address::checkbox+0x346,uint8_t(1));if(kind==9)memory.put(Address::value+0x40,uint8_t(1));if(kind==10)memory.put(Address::window+0x45f,uint8_t(0));if(kind==11)memory.put(Address::view+0x418,uintptr_t(0));
        if(kind==12)memory.text(Address::value+0x18,"fixABugsOnline",0xe0300);if(kind==13)memory.put(Address::inner+0x2d8,int32_t(513));
        need(!policy::read_control(copy,state,policy::Scope::Online,out,false,e)&&!e.empty(),"foreign/disabled/wrong-value control admitted");}
    reset();memory.put(Address::inner+0x2d8,int32_t(2));memory.put(0xd0208,Address::checkbox);need(policy::read_policy(copy,capture,state,e)&&!policy::read_control(copy,state,policy::Scope::Online,out,false,e)&&e.find("duplicate")!=std::string::npos,"duplicate child accepted");
    reset();const auto owned=memory.bytes;
    for(const auto& [p,byte]:owned)if(p>=Address::checkbox&&p<Address::checkbox+0x400)memory.bytes[p+0x1000]=byte;
    memory.put(Address::inner+0x2d8,int32_t(2));memory.put(0xd0208,Address::checkbox+0x1000);
    need(policy::read_policy(copy,capture,state,e)&&!policy::read_control(copy,state,policy::Scope::Online,out,false,e)&&e.find("ambiguous")!=std::string::npos,"two distinct native checkboxes bound to same property accepted");
    reset();memory.failure=Address::value+8;need(policy::read_policy(copy,capture,state,e)&&!policy::read_control(copy,state,policy::Scope::Online,out,false,e)&&e=="original ReadMemory failure","read cause overwritten");
    reset();memory.put(Address::lounge+0x52,uint8_t(2));need(!policy::read_policy(copy,capture,state,e)&&e.find("bool")!=std::string::npos,"nonboolean native policy accepted");
    reset();memory.put(Address::checkbox+0x344,uint8_t(1));need(policy::read_policy(copy,capture,state,e)&&policy::read_control(copy,state,policy::Scope::Online,out,true,e),"exact pressed repin failed");
    for(const int32_t transition:{1,2,-1}){
        reset();memory.put(Address::view+0x408,transition);policy::PageWitness page;
        need(policy::read_policy(copy,capture,state,e)&&!policy::read_control(copy,state,policy::Scope::Online,out,false,e,&page),"pending native transition accepted");
        const auto fields=policy::page_witness(page);
        need(fields.at("sampled")==true&&fields.at("next_matches_current")==true&&fields.at("transition")==transition&&e.find("transition="+std::to_string(transition))!=std::string::npos,"pending transition witness lost");
    }
    reset();policy::PageWitness page;
    need(policy::read_policy(copy,capture,state,e)&&policy::read_control(copy,state,policy::Scope::Online,out,false,e,&page),"original settled target/current pair rejected");
    need(policy::page_witness(page)==nlohmann::json({{"sampled",true},{"window_visible",1},{"requested_policies",true},{"current_page_present",true},{"next_matches_current",true},{"transition",0}}),"settled native page witness differs");
    reset();memory.put(Address::view+0x418,Address::inner);
    need(policy::read_policy(copy,capture,state,e)&&!policy::read_control(copy,state,policy::Scope::Online,out,false,e,&page)&&!page.next_matches_current,"different target page accepted");
    reset();memory.put(Address::view+0x410,uintptr_t(0));memory.put(Address::view+0x418,uintptr_t(0));
    need(policy::read_policy(copy,capture,state,e)&&!policy::read_control(copy,state,policy::Scope::Online,out,false,e,&page)&&!page.current_page_present,"two null pages admitted as settled");
    reset();memory.text(Address::view+0x3e8,"{dev_tab_staff}",0xe0100);
    need(policy::read_policy(copy,capture,state,e)&&!policy::read_control(copy,state,policy::Scope::Online,out,false,e,&page)&&!page.requested_policies,"foreign requested page accepted");
    reset();memory.failure=Address::view+0x418;
    need(policy::read_policy(copy,capture,state,e)&&!policy::read_control(copy,state,policy::Scope::Online,out,false,e,&page)&&e=="original ReadMemory failure","page read failure cause overwritten");
    need(policy::page_witness(page)==nlohmann::json({{"sampled",false}}),"incomplete page read fabricated measured fields");
    std::cout<<"PASS "<<cases<<" actual native policy/control reader assertions; synthetic owned memory only\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
