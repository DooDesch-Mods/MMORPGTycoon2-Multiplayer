#include "subscriber_lifecycle/client_world.h"
#include "subscriber_lifecycle/runtime_append.h"
#include "session_provider.h"
#include "mt2mp/core/config.h"
#include <Windows.h>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>

namespace action=mt2mp::action_session;
namespace native=mt2mp::game::native;
namespace ref=mt2mp::subscriber_lifecycle::first_append;
using World=mt2mp::game::building_mixed_identity::opened_file::CurrentWorld;
constexpr uintptr_t fixture_image=0x140000000,game=0x10000,fixture_state=0x20000,map=0x30000,manager=0x40000;
constexpr uintptr_t task=0x50000,tick=0x60000,update=0x70000,sub=0x80000,toon=0x90000,brain=0xa0000;
constexpr uintptr_t members=0xb0000,fixture_online=0xc0000,fixture_clock_object=0xd0000;
const std::string fixture_origin="11111111-1111-4111-8111-111111111111",snapshot="22222222-2222-4222-8222-222222222222",sha(64,'a');
World fixture_world;
native::WorldStatus fixture_status;
native::ClockState clock_state;
uint32_t game_thread=0;
uint64_t common_epoch=77;
bool session_failure=false,world_failure=false;
int session_calls=0,world_calls=0,change_session_at=0,checks=0;
int status_calls=0,clock_calls=0,change_world_at=0,change_status_at=0,change_task_at=0,change_clock_at=0;
action::Context session_context;
std::string error;
void need(bool pass,const char* name){++checks;if(!pass)throw std::runtime_error(std::string(name)+": "+error);}
struct Memory final:mt2mp::subscriber_lifecycle::Memory {
    std::map<uintptr_t,unsigned char> bytes;
    size_t copies=0,cohort_reads=0;
    template<class T>void put(uintptr_t address,const T& value){const auto* p=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=p[i];}
    bool read(uintptr_t address,void* out,size_t size,std::string& e)override{
        ++copies;
        if((address>=manager+0x38&&address<manager+0xb0)||(address>=sub&&address<fixture_clock_object)||(address>=0x200000&&address<fixture_image))++cohort_reads;
        auto* p=static_cast<unsigned char*>(out);
        for(size_t i=0;i<size;++i){auto found=bytes.find(address+i);if(found==bytes.end()){e="fixture missing byte";return false;}p[i]=found->second;}
        return true;
    }
} memory;
BOOL fixture_read(HANDLE,LPCVOID address,LPVOID out,SIZE_T size,SIZE_T* copied){
    std::string e;const bool ok=memory.read(reinterpret_cast<uintptr_t>(address),out,size,e);
    *copied=ok?size:0;if(!ok)SetLastError(ERROR_PARTIAL_COPY);return ok?TRUE:FALSE;
}
namespace mt2mp {Config& Config::instance(){static Config value;return value;}}
namespace mt2mp::action_session {
struct FixtureSession final:Session {
    bool current(Context& out,std::string& e)override{
        ++session_calls;if(session_failure){e="original selected Session failure";return false;}
        out=session_context;if(session_calls==change_session_at)++out.session;return true;
    }
};
Session& selected_session(){static FixtureSession value;return value;}
}
namespace mt2mp::game::native {
bool read_resolver_receipt(uintptr_t& out,uint32_t& thread,std::string& hash,std::string&){out=::fixture_image;thread=game_thread;hash="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";return true;}
bool read_world_status(WorldStatus& out,std::string&){++status_calls;out=::fixture_status;
    if(status_calls==change_status_at)++out.saves_in_progress;
    if(status_calls==change_task_at)memory.put(task+0x90,int32_t(1));return true;}
bool read_clock(ClockState& out,std::string&){++clock_calls;out=clock_state;if(clock_calls==change_clock_at)out.time_scale=1;return true;}
}
namespace mt2mp::game::native_open_runtime {
bool read_current_world(World& out,std::string& e){++world_calls;if(world_failure){e="original native world failure";return false;}out=::fixture_world;if(world_calls==change_world_at)++out.generation;return true;}
}
namespace mt2mp::net {uint64_t current_world_epoch(){return common_epoch;}}
namespace mt2mp::game::subscriber_lifecycle_capture {
namespace {
namespace local=mt2mp::subscriber_lifecycle;
constexpr const char* exe_sha="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
#define ReadProcessMemory fixture_read
#include "steam_capture_readers.inc"
#undef ReadProcessMemory
}
using Json=nlohmann::json;
const action::Context* supplied=nullptr;
bool wire_supplied=false;
bool capture_impl(const std::filesystem::path&,int32_t,const std::string&,Json&,std::string&,Json* wire,const action::Context* expected){supplied=expected;wire_supplied=wire!=nullptr;return true;}
#include "steam_capture_entries.inc"
}
namespace mt2mp::game::subscriber_lifecycle_append {
namespace stage=mt2mp::subscriber_lifecycle::first_append;
namespace local=mt2mp::subscriber_lifecycle;
struct Work{std::string origin,local_sha;const FrameBinding* frame=nullptr;stage::ClientWorld original;};
#include "steam_append_readers.inc"
#include "steam_held_verifier.inc"
}
namespace host=mt2mp::game::subscriber_lifecycle_capture;
namespace client=mt2mp::game::subscriber_lifecycle_append;

