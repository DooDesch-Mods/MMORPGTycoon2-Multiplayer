#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "tutorial_development.h"
#include "tutorial_replication/startingpoint.h"
#include "live_host_state/native_port.h"
#include "mt2mp/game/native_engine.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <type_traits>
namespace tutorial_development=mt2mp::game::tutorial_development;
namespace startingpoint=mt2mp::game::tutorial_replication::startingpoint;
namespace action_session=mt2mp::action_session;
namespace live_host_state=mt2mp::game::live_host_state;
namespace native=mt2mp::game::native;
using World=mt2mp::game::building_mixed_identity::opened_file::CurrentWorld;
namespace {
constexpr uintptr_t fixture_image=0x140000000,fixture_lounge=0x200000,fixture_project=0x300000;
uintptr_t image=fixture_image;uint32_t mainthread=7;
action_session::Context fixture_context;
action_session::Binding fixture_binding{5,8};
tutorial_development::StartingpointSnapshot physical;
live_host_state::Pin economy_pin;
native::WorldStatus world_status;
native::ClockState clock_status;
unsigned purchase_calls=0,set_calls=0,snapshot_reads=0,economy_reads=0,checks=0;
unsigned fail_economy_read=0;
DWORD purchase_error=0,set_error=0;
volatile DWORD unexpected_native_error=99;
std::function<void()> after_purchase,after_set;
std::function<void(unsigned)> during_read;
bool code_valid=true;
struct Memory{} memory;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool origin(const action_session::Context& c,const action_session::Binding& b,World& out,std::string& e){if(c!=fixture_context||b!=fixture_binding)return fail(e,"original NativeOpen Context/fixture_binding changed");out=physical.origin;return true;}
bool verify_commit_entry(uintptr_t address,std::string& e){return address==image+0x25fb10||fail(e,"original owned Commit entry differs");}
bool startingpoint_code(std::string& e){return code_valid||fail(e,"original Startingpoint code guard differs");}
DWORD startingpoint_deploy(uintptr_t){return unexpected_native_error;}
DWORD startingpoint_destroy(uintptr_t){return unexpected_native_error;}
template<class T>bool at(uintptr_t p,size_t offset,T& out,std::string& e){
    if(p==image+0x16e48c0&&offset==8){out=static_cast<T>(image+0x261980);return true;}
    if(p==physical.lounge&&offset==0xa8){out=static_cast<T>(physical.project);return true;}
    return fail(e,"unexpected physical fixture read");
}
template<class T>bool put(uintptr_t p,size_t offset,const T& value,std::string& e){
    if(p!=physical.lounge||!physical.row)return fail(e,"original project store owner changed");
    if(offset==0xb0){physical.row->work=static_cast<float>(value);if constexpr(sizeof(T)==8)physical.row->lines=0;}
    else if(offset==0xb4)physical.row->lines=static_cast<int32_t>(value);
    else if(offset==0xb8){physical.row->bugs_total=static_cast<int32_t>(value);if constexpr(sizeof(T)==8)physical.row->bugs_known=0;}
    else if(offset==0xbc)physical.row->bugs_known=static_cast<int32_t>(value);
    else if(offset==0xc4)physical.row->desired_workers=static_cast<int32_t>(value);
    else if(offset==0xa8)physical.project=static_cast<uintptr_t>(value);
    else return fail(e,"unexpected project fixture store");return true;
}
}
namespace mt2mp::game::tutorial_replication::startingpoint::creation {
template<class M>bool code(M&,uintptr_t,std::string& e){return code_valid||fail(e,"original creation entry differs");}
DWORD call_purchase_for_free(uintptr_t base,uintptr_t record){
    ++purchase_calls;if(base!=fixture_image||record!=physical.record)throw std::runtime_error("wrong PurchaseForFree receiver");
    physical.row->purchased=true;if(after_purchase)after_purchase();return purchase_error;
}
DWORD call_set_current_project(uintptr_t base,uintptr_t lounge,uintptr_t source){
    ++set_calls;if(base!=fixture_image||lounge!=physical.lounge||!physical.creation||source!=physical.creation->source)throw std::runtime_error("wrong SetCurrent receiver/source");
    physical.project=fixture_project;auto& row=*physical.row;row.project_present=row.purchased=true;row.work=0;row.lines=row.bugs_total=row.bugs_known=row.desired_workers=0;
    auto& owners=*physical.creation;auto entry=std::find_if(owners.catalogue.begin(),owners.catalogue.end(),[&](const auto& item){return item.address==source;});
    if(entry==owners.catalogue.end())throw std::runtime_error("fixture source absent");owners.current=*entry;owners.current->address=physical.project;
    for(auto& developer:owners.developers)if(developer.job>=5&&developer.job<=7)developer.job=0;
    if(owners.automatic_workers)row.desired_workers=owners.total_workers<=owners.max_workers?owners.total_workers-1:owners.max_workers;
    if(after_set)after_set();return set_error;
}
}
namespace mt2mp::game::tutorial_development {
bool read_startingpoint(StartingpointSnapshot& out,nlohmann::json&,std::string&,bool creation,bool){++snapshot_reads;if(during_read)during_read(snapshot_reads);out=physical;if(!creation)out.creation.reset();return true;}
}
namespace mt2mp::game::tutorial_development {
bool read_research(const std::string& key,StartingpointSnapshot& out,nlohmann::json& diagnostic,std::string& e,bool creation,bool share_metadata){
    if(key!="startingpoint")throw std::runtime_error("Startingpoint selected another Research key");
    return read_startingpoint(out,diagnostic,e,creation,share_metadata);
}
}
namespace mt2mp::game::native {
bool read_resolver_receipt(uintptr_t& base,uint32_t& thread,std::string& sha,std::string&){base=image;thread=mainthread;sha="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";return true;}
bool read_clock(ClockState& out,std::string&){out=clock_status;return true;}
bool read_world_status(WorldStatus& out,std::string&){out=world_status;return true;}
}
namespace mt2mp::game::live_host_state {
bool NativePort::publication_ready(uint64_t,bool&,std::string&){throw std::runtime_error("Startingpoint entered Host publication readiness");}
bool NativePort::read(Pin& out,std::string& e){++economy_reads;if(economy_reads==fail_economy_read){e="original physical Economy read failed";return false;}out=economy_pin;return true;}
bool NativePort::store(const Pin&,size_t,int32_t,int32_t,std::string& e){e="unexpected Economy store in creation fixture";return false;}
}
namespace {
// The actual pin checks the current thread returned by this public Windows API.
void reset(){
    fixture_context={};fixture_context.role=action_session::Role::Client;mainthread=GetCurrentThreadId();fixture_binding={5,8};
    physical={};physical.image=image;physical.origin.game=reinterpret_cast<void*>(1);physical.origin.mode=reinterpret_cast<void*>(2);physical.origin.state=reinterpret_cast<void*>(3);physical.origin.world=reinterpret_cast<void*>(4);physical.origin.map=reinterpret_cast<void*>(5);physical.origin.uuid="joined-snapshot";physical.origin.sha256="fixture";physical.origin.bytes=75843;physical.origin.world_epoch=5;physical.origin.generation=8;
    physical.lounge=fixture_lounge;physical.tech=0x210000;physical.record=0x220000;physical.player=0x230000;physical.inventory=0x240000;physical.slots=0x250000;physical.release=0x260000;physical.row=startingpoint::Row{};
    tutorial_development::CreationOwners owners;owners.source=0x270000;owners.catalogue_data=0x280000;owners.catalogue_capacity=4;owners.catalogue.push_back({owners.source,0,2,"startingpoint","native_art"});owners.developer_data=0x290000;owners.developer_capacity=4;owners.developers={{0x291000,5},{0x292000,6},{0x293000,7},{0x294000,4}};owners.max_workers=8;owners.total_workers=5;owners.release_manager=0x2a0000;owners.release_owner=physical.release;owners.release_data=0x2b0000;owners.release_capacity=4;physical.creation=owners;
    economy_pin={};economy_pin.context=fixture_context;economy_pin.binding=fixture_binding;economy_pin.image=image;economy_pin.world=reinterpret_cast<uintptr_t>(physical.origin.world);economy_pin.clock=0x400000;economy_pin.business=0x410000;economy_pin.record=0x420000;economy_pin.history=0x430000;economy_pin.thread=mainthread;economy_pin.history_count=3;economy_pin.history_capacity=5;economy_pin.values.cash=92430;economy_pin.values.record={1,2,3,4,5};economy_pin.values.calendar={1,2,3,4,5,6};for(size_t i=0;i<20;++i)economy_pin.values.lines[i]={int32_t(i),int32_t(i+20)};
    world_status={};world_status.saves_in_progress=world_status.edit_operations=0;clock_status={};clock_status.time_scale=0;clock_status.pause_scale=1;purchase_calls=set_calls=snapshot_reads=economy_reads=0;fail_economy_read=0;purchase_error=set_error=0;after_purchase=after_set={};during_read={};code_valid=true;
}
// Startingpoint must never enter the keyed Research deployment boundary.
namespace research_release {
struct Graph { bool operator==(const Graph&)const=default; };
bool read(uintptr_t,Graph&,std::string&){throw std::runtime_error("Startingpoint entered Research Release read");}
bool admissible(const Graph&,uintptr_t,std::string&){throw std::runtime_error("Startingpoint entered Research Release admission");}
bool preserve(const Graph&,const Graph&,uintptr_t,nlohmann::json&,std::string&){throw std::runtime_error("Startingpoint entered Research Release preservation");}
}
World research_proof_origin;
nlohmann::json research_commit_proof;
#include "creation_calls.inc"
startingpoint::Row target(){startingpoint::Row row;row.project_present=row.purchased=true;row.work=78.88058471679688f;row.lines=182;row.desired_workers=1;return row;}
void need(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
void pass(const char* name){++checks;std::cout<<"PASS "<<name<<'\n';}
}
int main(){try{
    {reset();const auto initial=physical.row;StartingpointCalls calls{fixture_context,fixture_binding,{}};bool attempted=false,unknown=false;std::string error;need(startingpoint::apply(initial,target(),calls,attempted,unknown,error)&&attempted&&!unknown&&physical.row==target()&&purchase_calls==1&&set_calls==1,"actual creation adapter failed: "+error);need(!calls.create(error)&&purchase_calls==1&&set_calls==1,"same object replayed create after success");need(!calls.pin(*initial,false,error)&&purchase_calls==1&&set_calls==1,"same object re-pinned after success");pass("actual full adapter creates once, copies Host project and prevents same-object replay/re-pin");}
    {reset();physical.row->purchased=true;const auto initial=physical.row;StartingpointCalls calls{fixture_context,fixture_binding,{}};bool attempted=false,unknown=false;std::string error;need(startingpoint::apply(initial,target(),calls,attempted,unknown,error)&&!purchase_calls&&set_calls==1,"purchased record repeated PurchaseForFree: "+error);pass("already purchased owned record starts project without another purchase");}
    for(unsigned stage=0;stage<2;++stage){reset();const auto initial=physical.row;if(!stage)purchase_error=5;else set_error=7;StartingpointCalls calls{fixture_context,fixture_binding,{}};bool attempted=false,unknown=false;std::string error;need(!startingpoint::apply(initial,target(),calls,attempted,unknown,error)&&attempted&&unknown&&error==(!stage?"startingpoint original PurchaseForFree exception=5":"startingpoint original SetCurrentDevProject exception=7"),"native error lost attempted/Unknown or original code");const auto purchases=purchase_calls,sets=set_calls;need(!calls.create(error)&&!calls.pin(*initial,false,error)&&purchase_calls==purchases&&set_calls==sets,"partial native creation replayed through create or pin");}pass("both original-call failures stay Unknown and forbid local replay/re-pin");
    for(unsigned field=0;field<6;++field){reset();const auto initial=physical.row;after_set=[field]{switch(field){case 0:++economy_pin.values.lines[19][1];break;case 1:++economy_pin.values.calendar[5];break;case 2:++economy_pin.values.record[4];break;case 3:++economy_pin.history;break;case 4:++economy_pin.values.cash;break;case 5:++economy_pin.binding.native_generation;break;}};StartingpointCalls calls{fixture_context,fixture_binding,{}};bool attempted=false,unknown=false;std::string error;need(!startingpoint::apply(initial,target(),calls,attempted,unknown,error)&&attempted&&unknown&&error=="startingpoint creation changed current Economy owners/full ledger","full ledger or owner change was reduced to cash-only acceptance");}pass("actual full Pin equality rejects changed ledger/calendar/record/owner/fixture_binding");
    {reset();const auto initial=physical.row;after_set=[]{physical.creation->catalogue[0].art="changed";};StartingpointCalls calls{fixture_context,fixture_binding,{}};bool attempted=false,unknown=false;std::string error;need(!startingpoint::apply(initial,target(),calls,attempted,unknown,error)&&attempted&&unknown&&error.find("catalogue/Developer/native project copy")!=std::string::npos,"source catalogue changed invisibly");pass("owned catalogue source must remain unchanged after original creation");}
    for(unsigned field=0;field<3;++field){reset();const auto initial=physical.row;after_set=[field]{auto& row=*physical.creation->current;if(!field)row.key="other";else if(field==1)row.art="other";else ++row.statistic;};StartingpointCalls calls{fixture_context,fixture_binding,{}};bool attempted=false,unknown=false;std::string error;need(!startingpoint::apply(initial,target(),calls,attempted,unknown,error)&&attempted&&unknown,"copied native project field was unchecked");}pass("new original project copy must preserve source key/art/statistic");
    {reset();const auto initial=physical.row;after_set=[]{physical.creation->developers[1].job=6;};StartingpointCalls calls{fixture_context,fixture_binding,{}};bool attempted=false,unknown=false;std::string error;need(!startingpoint::apply(initial,target(),calls,attempted,unknown,error)&&attempted&&unknown,"original cancelled Developer state was not checked");pass("original Developer cancellation jobs5-to-7 must read back as idle");}
    {reset();const auto initial=physical.row;after_set=[]{physical.creation->developers[3].job=0;};StartingpointCalls calls{fixture_context,fixture_binding,{}};bool attempted=false,unknown=false;std::string error;need(!startingpoint::apply(initial,target(),calls,attempted,unknown,error)&&attempted&&unknown,"unrelated Developer job was changed");pass("untouched Developer jobs must remain unchanged");}
    {reset();const auto initial=physical.row;after_set=[]{physical.creation->release_changes.push_back(0x500000);};StartingpointCalls calls{fixture_context,fixture_binding,{}};bool attempted=false,unknown=false;std::string error;need(!startingpoint::apply(initial,target(),calls,attempted,unknown,error)&&attempted&&unknown,"Release membership changed during free purchase/project start");pass("actual Release list membership is part of full creation readback");}
    {reset();const auto initial=physical.row;physical.creation->automatic_workers=1;StartingpointCalls calls{fixture_context,fixture_binding,{}};bool attempted=false,unknown=false;std::string error;need(startingpoint::apply(initial,target(),calls,attempted,unknown,error)&&physical.row==target(),"original automatic worker policy baseline rejected: "+error);pass("original automatic worker reset is validated before Host work is copied");}
    {reset();const auto initial=physical.row;physical.creation->automatic_workers=1;physical.creation->total_workers=0;StartingpointCalls calls{fixture_context,fixture_binding,{}};bool attempted=false,unknown=false;std::string error;need(startingpoint::apply(initial,target(),calls,attempted,unknown,error)&&physical.row==target(),"original zero-worker temporary state rejected: "+error);pass("temporary original desired-worker minus1 is observed before Host values are copied");}
    {reset();const auto initial=physical.row;during_read=[](unsigned n){if(n==2)++physical.creation->catalogue_capacity;};StartingpointCalls calls{fixture_context,fixture_binding,{}};bool attempted=false,unknown=false;std::string error;need(!startingpoint::apply(initial,target(),calls,attempted,unknown,error)&&!attempted&&!unknown&&!purchase_calls&&!set_calls&&error=="startingpoint native row/owners changed before adoption","changing source owner passed double-read pin");pass("changed catalogue ownership rejects during pin before effects");}
    {reset();const auto initial=physical.row;physical.record=0;StartingpointCalls calls{fixture_context,fixture_binding,{}};bool attempted=false,unknown=false;std::string error;need(!startingpoint::apply(initial,target(),calls,attempted,unknown,error)&&!attempted&&!unknown&&!purchase_calls&&!set_calls,"missing owned TechRecord reached native calls");pass("missing actual TechRecord rejects before purchase");}
    {reset();const auto initial=physical.row;physical.project=0x600000;StartingpointCalls calls{fixture_context,fixture_binding,{}};bool attempted=false,unknown=false;std::string error;need(!startingpoint::apply(initial,target(),calls,attempted,unknown,error)&&!attempted&&!unknown&&!purchase_calls&&!set_calls,"different current project was replaced");pass("existing different project cannot be replaced by fixed creation");}
    {reset();const auto initial=physical.row;fail_economy_read=4;StartingpointCalls calls{fixture_context,fixture_binding,{}};bool attempted=false,unknown=false;std::string error;need(!startingpoint::apply(initial,target(),calls,attempted,unknown,error)&&attempted&&unknown&&error=="original physical Economy read failed","post-effect Economy read error was hidden");pass("post-effect physical Economy failure preserves original cause and Unknown");}
    {reset();const auto initial=physical.row;world_status.edit_operations=1;StartingpointCalls calls{fixture_context,fixture_binding,{}};bool attempted=false,unknown=false;std::string error;need(!startingpoint::apply(initial,target(),calls,attempted,unknown,error)&&!attempted&&!unknown&&!purchase_calls&&!set_calls&&error.find("Save/Edit idle")!=std::string::npos,"active native edit allowed creation");pass("actual Save/Edit idle guard runs before native creation");}
    {reset();const auto initial=physical.row;clock_status.time_scale=1;StartingpointCalls calls{fixture_context,fixture_binding,{}};bool attempted=false,unknown=false;std::string error;need(!startingpoint::apply(initial,target(),calls,attempted,unknown,error)&&!attempted&&!unknown&&!purchase_calls&&!set_calls&&error.find("Clock0")!=std::string::npos,"unfrozen Client clock admitted creation");pass("actual selected Client Clock0 remains mandatory before creation");}
    std::cout<<checks<<" actual creation adapter scenarios passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
