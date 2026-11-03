#include <windows.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
namespace test {
unsigned checks=0,setups=0,destroys=0,proposal_destroys=0;
bool connected=true,ready=true,fault=false,code_ok=true,census_ok=true,started=true,retiring=false;std::string cause;
struct State {uintptr_t image=0;DWORD thread=::GetCurrentThreadId();}state;
void need(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
bool fail(std::string& error,const std::string& text){error=text;return false;}
bool reject(const std::string& text){cause=text;return false;}
struct Memory {};
template<class T>bool field(Memory&,uintptr_t object,size_t offset,T& value,std::string& error){
    if(!object)return fail(error,"null fixture native read");std::memcpy(&value,reinterpret_cast<const void*>(object+offset),sizeof value);return true;
}
bool code(Memory&,std::string& error){return code_ok||fail(error,"fixture original code mismatch");}
namespace net::session_wire {bool connected(){return test::connected;}}
namespace a {
struct Context {std::string snapshot_uuid="fixture-snapshot";};struct Binding {};
struct Session {bool current(Context&,std::string&){return true;}}session;
Session& selected_session(){return session;}
}
namespace typed_action_runtime {
bool read_binding(const std::string&,a::Binding&,std::string&){return true;}
struct Status {bool value(const std::string& key,bool fallback)const{return key=="started"?started:(key=="lobby_retiring"?retiring:fallback);}};
Status controller_status(){return {};}
}
namespace character_definition_identity {
struct Definition {uintptr_t object=0;bool deleted=false;};
struct Census {std::vector<Definition> definitions;};
}
namespace character_membership_reader {struct Users {};}
character_definition_identity::Census current;
namespace character_actor_records {
bool read_current(const a::Context&,const a::Binding&,character_definition_identity::Census& out,character_membership_reader::Users&,std::string& error){
    if(!census_ok)return fail(error,"fixture canonical owner changed");out=current;return true;
}
}
void* _ReturnAddress(){return reinterpret_cast<void*>(0x570b63);}
#include "ability_apply_tails_actual.inc"
#include "ability_apply_disposal_actual.inc"
alignas(8) std::array<unsigned char,0x608> window{};
alignas(8) std::array<unsigned char,0x210> old_live{},new_live{},old_draft{},new_draft{};
alignas(8) std::array<unsigned char,0x140> old_owner{},new_owner{};
alignas(8) std::array<unsigned char,16> old_proxy{},new_proxy{};
std::array<uintptr_t,1> old_members{},new_members{};
std::map<uint32_t,uintptr_t> registry;
constexpr uint64_t old_uid=0x1111000000000042ULL,new_uid=0x2222000000000043ULL;
uintptr_t ptr(const auto& x){return reinterpret_cast<uintptr_t>(x.data());}
template<class T>void put(auto& x,size_t offset,T value){std::memcpy(x.data()+offset,&value,sizeof value);}
template<class T=uintptr_t>T get(const auto& x,size_t offset){T value{};std::memcpy(&value,x.data()+offset,sizeof value);return value;}
uintptr_t find_uid(uint64_t uid){if(!uid)return 0;const auto f=registry.find(static_cast<uint32_t>(uid));return f==registry.end()?0:f->second;}
uint32_t proposal_clear_copy(uintptr_t draft,uintptr_t live,uint64_t uid){return apply_relinquish_copy(draft,live,uid,0x171c930,find_uid);}
#include "ability_proposal_actual.inc"
void native_delete_proposal(uintptr_t draft){
    if(!draft)return;++proposal_destroys;uint64_t uid=0;std::memcpy(&uid,reinterpret_cast<void*>(draft+0x18),8);
    registry.erase(static_cast<uint32_t>(uid));std::memset(reinterpret_cast<void*>(draft),0xdd,0x210);
}
void native_setup(void* outer,void* target){
    ++setups;need(outer==window.data(),"wrong original setup receiver");native_delete_proposal(get(window,0x510));
    const auto live=reinterpret_cast<uintptr_t>(target);std::memcpy(new_draft.data(),target,0x210);put(new_draft,0x40,uintptr_t{0});
    uintptr_t proxy=0;std::memcpy(&proxy,reinterpret_cast<const void*>(live+0x40),8);
    put(window,0x510,ptr(new_draft));put(window,0x520,live);put(window,0x528,proxy);
}
void native_window_destroy(void* outer){
    ++destroys;need(outer==window.data(),"wrong original destroy receiver");native_delete_proposal(get(window,0x510));put(window,0x510,uintptr_t{0});
}
void reset(){
    window={};old_live={};new_live={};old_draft={};new_draft={};old_owner={};new_owner={};old_proxy={};new_proxy={};
    registry.clear();connected=ready=code_ok=census_ok=started=true;retiring=false;fault=false;cause.clear();setups=destroys=proposal_destroys=0;apply_tail={};
    state.thread=::GetCurrentThreadId();original_apply_setup=native_setup;original_apply_window_destroy=native_window_destroy;
    put(window,0,uintptr_t{0x1720f30});put(window,0x510,ptr(old_draft));put(window,0x520,ptr(old_live));put(window,0x528,ptr(old_proxy));
    put(window,0x530,ptr(new_owner)); // The stock wrapper already installed the next owner.
    for(auto* object:{&old_live,&new_live,&old_draft})put(*object,0,uintptr_t{0x171c930});
    put(old_live,0x18,old_uid);put(old_draft,0x18,old_uid);put(new_live,0x18,new_uid);
    put(old_live,0x40,ptr(old_proxy));put(new_live,0x40,ptr(new_proxy));
    put(old_proxy,0,ptr(old_live)+0x38);put(old_proxy,8,int32_t{1});put(new_proxy,0,ptr(new_live)+0x38);put(new_proxy,8,int32_t{1});
    old_members={ptr(old_live)};new_members={ptr(new_live)};
    put(old_owner,0x130,ptr(old_members));put(old_owner,0x138,int32_t{1});put(old_owner,0x13c,int32_t{1});
    put(new_owner,0x130,ptr(new_members));put(new_owner,0x138,int32_t{1});put(new_owner,0x13c,int32_t{1});
    current.definitions={{ptr(old_owner),false},{ptr(new_owner),false}};
    registry[static_cast<uint32_t>(old_uid&0xffffffffULL)]=ptr(old_live);registry[static_cast<uint32_t>(new_uid&0xffffffffULL)]=ptr(new_live);
}
void run(){
    reset();const auto old=old_live,next=new_live;
    apply_setup_entry(window.data(),new_live.data());
    need(!fault&&setups==1&&proposal_destroys==1,"stock switch did not execute once");
    need(old_live==old&&new_live==next&&find_uid(old_uid)==ptr(old_live)&&find_uid(new_uid)==ptr(new_live),"switch erased old UID or changed live records");
    need(get(window,0x530)==ptr(new_owner)&&get(window,0x510)==ptr(new_draft),"switch changed new owner binding or failed clone");
    // Destroy the newly created stock proposal, preserving its own live mapping.
    apply_window_destroy_entry(window.data());need(!fault&&destroys==1&&proposal_destroys==2&&find_uid(new_uid)==ptr(new_live),"reopen/switch/destroy lost current live UID");
    reset();apply_setup_entry(window.data(),old_live.data());need(!fault&&find_uid(old_uid)==ptr(old_live),"same-target reopen erased UID");
    reset();put(window,0x510,uintptr_t{0});apply_window_destroy_entry(window.data());need(!fault&&destroys==1&&!proposal_destroys,"empty editor destruction failed");
    for(int defect=0;defect<8;++defect){
        reset();
        if(defect==0)current.definitions.erase(current.definitions.begin());
        if(defect==1)new_members[0]=ptr(old_live);
        if(defect==2)put(old_proxy,0,ptr(old_live));
        if(defect==3)put(old_proxy,8,int32_t{0});
        if(defect==4)put(old_live,0x40,ptr(new_proxy));
        if(defect==5)registry[static_cast<uint32_t>(old_uid&0xffffffffULL)]=ptr(new_live);
        if(defect==6)put(old_draft,0x18,old_uid+(1ULL<<32));
        if(defect==7)registry[0]=ptr(new_live);
        const auto before=old_draft;apply_setup_entry(window.data(),new_live.data());
        need(fault&&!cause.empty()&&!setups&&!proposal_destroys&&old_draft==before,"invalid old ownership reached replacement");
    }
    reset();census_ok=false;apply_window_destroy_entry(window.data());need(fault&&!cause.empty()&&destroys==1&&proposal_destroys==1,"D1 protection failure concealed fault or skipped required native destruction");
    reset();connected=false;apply_setup_entry(window.data(),new_live.data());need(!fault&&setups==1&&!find_uid(old_uid),"disconnected original setup behavior changed");
    reset();connected=false;apply_window_destroy_entry(window.data());need(!fault&&destroys==1&&!find_uid(old_uid),"disconnected original destruction behavior changed");
    for(int disabled=0;disabled<3;++disabled){reset();census_ok=false;
        if(disabled==0)ready=false;if(disabled==1)started=false;if(disabled==2)retiring=true;
        apply_window_destroy_entry(window.data());need(!fault&&cause.empty()&&destroys==1&&proposal_destroys==1,"normal disabled/retiring teardown invented a protection fault");}
    reset();apply_tail={true,ptr(window),ptr(old_live),0,0,false};apply_setup_entry(window.data(),old_live.data());
    need(apply_tail.setup==1&&!setups&&!proposal_destroys&&get(old_draft,0x18)==old_uid,"consumed Apply suppression touched unrelated proposal");
}
}
int main()try{test::run();std::cout<<test::checks<<" actual-body stock proposal lifetime checks passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