action::Context context(bool host_role){return {0x123456789abcdefULL,host_role?1u:2u,1,3,host_role?action::Role::Host:action::Role::Client,snapshot};}
void reset(bool host_role=false){
    error.clear();memory.bytes.clear();memory.copies=memory.cohort_reads=0;session_calls=world_calls=change_session_at=0;session_failure=world_failure=false;
    status_calls=clock_calls=change_world_at=change_status_at=change_task_at=change_clock_at=0;
    game_thread=GetCurrentThreadId();session_context=context(host_role);common_epoch=77;
    auto& config=mt2mp::Config::instance();config.local_test=false;config.is_host=!host_role;config.peer_id=host_role?4:1;
    fixture_world={};fixture_world.game=reinterpret_cast<void*>(game);fixture_world.state=reinterpret_cast<void*>(fixture_state);fixture_world.world=reinterpret_cast<void*>(0xe0000);fixture_world.map=reinterpret_cast<void*>(map);
    fixture_world.uuid=fixture_origin;fixture_world.sha256=sha;fixture_world.world_epoch=5;fixture_world.generation=6;
    fixture_status={};fixture_status.active_mode_index=3;fixture_status.in_game_state=4;fixture_status.world_present=true;
    fixture_status.edit_operations=0;fixture_status.saves_in_progress=0;
    clock_state={};clock_state.paused=true;clock_state.time_scale=0;clock_state.pause_scale=1;
    memory.put(fixture_image+0x3c,uint32_t(0x80));memory.put(fixture_image+0x80+24+56,uint32_t(0x1800000));
    memory.put(fixture_state,fixture_image+0x16de920);memory.put(fixture_state+0x1b8,manager);memory.put(fixture_image+0x13df690,manager);memory.put(manager,fixture_image+0x1725f40);
    memory.put(fixture_image+0x13df340,task);memory.put(fixture_image+0x13df350,tick);memory.put(fixture_image+0x13df4a0,update);
    memory.put(task,fixture_image+0x16fdaf0);memory.put(tick,fixture_image+0x16fdc40);memory.put(update,fixture_image+0x1709650);
    memory.put(game+0x68,task);memory.put(game+0x70,update);memory.put(game+0x150,tick);
    memory.put(task+0xf8,uint8_t(0));memory.put(tick+0x28,uint8_t(0));memory.put(update+0x68,uint8_t(0));
    memory.put(task+0xa8,int32_t(0));memory.put(task+0x90,int32_t(0));
    memory.put(fixture_image+0x13df9a0,fixture_clock_object);memory.put(fixture_clock_object,fixture_image+0x17499d0);
    memory.put(manager+0x40,members);memory.put(manager+0x48,int32_t(1));memory.put(manager+0x4c,int32_t(1));memory.put(manager+0x68,int32_t(8));memory.put(members,sub);
    memory.put(manager+0x88,fixture_online);memory.put(manager+0x90,int32_t(1));memory.put(manager+0x94,int32_t(1));memory.put(fixture_online,sub);
    memory.put(manager+0xa0,uintptr_t(0));memory.put(manager+0xa8,int32_t(0));memory.put(manager+0xac,int32_t(0));
    memory.put(sub,fixture_image+0x16e75f0);memory.put(sub+0x20,int32_t(7));memory.put(sub+0x80,toon);memory.put(sub+0x88,brain);
    memory.put(toon,fixture_image+0x1744e90);memory.put(toon+0x940,sub);memory.put(toon+0x825,uint8_t(1));
    memory.put(brain,fixture_image+0x16fdd60);memory.put(brain+0x28,sub);
}
bool read_client(const action::Context* expected,bool held=false){ref::ClientWorld out;return ref::read_client_world(memory,sha,fixture_origin,held,out,error,expected);}
bool read_host(const action::Context* expected){host::Pin out;return host::pin(7,fixture_origin,out,error,0,true,expected);}
void hold(){
    fixture_status.saves_in_progress=1;clock_state.pause_scale=0;
    memory.put(task+0xf8,uint8_t(1));memory.put(tick+0x28,uint8_t(1));memory.put(update+0x68,uint8_t(1));
}
bool read_roots(const action::Context* expected){ref::ClientWorld out;return ref::read_client_world_roots(memory,sha,fixture_origin,true,out,error,expected);}
void cohort(int32_t count){
    memory.put(manager+0x48,count);memory.put(manager+0x4c,count);memory.put(manager+0x68,count+7);
    memory.put(manager+0x90,count);memory.put(manager+0x94,count);
    for(int32_t i=0;i<count;++i){
        const auto s=uintptr_t(0x200000)+uintptr_t(i)*0x3000,t=s+0x1000,b=s+0x2000;
        memory.put(members+uintptr_t(i)*8,s);memory.put(fixture_online+uintptr_t(i)*8,s);
        memory.put(s,fixture_image+0x16e75f0);memory.put(s+0x20,i+7);memory.put(s+0x80,t);memory.put(s+0x88,b);
        memory.put(t,fixture_image+0x1744e90);memory.put(t+0x940,s);memory.put(t+0x825,uint8_t(1));
        memory.put(b,fixture_image+0x16fdd60);memory.put(b+0x28,s);
    }
}
void held_root_checks(){
    const auto expected=context(false);size_t one_root=0,one_full=0;
    for(const int32_t count:{1,251}){
        reset();hold();cohort(count);
        need(read_roots(&expected)&&session_calls==2&&world_calls==2&&memory.cohort_reads==0,"held roots use two fresh pins and zero census reads");
        if(count==1)one_root=memory.copies;else need(memory.copies==one_root,"held root read work is independent of cohort size");
        memory.copies=memory.cohort_reads=0;
        need(read_client(&expected,true)&&memory.cohort_reads>0,"full reader retains complete census reads");
        if(count==1)one_full=memory.copies;else need(memory.copies>one_full,"full census read grows with real cohort size");
    }
    reset();hold();change_session_at=2;need(!read_roots(&expected)&&session_calls==2,"second root pin rejects changed session");
    reset();hold();change_world_at=2;need(!read_roots(&expected)&&error=="Subscriber Client double-read roots changed","second root pin rejects changed world generation");
    reset();hold();change_status_at=2;need(!read_roots(&expected)&&error.find("effects guard")!=std::string::npos,"second root pin rejects SaveCount drift");
    reset();hold();change_task_at=2;need(!read_roots(&expected)&&error.find("not drained")!=std::string::npos,"second root pin rejects queued task");
    reset();hold();change_clock_at=2;need(!read_roots(&expected)&&error.find("Clock0")!=std::string::npos,"second root pin rejects Clock change");
    for(int mutation=0;mutation<12;++mutation){
        reset();hold();
        switch(mutation){
        case 0:fixture_status.edit_operations=1;break;case 1:fixture_status.saves_in_progress=0;break;
        case 2:fixture_status.active_mode_index=2;break;case 3:fixture_status.quitting=true;break;
        case 4:memory.put(task+0xf8,uint8_t(0));break;case 5:memory.put(tick+0x28,uint8_t(0));break;
        case 6:memory.put(update+0x68,uint8_t(0));break;case 7:memory.put(task+0xa8,int32_t(1));break;
        case 8:memory.put(task+0x90,int32_t(1));break;case 9:memory.put(game+0x68,uintptr_t(0));break;
        case 10:memory.put(fixture_state+0x1b8,uintptr_t(0));break;default:game_thread=GetCurrentThreadId()+1;break;
        }
        need(!read_roots(&expected),"held roots retain native owner/pause/effects guard");
    }
    reset();hold();need(!read_roots(nullptr),"roots without Context retain private restriction");
    auto& cfg=mt2mp::Config::instance();cfg.local_test=true;cfg.is_host=false;cfg.peer_id=2;
    need(read_roots(nullptr)&&session_calls==0,"private roots retain no-Context admission");
    reset();hold();session_failure=true;need(!read_roots(&expected)&&error=="original selected Session failure","roots preserve session failure cause");
    reset();hold();world_failure=true;need(!read_roots(&expected)&&error=="original native world failure","roots preserve world failure cause");
    for(int corruption=0;corruption<3;++corruption){
        reset();hold();
        if(corruption==0)memory.put(toon+0x940,uintptr_t(0));
        else if(corruption==1)memory.put(fixture_online,uintptr_t(0));
        else memory.put(brain+0x28,uintptr_t(0));
        need(read_roots(&expected)&&memory.cohort_reads==0,"root proof deliberately does not claim cohort integrity");
        need(!read_client(&expected,true),"unchanged full boundary rejects cohort corruption");
    }
    reset();hold();client::FrameBinding binding{expected,{5,6},77,sha};client::QuestEditBatch batch;
    batch.world.origin=fixture_origin;batch.world.local_sha=sha;batch.world.frame=&binding;
    need(client::read_world(batch.world,true,batch.world.original,error),"batch fixture full admission");
    batch.claimed=true;batch.scope.enter_returned=true;
    memory.copies=memory.cohort_reads=0;session_calls=world_calls=0;
    need(client::verify_quest_edits_held(batch,error)&&memory.cohort_reads==0&&session_calls==3,"actual verifier uses current Frame plus both fresh root pins");
    ++common_epoch;need(!client::verify_quest_edits_held(batch,error)&&error.find("origin binding")!=std::string::npos,"actual roots wrapper retains common epoch fence");--common_epoch;
    for(int mutation=0;mutation<5;++mutation){
        batch.claimed=true;batch.scope.enter_returned=true;batch.scope.leave_attempted=false;batch.scope.unknown=false;batch.may_leave=true;
        if(mutation==0)batch.claimed=false;else if(mutation==1)batch.scope.leave_attempted=true;else if(mutation==2)batch.scope.unknown=true;else if(mutation==3)batch.may_leave=false;else batch.scope.enter_returned=false;
        const auto prior=memory.copies;
        need(!client::verify_quest_edits_held(batch,error)&&memory.copies==prior,"actual verifier rejects unhealthy scope before reads");
    }
}
int main(){try{
    reset();const auto client_context=context(false);need(read_client(&client_context)&&session_calls==2&&world_calls==2,"Steam Client supplied Context admits both complete passes despite opposite Config role");
    const auto production_copies=memory.copies;
    reset();auto& cfg=mt2mp::Config::instance();cfg.local_test=true;cfg.is_host=false;cfg.peer_id=2;
    need(read_client(nullptr)&&session_calls==0&&memory.copies==production_copies,"private Client preserves all existing field reads without selected Session");
    reset();need(!read_client(nullptr)&&error.find("owned Client")!=std::string::npos&&memory.copies==0,"Steam cannot enter private Client reader without Context");
    reset(true);const auto host_context=context(true);need(read_host(&host_context)&&session_calls==2&&world_calls==2,"Steam Host supplied Context admits both complete census passes");
    const auto host_copies=memory.copies;
    reset(true);cfg.local_test=true;cfg.is_host=true;cfg.peer_id=1;need(read_host(nullptr)&&session_calls==0&&memory.copies==host_copies,"private Host retains identical field reads");
    reset(true);need(!read_host(nullptr)&&error.find("owned test Host1")!=std::string::npos&&memory.copies==0,"Steam cannot enter private Host capture without Context");
    for(bool host_role:{false,true}){
        reset(host_role);auto expected=context(host_role);session_failure=true;
        need(!(host_role?read_host(&expected):read_client(&expected))&&error=="original selected Session failure"&&memory.copies==0,"provider failure preserved before native field reads");
        for(int mutation=0;mutation<7;++mutation){
            reset(host_role);expected=context(host_role);
            switch(mutation){case 0:++session_context.session;break;case 1:session_context.snapshot_uuid=fixture_origin;break;
            case 2:session_context.role=host_role?action::Role::Client:action::Role::Host;break;case 3:session_context.local_peer=5;break;
            case 4:session_context.host_peer=2;break;case 5:session_context.peer_mask=0;break;default:session_context.local_peer=76561198013473990ULL;break;}
            need(!(host_role?read_host(&expected):read_client(&expected))&&memory.copies==0,"changed Context/role/slot/membership fails before reads");
            if(mutation>=2){expected=session_context;need(!(host_role?read_host(&expected):read_client(&expected))&&memory.copies==0,"matching malformed role/slot/mask remains inadmissible");}
        }
        reset(host_role);expected=context(host_role);change_session_at=2;
        need(!(host_role?read_host(&expected):read_client(&expected))&&session_calls==2&&world_calls==1,"second full pin freshly rejects changed session");
        reset(host_role);expected=context(host_role);fixture_status.saves_in_progress=1;
        need(!(host_role?read_host(&expected):read_client(&expected))&&error.find("effects guard")!=std::string::npos,"SaveIdle guard unchanged");
        reset(host_role);expected=context(host_role);game_thread=GetCurrentThreadId()+1;
        need(!(host_role?read_host(&expected):read_client(&expected))&&error.find("mainthread")!=std::string::npos,"Mainthread guard unchanged");
        reset(host_role);expected=context(host_role);memory.put(toon+0x940,uintptr_t(0));
        need(!(host_role?read_host(&expected):read_client(&expected)),"reciprocal Toon owner guard unchanged");
    }
    reset();fixture_status.saves_in_progress=1;memory.put(task+0xf8,uint8_t(1));memory.put(tick+0x28,uint8_t(1));memory.put(update+0x68,uint8_t(1));
    need(read_client(&client_context,true),"held Client SaveScope uses supplied Context");
    memory.put(task+0xa8,int32_t(1));need(!read_client(&client_context,true)&&error.find("not drained")!=std::string::npos,"held scheduler drain guard retained");
    reset();client::FrameBinding binding{client_context,{5,6},77,sha};client::Work work{fixture_origin,sha,&binding};ref::ClientWorld out;
    need(client::read_world(work,false,out,error)&&session_calls==3,"actual append read_world forwards Frame Context through both fresh pins");
    reset();++common_epoch;need(!client::read_world(work,false,out,error)&&error.find("origin binding")!=std::string::npos,"existing current_frame common epoch guard retained");
    reset();work.frame=nullptr;need(!client::read_world(work,false,out,error)&&session_calls==0,"actual private append wiring cannot gain Steam authority");
    nlohmann::json graph,receipt;
    need(host::capture_frame(7,fixture_origin,host_context,graph,receipt,error)&&host::supplied==&host_context&&host::wire_supplied,"actual Host Frame entry forwards supplied Context");
    need(host::capture({},7,fixture_origin,receipt,error)&&host::supplied==nullptr&&!host::wire_supplied,"actual private Host entry keeps absent Context");
    held_root_checks();
    std::cout<<checks<<" Steam lifecycle admission checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"failure after "<<checks<<": "<<e.what()<<'\n';return 1;}}
