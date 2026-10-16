#include "bug_authority/manual_ui.h"
#include "bug_authority/manual_flow.h"
#include "bug_authority/manual_intent.h"
#include <iostream>
#include <map>
#include <stdexcept>
using namespace mt2mp::game;
namespace manual=bug_authority::manual_action;
int checks=0;
void require(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;
    template<class T>void put(uintptr_t p,T value){const auto* raw=reinterpret_cast<const unsigned char*>(&value);for(size_t n=0;n<sizeof value;++n)bytes[p+n]=raw[n];}
    bool copy(uintptr_t p,void* out,size_t n,std::string& e){for(size_t i=0;i<n;++i){const auto at=bytes.find(p+i);if(at==bytes.end()){e="actual read failed";return false;}static_cast<unsigned char*>(out)[i]=at->second;}return true;}
};
struct Selected {
    Memory memory;developer_staff::Capture capture;uintptr_t view=0x4000,bug=0x5000,proxy=0x6000;
    Selected(){capture.image=0x10000000;capture.pin.origin.mode=reinterpret_cast<void*>(0x1000);
        memory.put<uintptr_t>(0x10a0,0x2000);memory.put<int32_t>(0x10a8,1);memory.put<int32_t>(0x10ac,1);memory.put<uintptr_t>(0x2000,0x3000);
        memory.put<uintptr_t>(0x3000,capture.image+0x1722e60);memory.put<uintptr_t>(0x31b0,0x7000);memory.put<uint64_t>(0x31b8,11);memory.put<uintptr_t>(0x3318,0);
        const char name[]="development";for(size_t i=0;i<sizeof name;++i)memory.put<char>(0x7000+i,name[i]);
        memory.put<uintptr_t>(0x3558,view);memory.put<uintptr_t>(view,capture.image+0x172abf0);
        memory.put<uintptr_t>(view+0x460,bug);memory.put<uintptr_t>(view+0x468,proxy);memory.put<uintptr_t>(proxy,bug+0x18);memory.put<uintptr_t>(bug+0x20,proxy);
    }
    bool read(uintptr_t& out,std::string& e){return manual::selected_bug([&](auto p,auto target,auto n,auto& error){return memory.copy(p,target,n,error);},capture,view,out,e);}
};
struct Host {
    int pin_calls=0,job_calls=0,manual_calls=0;int failure=0;
    bool pin(std::string& e){++pin_calls;if(failure==1){e="pin cause";return false;}return true;}
    void job(bug_authority::JobCall& c){++job_calls;c.attempted=true;if(failure==2){c.exception=0xc0000005;return;}if(failure==3)throw std::runtime_error("original cause");c.returned=true;}
    bool after_job(std::string& e){if(failure==4){e="job readback cause";return false;}return true;}
    void manual(bug_authority::JobCall& c){++manual_calls;c.attempted=c.returned=c.record_confirmed=c.accepted=c.cleanup_confirmed=true;
        if(failure==5)c.accepted=false;if(failure==6)c.cleanup_exception=17;}
    bool after_manual(std::string& e){if(failure==7){e="manual readback cause";return false;}return true;}
};
struct Client {
    int step=0,failure=0;
    bool preflight(std::string& e){++step;if(failure==1){e="preflight cause";return false;}return true;}
    bool call(bool& attempted,std::string& e){++step;attempted=true;if(failure==step){e="apply cause";return false;}if(failure==step+10)throw std::runtime_error("apply throw");return true;}
    bool bugs(bool& attempted,std::string& e){return call(attempted,e);}
    bool developers(bool& attempted,std::string& e){return call(attempted,e);}
    bool finish(bool& attempted,std::string& e){return call(attempted,e);}
};
struct Ui {int originals=0,admits=0,retains=0;bool valid=true;
    bool original(){++originals;return false;}
    bool admit(std::string& e){++admits;if(!valid)e="UI owner cause";return valid;}
    bool retain(std::string&){++retains;return true;}
};
int main(){try{
    std::string e;uintptr_t found=0;
    Selected selected;require(selected.read(found,e)&&found==selected.bug,"actual owned selected Bug");
    {
        bug_authority::Row row;row.parent_key="action:11111111-1111-4111-8111-111111111111";row.birth.source=std::string(64,'a');row.value.known=true;
        bug_authority::State state{{row}};bug_authority::Lifetimes live;bug_authority::JobLifetimes jobs;
        require(live.born({row,0x9000,0xa000,0xb000,false},e),"original Bug birth");
        mt2mp::action_session::BugFixIntent intent;intent.bug=bug_authority::bug_identity(row);
        Memory memory;memory.put<uintptr_t>(0x9048,0);
        auto resolve=[&](){return manual::resolve_intent(intent,state,live,jobs,[&](auto p,auto out,auto n,auto& error){return memory.copy(p,out,n,error);},found,e);};
        require(resolve()&&found==0x9000,"Schedule witnessed pointer resolves");
        state.rows[0].value.known=false;require(!resolve(),"unknown Bug cannot fabricate UI eligibility");state.rows[0].value.known=true;
        memory.put<uintptr_t>(0x9048,0xc000);require(!resolve(),"Schedule stale after original Job creation");
        intent.schedule=false;intent.expected_job=bug_authority::Birth{bug_authority::BirthKind::Action,"11111111-1111-4111-8111-111111111111",0,1};
        require(!resolve(),"unwitnessed native Job cannot cancel");
        bug_authority::JobRow job;job.bug=intent.bug;job.birth=*intent.expected_job;
        require(jobs.born({job,0xc000,0x9000,0xa000,false},e)&&resolve(),"Cancel exact Job birth resolves");
        intent.expected_job->occurrence=2;require(!resolve(),"Cancel replaced Job birth rejects");intent.expected_job->occurrence=1;
        jobs.destroyed(0xc000);require(!resolve(),"Cancel destroyed Job cannot replay");
        memory.put<uintptr_t>(0x9048,0);intent.schedule=true;intent.expected_job.reset();require(live.remove(0x9000,e)&&!resolve(),"retired Bug cannot replay");
    }
    for(int mutation=0;mutation<7;++mutation){Selected bad;
        if(mutation==0)bad.memory.put<uintptr_t>(0x3558,0x4100);
        if(mutation==1)bad.memory.put<uintptr_t>(bad.view,0);
        if(mutation==2)bad.memory.put<uintptr_t>(bad.view+0x468,0);
        if(mutation==3)bad.memory.put<uintptr_t>(bad.proxy,0);
        if(mutation==4)bad.memory.put<uintptr_t>(bad.proxy,bad.bug+0x19);
        if(mutation==5)bad.memory.put<uintptr_t>(bad.bug+0x20,bad.proxy+8);
        if(mutation==6)bad.memory.bytes.erase(bad.proxy);
        e.clear();require(!bad.read(found,e)&&!e.empty(),"foreign/expired selected owner rejects");
        if(mutation==6)require(e=="actual read failed","selected read cause retained");
    }
    for(int command=0;command<3;++command){Ui ui;e.clear();const auto c=static_cast<manual::Command>(command);
        const bool handled=manual::capture_ui(true,c,ui,e);
        if(c==manual::Command::Other)require(!handled&&ui.originals==1&&!ui.admits&&!ui.retains,"unrelated UI forwards exactly once");
        if(c==manual::Command::Button)require(handled&&!ui.originals&&ui.admits==1&&ui.retains==1,"selected command suppresses and retains once");
        if(c==manual::Command::Unreadable)require(handled&&!ui.originals&&!ui.retains&&!e.empty(),"unreadable command never forwards");
    }
    {Ui ui;ui.valid=false;e.clear();require(manual::capture_ui(true,manual::Command::Button,ui,e)&&!ui.originals&&!ui.retains&&e=="UI owner cause","admission failure suppresses original");}
    {Ui ui;require(!manual::capture_ui(false,manual::Command::Button,ui,e)&&ui.originals==1&&!ui.admits,"singleplayer forwards");}
    for(int failure=0;failure<=7;++failure){Host calls;calls.failure=failure;manual::CallReceipt receipt;e.clear();
        const bool ok=manual::execute_host(calls,receipt,e);require(ok==(failure==0),"Host result matches exact stage");
        require(calls.job_calls==(failure==1?0:1),"Host original exactly once");
        require(calls.manual_calls==((failure==0||failure>=5)?1:0),"manual property runs only after original and readback");
        require(receipt.attempted==(failure!=1),"Host attempted survives failure");
        if(failure)require(!e.empty()&&!receipt.confirmed,"Host failure visible, not confirmed");
        if(failure==3)require(e.find("original cause")!=std::string::npos,"original exception cause retained");
        if(receipt.admitted){const int job=calls.job_calls;require(!manual::execute_host(calls,receipt,e)&&calls.job_calls==job,"admitted Host call cannot replay");}
    }
    for(int failure:{0,1,2,3,4,12,13,14}){Client calls;calls.failure=failure;bool attempted=false,threw=false;e.clear();bool ok=false;
        try{ok=manual::apply_client(calls,attempted,e);}catch(const std::exception& cause){threw=true;e=cause.what();}
        require(ok==(failure==0),"Client stage result");require(attempted==(failure!=1),"Client sticky attempted");
        require(threw==(failure>=12),"Client exceptions preserved");require(calls.step==(failure==0?4:failure>=12?failure-10:failure),"Client no later stages after failure");
    }
    manual::Entry entry{0x10000,0x30000,0x20000,true};Memory memory;
    for(size_t i=0;i<320;++i)memory.put<unsigned char>(entry.target+i,0x90);
    for(size_t i=0;i<36;++i)memory.put<unsigned char>(entry.trampoline+i,0);
    for(size_t i=0;i<8;++i){memory.put<unsigned char>(entry.target+i,manual::prefix[i]);memory.put<unsigned char>(entry.trampoline+i,manual::prefix[i]);}
    memory.put<unsigned char>(entry.target,0xe9);memory.put<int32_t>(entry.target+1,int32_t(entry.trampoline+22-entry.target-5));
    for(size_t start:{size_t(8),size_t(22)}){memory.put<unsigned char>(entry.trampoline+start,0xff);memory.put<unsigned char>(entry.trampoline+start+1,0x25);}
    memory.put<uintptr_t>(entry.trampoline+14,entry.target+8);memory.put<uintptr_t>(entry.trampoline+28,entry.detour);
    auto verify=[&](Memory& m,bool hash=true){return manual::verify_entry(entry,[&](auto p,auto out,auto n,auto& error){return m.copy(p,out,n,error);},[&](const auto& bytes){
        require(bytes.size()==320&&std::equal(manual::prefix.begin(),manual::prefix.end(),bytes.begin()),"hash covers restored full original body");
        return hash?std::string("b74314774d32edfffddf7f73ad97d9946ba868f819376459348cfb74b631a3fb"):std::string("bad");},e);};
    require(verify(memory),"actual MinHook8-byte relocation shape");require(!verify(memory,false),"original body mismatch rejects");
    for(int n=0;n<4;++n){auto wrong=memory;if(n==0)wrong.put<uintptr_t>(entry.trampoline+14,entry.target+9);if(n==1)wrong.put<uintptr_t>(entry.trampoline+28,entry.detour+1);
        if(n==2)wrong.put<unsigned char>(entry.target+5,0x90);if(n==3)wrong.bytes.erase(entry.trampoline);require(!verify(wrong),"hook ownership mismatch rejects");}
    std::cout<<checks<<" actual BugButton checks PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL check="<<checks<<" cause="<<e.what()<<'\n';return 1;}}
