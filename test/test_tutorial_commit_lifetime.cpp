#include "tutorial_development.h"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace tutorial_development=mt2mp::game::tutorial_development;
namespace startingpoint=mt2mp::game::tutorial_replication::startingpoint;
using World=mt2mp::game::building_mixed_identity::opened_file::CurrentWorld;
namespace action_session {
enum class Role{Host,Client};struct Context{Role role=Role::Host;};
struct Session{bool available=true;Role role=Role::Host;bool current(Context& out,std::string& e){if(!available){e="original common Context unavailable";return false;}out.role=role;return true;}} session;
Session& selected_session(){return session;}
}
namespace {
struct CommitReceipt{World origin;uintptr_t tech=0,record=0;std::optional<startingpoint::CommitBugs> bugs;std::string cause;} commit_receipt,research_commit_receipt;
struct {bool armed=false;} load;
bool client_commit_suppressed=false,entry_valid=true;
bool client_commit_submitted=false,intent_accept=false;
std::string client_commit_action,client_commit_cause;
unsigned intent_calls=0,research_intent_calls=0;std::string selected_key="startingpoint";
bool current_project_key(void*,std::string& out,std::string&){out=selected_key;return true;}
bool submit_client_research_commit(void*,std::string& e){++research_intent_calls;if(!intent_accept){e="original Controller request pending";return false;}client_commit_action="bridges-intent";return true;}
bool submit_client_commit(void*,std::string& e){++intent_calls;if(!intent_accept){e="original Controller request pending";return false;}client_commit_action="fixed-intent";return true;}
uintptr_t image=0x140000000;uint32_t mainthread=7,thread=7;
unsigned originals=0,read_calls=0,tests=0;
std::string fault;
std::vector<std::string> messages;
std::vector<tutorial_development::StartingpointSnapshot> samples;
std::function<void()> original_effect;
int fail_read=0;
uint32_t GetCurrentThreadId(){return thread;}
void log(const std::string& text,const std::string& cause={}){messages.push_back(text+cause);}
#define LOG_INFO(...) log(__VA_ARGS__)
#define LOG_WARN(...) log(__VA_ARGS__)
void block(const std::string& cause){if(fault.empty())fault=cause;}
bool verify_commit_entry(uintptr_t target,std::string& e){if(!entry_valid||target!=image+0x25fb10){e="original owned Commit route failed";return false;}return true;}
void original_commit(void*){++originals;if(original_effect)original_effect();}
}
namespace mt2mp::game::tutorial_development {
bool read_startingpoint(StartingpointSnapshot& out,nlohmann::json&,std::string& e,bool){
    ++read_calls;if(int(read_calls)==fail_read){e="original Startingpoint read failed";return false;}
    if(read_calls>samples.size()){e="fixture snapshot absent";return false;}out=samples[read_calls-1];return true;
}
}
namespace mt2mp::game::tutorial_development {
bool read_research(const std::string& key,StartingpointSnapshot& out,nlohmann::json& diagnostic,std::string& e,bool creation){
    if(key!=selected_key){e="fixture selected Commit key differs";return false;}
    return read_startingpoint(out,diagnostic,e,creation);
}
}
namespace {
#include "native_commit.inc"
tutorial_development::StartingpointSnapshot project(){
    tutorial_development::StartingpointSnapshot out;out.origin.game=reinterpret_cast<void*>(1);out.origin.mode=reinterpret_cast<void*>(2);out.origin.state=reinterpret_cast<void*>(3);out.origin.world=reinterpret_cast<void*>(4);out.origin.map=reinterpret_cast<void*>(5);out.origin.uuid="joined-origin";out.origin.sha256="fixture";out.origin.bytes=75843;out.origin.generation=8;out.origin.world_epoch=5;
    out.lounge=0x200000;out.tech=0x210000;out.record=0x220000;out.project=0x230000;
    startingpoint::Row row;row.project_present=row.purchased=true;out.row=row;return out;
}
auto deployed(){auto out=project();out.project=0;out.row->project_present=false;out.row->deployed=true;out.row->inventory_count=1;out.row->release_change_count=1;out.row->release_feature_count=1;out.row->release_counter=1;return out;}
void reset(){client_commit_submitted=intent_accept=false;client_commit_action.clear();client_commit_cause.clear();intent_calls=research_intent_calls=0;selected_key="startingpoint";commit_receipt={};research_commit_receipt={};load.armed=false;client_commit_suppressed=false;entry_valid=true;thread=mainthread;originals=read_calls=0;fault.clear();messages.clear();samples.clear();original_effect={};fail_read=0;action_session::session={};}
void saved_receipt(){auto old=deployed();commit_receipt={old.origin,old.tech,old.record,startingpoint::CommitBugs{0,0},{}};}
void need(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
void pass(const char* name){++tests;std::cout<<"PASS "<<name<<'\n';}
}
int main(){try{
    {reset();load.armed=true;intent_accept=true;commit_current(reinterpret_cast<void*>(0x200000));need(!originals&&!read_calls&&intent_calls==1&&client_commit_submitted&&client_commit_action=="fixed-intent"&&client_commit_cause.empty(),"Client Commit did not submit exactly one intent");pass("joined Client submits fixed intent without executing original Commit");}
    {reset();auto before=project();before.row->bugs_total=2;before.row->bugs_known=1;samples={before,deployed()};commit_current(reinterpret_cast<void*>(before.lounge));need(originals==1&&read_calls==2&&commit_receipt.bugs==startingpoint::CommitBugs{2,1}&&commit_receipt.tech==before.tech&&commit_receipt.record==before.record&&commit_receipt.cause.empty(),"Host Commit did not preserve actual pre-reset bug receipt");pass("original Host Commit runs once and records pre-reset bug counters");}
    {reset();saved_receipt();auto other=deployed();other.project=0x240000;samples={other};commit_current(reinterpret_cast<void*>(other.lounge));need(originals==1&&read_calls==1&&commit_receipt.bugs==startingpoint::CommitBugs{0,0}&&commit_receipt.record==other.record,"later unrelated Commit erased Startingpoint receipt");pass("later other-project Commit preserves proven Startingpoint receipt");}
    {reset();saved_receipt();fail_read=1;samples={project()};commit_current(reinterpret_cast<void*>(0x200000));need(originals==1&&commit_receipt.bugs.has_value()&&!messages.empty()&&messages.back().find("original Startingpoint read failed")!=std::string::npos,"optional read failure erased receipt or swallowed original");pass("optional Host read failure retains receipt and cause while original runs once");}
    {reset();saved_receipt();action_session::session.available=false;commit_current(reinterpret_cast<void*>(0x200000));need(originals==1&&!read_calls&&commit_receipt.bugs.has_value()&&!messages.empty()&&messages.back().find("original common Context unavailable")!=std::string::npos,"missing Context fabricated native observation");pass("missing common Context cannot create a receipt or suppress Host original");}
    {reset();load.armed=true;commit_current(reinterpret_cast<void*>(0x200000));commit_current(reinterpret_cast<void*>(0x200000));need(!originals&&!read_calls&&client_commit_suppressed&&messages.size()==1&&fault.empty(),"joined Client repeated native Commit or repeated its diagnostic");pass("joined Client Commit is suppressed without independent Deploy and logs once");}
    {reset();load.armed=true;++thread;commit_current(reinterpret_cast<void*>(0x200000));need(!originals&&!read_calls&&!client_commit_suppressed&&fault=="Client native Commit thread differs","foreign Client thread crossed Commit boundary");pass("wrong Client thread fails visibly before any native call");}
    {reset();auto before=project(),after=deployed();after.record+=8;samples={before,after};commit_current(reinterpret_cast<void*>(before.lounge));need(originals==1&&!commit_receipt.bugs&&commit_receipt.cause=="startingpoint original Commit owner/retirement readback differs","changed actual native record authorized receipt");pass("post-Commit record mismatch cannot confirm original effects");}
    {reset();auto before=project();samples={before,deployed()};fail_read=2;commit_current(reinterpret_cast<void*>(before.lounge));need(originals==1&&!commit_receipt.bugs&&commit_receipt.cause=="original Startingpoint read failed","post-effect read failure was success-shaped");pass("post-Commit read failure preserves original partial-observation cause");}
    {reset();entry_valid=false;samples={project()};commit_current(reinterpret_cast<void*>(0x200000));need(originals==1&&!read_calls&&!commit_receipt.bugs&&!messages.empty()&&messages.back().find("original owned Commit route failed")!=std::string::npos,"unowned route fabricated Commit receipt");pass("optional exact-hook guard failure preserves original Host call but yields no receipt");}
    {reset();selected_key="bridges";load.armed=true;intent_accept=true;commit_current(reinterpret_cast<void*>(0x200000));need(!originals&&!read_calls&&!intent_calls&&research_intent_calls==1&&client_commit_action=="bridges-intent"&&client_commit_submitted,"Client Bridges Commit routed to fixed Startingpoint intent");pass("joined Client selects Bridges Commit intent without native Commit");}
    {reset();saved_receipt();selected_key="bridges";auto before=project();before.row->bugs_total=5;before.row->bugs_known=3;before.tech+=0x100;before.record+=0x100;auto after=deployed();after.tech=before.tech;after.record=before.record;samples={before,after};commit_current(reinterpret_cast<void*>(before.lounge));need(originals==1&&read_calls==2&&research_commit_receipt.bugs==startingpoint::CommitBugs{5,3}&&research_commit_receipt.tech==before.tech&&commit_receipt.bugs==startingpoint::CommitBugs{0,0}&&commit_receipt.tech!=before.tech,"Bridges Commit lost actual bugs or overwrote historical Startingpoint receipt");pass("Host Bridges original Commit records actual bugs in separate receipt");}
    {reset();selected_key="bridges";auto before=project();samples={before,deployed()};fail_read=2;commit_current(reinterpret_cast<void*>(before.lounge));need(originals==1&&!research_commit_receipt.bugs&&research_commit_receipt.cause=="original Startingpoint read failed"&&commit_receipt.cause.empty(),"Bridges post-effect read failure was hidden or polluted Startingpoint receipt");pass("Bridges partial read failure remains on selected receipt");}
    std::cout<<tests<<" actual Commit lifetime scenarios passed\n";return 0;
}catch(const std::exception& cause){std::cerr<<"error: "<<cause.what()<<'\n';return 1;}}
