#include "mt2mp/core/runtime_paths.h"
#include "runtime.h"
#include "marketing_authority/notice_hook.h"
#include "native_pins.h"
#include "subscriber_lifecycle/append_calls.h"
#include "subscriber_lifecycle/client_world.h"
#include "subscriber_lifecycle/native_codes.h"
#include "subscriber_lifecycle/save_scope.h"
#include "mt2mp/core/hash.h"
#include <array>
#include <atomic>
#include <bit>
#include <fstream>
#include <map>
#include <memory>
#include <regex>
#include <Windows.h>

namespace mt2mp::game::marketing_native_probe {
namespace {
namespace local=mt2mp::subscriber_lifecycle;
namespace stage=local::first_append;
class ProcessMemory final:public Memory {public:bool read(uintptr_t p,void* v,size_t n,std::string& e)override{
    if(!p||p>UINTPTR_MAX-n){e="Marketing memory extent invalid";return false;}SIZE_T copied=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),v,n,&copied)||copied!=n){e="Marketing ReadProcessMemory error="+std::to_string(GetLastError());return false;}return true;
}} memory;
bool fail(std::string& e,const std::string& s){e=s;return false;}
enum class Op {Allocate,AwardConstruct,AwardLoad,AwardAdd,CampaignStart,CampaignLoad,ContextConstruct,ContextResolveDestroy,Rebuild};
struct Call {Op op;uintptr_t image=0,target=0,record=0,context=0,result=0;bool accepted=false;};
uint32_t invoke(Call* c){
    __try{
        const auto p=reinterpret_cast<void*>(c->target);
        switch(c->op){
        case Op::Allocate:c->result=reinterpret_cast<uintptr_t>(reinterpret_cast<void*(*)(size_t)>(c->image+0xa02670)(0xa8));break;
        case Op::AwardConstruct:reinterpret_cast<void(*)(void*)>(c->image+0x51a9b0)(p);break;
        case Op::AwardLoad:reinterpret_cast<void(*)(void*,void*,void*)>(c->image+0xceabb0)(p,reinterpret_cast<void*>(c->record),reinterpret_cast<void*>(c->context));break;
        case Op::AwardAdd:reinterpret_cast<void(*)(void*,void*)>(c->image+0xb5d8d0)(p,reinterpret_cast<void*>(c->result));break;
        case Op::CampaignStart:c->accepted=reinterpret_cast<bool(*)(void*,int32_t,int32_t)>(c->image+0x51c640)(p,4,5);break;
        case Op::CampaignLoad:reinterpret_cast<void(*)(void*,void*,void*)>(c->image+0xc7cf80)(p,reinterpret_cast<void*>(c->record),reinterpret_cast<void*>(c->context));break;
        case Op::ContextConstruct:reinterpret_cast<void(*)(void*)>(c->image+0x89e940)(p);break;
        case Op::ContextResolveDestroy:reinterpret_cast<void(*)(void*)>(c->image+0x89e480)(p);break;
        case Op::Rebuild:reinterpret_cast<void(*)(void*)>(c->image+0x663880)(p);break;
        }
    }__except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    return 0;
}
struct Owners {
    stage::NativeTree award_record,campaign_record;
    stage::DecodeReceipt award_decode,campaign_decode;
    uintptr_t award=0,campaign=0,award_context=0,campaign_context=0;
    bool award_handed_off=false,campaign_handed_off=false;
    std::unique_ptr<Owners> next;
};
std::unique_ptr<Owners> retained;
std::atomic_flag busy=ATOMIC_FLAG_INIT;
std::map<std::string,Json> receipts;
std::string attempted_world;
struct Work {
    stage::ClientWorld original;
    stage::DecodeApi records;
    Payload payload;
    Snapshot before;
    native::SimulationState money;
    native::SoftwareState software;
    std::string origin,local_sha;
    Json* receipt=nullptr;
    Owners* owners=nullptr;
    bool called=false,unknown=false;
};
bool call(Work& w,Call& c,const char* name,std::string& e){
    auto& out=(*w.receipt)["calls"][name];out={{"attempted",true},{"returned",false}};w.called=true;
    try{const auto seh=invoke(&c);out["seh"]=seh;if(seh){w.unknown=true;return fail(e,std::string(name)+" SEH="+std::to_string(seh));}}
    catch(const std::exception& ex){w.unknown=true;return fail(e,std::string(name)+" C++: "+ex.what());}
    catch(...){w.unknown=true;return fail(e,std::string(name)+" unknown C++ exception");}
    out["returned"]=true;return true;
}
bool read(Work& w,bool held,Snapshot& out,std::string& e){
    stage::ClientWorld current;
    if(!stage::read_client_world(memory,w.local_sha,w.origin,held,current,e))return false;
    if(!stage::same_client_owner(w.original,current)||current.census.members!=w.original.census.members||current.census.online!=w.original.census.online||
       current.census.offline!=w.original.census.offline||current.census.next_id!=w.original.census.next_id)return fail(e,"Marketing current world/Subscriber cohort changed");
    const auto state=reinterpret_cast<uintptr_t>(current.origin.state),mode=reinterpret_cast<uintptr_t>(current.origin.mode);
    Snapshot first;
    if(!read_snapshot(memory,current.image,state,mode,first,e)||!read_snapshot(memory,current.image,state,mode,out,e))return false;
    return first==out||fail(e,"Marketing double-read lifetime/values changed");
}
bool unchanged(Work& w,ClockPhase phase,std::string& e){
    native::SimulationState now;native::SoftwareState software;
    if(!native::read_simulation_state(now,e)||!native::read_software_state(software,e))return false;
    const auto& a=w.money;const auto& b=now;
    (*w.receipt)["finance_after"]={{"cash",b.cash},{"record_present",b.record_present},{"record_day",b.record_day},{"record_cash",b.record_cash},
        {"record_income",b.record_income},{"record_expense",b.record_expense},{"line_counts",b.line_counts},{"line_income",b.line_income},{"logged_in",b.logged_in_subscribers},
        {"rating_bits",std::bit_cast<uint32_t>(software.rating)},{"fun_bits",std::bit_cast<uint32_t>(software.fun)}};
    if(a.cash!=b.cash||a.record_present!=b.record_present||a.record_day!=b.record_day||a.record_cash!=b.record_cash||a.record_income!=b.record_income||
       a.record_expense!=b.record_expense||a.line_counts!=b.line_counts||a.line_income!=b.line_income||a.logged_in_subscribers!=b.logged_in_subscribers||
       std::bit_cast<uint32_t>(w.software.rating)!=std::bit_cast<uint32_t>(software.rating)||std::bit_cast<uint32_t>(w.software.fun)!=std::bit_cast<uint32_t>(software.fun))return fail(e,"Marketing insertion changed finance/Software cache");
    const auto sample=[](const native::ClockState& c){return ClockSample{c.flat_day,c.seconds,c.time_scale,c.pause_scale,c.paused};};
    Json diagnostic;const bool clock_ok=same_clock(sample(a.clock),sample(b.clock),phase,diagnostic,e);
    (*w.receipt)["clock_checks"].push_back(std::move(diagnostic));
    if(!clock_ok)return false;
    return current_campaign(b.clock.flat_day,2,5,1,e);
}
bool owners_same(Work& w,const Snapshot& s,std::string& e,ClockPhase phase=ClockPhase::Held){
    const auto& b=w.before;
    if(s.marketing!=b.marketing||s.clock!=b.clock||s.hud!=b.hud||s.window!=b.window||s.width!=b.width||s.height!=b.height)return fail(e,"Marketing bound owner/Clock/BoostWindow changed");
    return unchanged(w,phase,e);
}
bool exit_guard(void* p,std::string& e){auto& w=*static_cast<Work*>(p);Snapshot s;return read(w,true,s,e)&&owners_same(w,s,e);}
bool pins(uintptr_t image,std::string& e){
    for(const auto& pin:native_pins){if(pin.rva==0x518290){if(!marketing_authority::notice_hook::verify(image,e))return false;continue;}std::vector<unsigned char> bytes(pin.size);
        if(!memory.read(image+pin.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=pin.sha)return fail(e,"Marketing native body SHA differs RVA="+std::to_string(pin.rva));}
    for(const auto rva:{uintptr_t(0x89e940),uintptr_t(0x89e480),uintptr_t(0x4a03a0),uintptr_t(0x49f6f0)}){
        bool found=false;for(const auto& code:stage::native_codes)if(code.rva==rva){found=true;std::array<unsigned char,16> bytes{};
            if(!memory.read(image+rva,bytes.data(),bytes.size(),e))return false;
            if(bytes!=code.bytes)return fail(e,"Marketing Context/SaveScope code differs RVA="+std::to_string(rva));}
        if(!found)return fail(e,"Marketing required original code pin missing");
    }
    for(const auto& pair:{std::pair<uintptr_t,uintptr_t>{0x1749880+0x70,0xceabb0},{0x1749880+0x80,0xb63070},
                         {0x16e3d50+0x70,0xc7cf80},{0x16e3d50+0x80,0x5199e0}}){uintptr_t target=0;
        if(!memory.read(image+pair.first,&target,sizeof target,e))return false;if(target!=image+pair.second)return fail(e,"Marketing typed Load/Resolve virtual slot differs");}
    return true;
}
bool body_inner(Work& w,std::string& e){
    auto& o=*w.owners;const auto image=w.original.image;Snapshot s;
    if(!read(w,true,s,e)||s!=w.before||!baseline(s,e)||!owners_same(w,s,e))return fail(e,e.empty()?"Marketing held expected-before changed":e);
    if(!stage::decode_marketing_record(w.records,memory,stage::MarketingRecordKind::Award,w.payload.award,o.award_record,o.award_decode,e)||
       !stage::decode_marketing_record(w.records,memory,stage::MarketingRecordKind::Campaign,w.payload.campaign,o.campaign_record,o.campaign_decode,e))return false;
    Call c{Op::Allocate,image};if(!call(w,c,"award_allocate",e))return false;o.award=c.result;
    if(!o.award)return fail(e,"Marketing Award allocation returned null");
    c={Op::AwardConstruct,image,o.award};if(!call(w,c,"award_construct",e))return false;
    c={Op::ContextConstruct,image,reinterpret_cast<uintptr_t>(&o.award_context)};if(!call(w,c,"award_context_construct",e))return false;
    c={Op::AwardLoad,image,o.award,o.award_record.root,reinterpret_cast<uintptr_t>(&o.award_context)};if(!call(w,c,"award_load",e))return false;
    c={Op::ContextResolveDestroy,image,reinterpret_cast<uintptr_t>(&o.award_context)};if(!call(w,c,"award_context_resolve_destroy",e))return false;o.award_context=0;
    if(!read(w,true,s,e)||s!=w.before||!owners_same(w,s,e))return fail(e,e.empty()?"Marketing before Award handoff changed":e);
    c={Op::AwardAdd,image,s.marketing+0x38};c.result=o.award;
    if(!call(w,c,"award_add",e))return false;
    if(!read(w,true,s,e)||s.awards!=std::vector<uintptr_t>{o.award}||s.campaigns!=w.before.campaigns)return fail(e,e.empty()?"Marketing Award handoff readback differs":e);
    o.award_handed_off=true;(*w.receipt)["award_handoff_confirmed"]=true;
    if(!owners_same(w,s,e))return false;
    c={Op::CampaignStart,image,s.marketing};if(!call(w,c,"campaign_start",e))return false;
    (*w.receipt)["calls"]["campaign_start"]["accepted"]=c.accepted;
    if(!c.accepted)return fail(e,"Marketing original StartAdCampaign returned false after Award insertion");
    if(!read(w,true,s,e)||s.awards!=std::vector<uintptr_t>{o.award}||s.campaigns.size()!=2||s.campaigns[0]!=w.before.campaigns[0]||s.campaigns[1]==s.campaigns[0])return fail(e,e.empty()?"Marketing StartAdCampaign fresh row admission differs":e);
    o.campaign=s.campaigns[1];o.campaign_handed_off=true;(*w.receipt)["campaign_handoff_confirmed"]=true;
    if(!owners_same(w,s,e)||s.values["campaigns"][1]!=expected_values(true)["campaigns"][1])return fail(e,e.empty()?"Marketing fresh Campaign Start fields differ":e);
    c={Op::ContextConstruct,image,reinterpret_cast<uintptr_t>(&o.campaign_context)};if(!call(w,c,"campaign_context_construct",e))return false;
    // D1 resolves registered objects once. Keep the pinned current-day branch true before Load and D1.
    if(!read(w,true,s,e)||!owners_same(w,s,e))return false;
    c={Op::CampaignLoad,image,o.campaign,o.campaign_record.root,reinterpret_cast<uintptr_t>(&o.campaign_context)};if(!call(w,c,"campaign_load",e))return false;
    if(!read(w,true,s,e)||!owners_same(w,s,e)||s.values!=expected_values(true))return fail(e,e.empty()?"Marketing pre-Resolve target Record readback differs":e);
    c={Op::ContextResolveDestroy,image,reinterpret_cast<uintptr_t>(&o.campaign_context)};if(!call(w,c,"campaign_context_resolve_destroy",e))return false;o.campaign_context=0;
    if(!read(w,true,s,e)||!owners_same(w,s,e)||s.values!=expected_values(true))return fail(e,e.empty()?"Marketing original Resolve readback differs":e);
    c={Op::Rebuild,image,w.before.window};if(!call(w,c,"boost_rebuild",e))return false;
    if(!read(w,true,s,e))return false;(*w.receipt)["after"]=report(s);
    return completed(w.before,s,o.award,o.campaign,e)&&owners_same(w,s,e);
}
bool body(void* p,std::string& e){
    auto& w=*static_cast<Work*>(p);bool ok=false;
    try{ok=body_inner(w,e);}catch(const std::exception& ex){e="Marketing body C++: "+std::string(ex.what());w.unknown=true;}
    catch(...){e="Marketing body unknown C++ exception";w.unknown=true;}
    auto& o=*w.owners;
    if(!ok){w.unknown=w.unknown||w.called||o.award_decode.allocated||o.campaign_decode.allocated;return false;}
    if(!stage::destroy_records(w.records,o.award_record,o.award_decode,e)||!stage::destroy_records(w.records,o.campaign_record,o.campaign_decode,e)){w.unknown=true;return false;}
    (*w.receipt)["native_record_cleanup_confirmed"]=true;return true;
}
bool valid_args(const Json& a,std::string& e){
    if(!a.is_object()||a.size()!=7)return fail(e,"Marketing probe requires seven exact arguments");
    for(const auto key:{"artifact_basename","artifact_sha256","native_origin_uuid","native_origin_sha256","token","native_epoch","native_generation"})if(!a.contains(key))return fail(e,"Marketing probe argument missing");
    const std::regex uuid("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}");
    for(const auto key:{"native_origin_uuid","token"})if(!a[key].is_string()||!std::regex_match(a[key].get<std::string>(),uuid))return fail(e,"Marketing probe UUID invalid");
    if(!a["artifact_basename"].is_string()||!std::regex_match(a["artifact_basename"].get<std::string>(),std::regex("[a-z0-9-]{1,155}\\.json"))||
       !a["artifact_sha256"].is_string()||!std::regex_match(a["artifact_sha256"].get<std::string>(),std::regex("[0-9a-f]{64}"))||
       !a["native_origin_sha256"].is_string()||!std::regex_match(a["native_origin_sha256"].get<std::string>(),std::regex("[0-9a-f]{64}")))return fail(e,"Marketing private artifact/native origin name/SHA invalid");
    for(const auto key:{"native_epoch","native_generation"})if(!a[key].is_number_unsigned()||a[key].get<uint64_t>()==0)return fail(e,"Marketing native epoch/generation invalid");
    return true;
}
}
bool probe(const std::filesystem::path& root,const Json& args,Json& result,std::string& e){
    result={{"state","rejected"},{"replay_permitted",false},{"calls",Json::object()}};
    if(busy.test_and_set())return fail(e,"Marketing probe already running");struct Busy{~Busy(){busy.clear();}} release;
    Work w;w.receipt=&result;auto owners=std::make_unique<Owners>();w.owners=owners.get();local::SaveScopeReceipt scope;
    bool reserved=false,ok=false;std::string token;
    try{
        if(!valid_args(args,e))return false;token=args["token"].get<std::string>();
        if(auto found=receipts.find(token);found!=receipts.end()){result=found->second;result["replay_rejected"]=true;return fail(e,"Marketing token already attempted; retained receipt returned without replay");}
        const auto path=runtime_paths::diagnostics(root)/"marketing-records"/args["artifact_basename"].get<std::string>();
        if(std::filesystem::file_size(path)>32768)return fail(e,"Marketing private artifact exceeds 32 KiB");
        std::ifstream file(path,std::ios::binary);if(!file)return fail(e,"Marketing private artifact open failed");
        const std::string content((std::istreambuf_iterator<char>(file)),{});
        if(file.bad()||mt2mp::sha256(std::vector<unsigned char>(content.begin(),content.end()))!=args["artifact_sha256"].get<std::string>())return fail(e,"Marketing artifact readback SHA differs");
        if(!parse_payload(Json::parse(content),w.payload,e))return false;w.origin=args["native_origin_uuid"].get<std::string>();w.local_sha=args["native_origin_sha256"].get<std::string>();
        if(!stage::read_client_world(memory,w.local_sha,w.origin,false,w.original,e))return false;
        if(w.original.origin.world_epoch!=args["native_epoch"].get<uint64_t>()||w.original.origin.generation!=args["native_generation"].get<uint64_t>())return fail(e,"Marketing current loaded world epoch/generation differs");
        const auto world=w.origin+":"+std::to_string(w.original.origin.world_epoch)+":"+std::to_string(w.original.origin.generation);
        if(attempted_world==world)return fail(e,"Marketing world already attempted; new tokens cannot replay it");
        if(!pins(w.original.image,e)||!stage::resolve_record_api(memory,w.original.image,w.records,e)||!read(w,false,w.before,e)||!baseline(w.before,e)||
           !native::read_simulation_state(w.money,e)||!native::read_software_state(w.software,e)||!unchanged(w,ClockPhase::Before,e))return false;
        result["before"]=report(w.before);result["finance_before"]=result["finance_after"];result["clock"]={{"day",w.money.clock.flat_day},{"seconds",w.money.clock.seconds},
            {"time_scale",w.money.clock.time_scale},{"pause_scale",w.money.clock.pause_scale},{"paused",w.money.clock.paused}};
        result["token"]=token;result["native_origin_uuid"]=w.origin;result["native_origin_sha256"]=w.local_sha;result["native_epoch"]=w.original.origin.world_epoch;result["native_generation"]=w.original.origin.generation;
        result["baseline_sha256"]=baseline_sha;result["source_sha256"]=source_sha;result["artifact_sha256"]=args["artifact_sha256"];
        if(receipts.size()>=64)return fail(e,"Marketing retained receipt capacity reached");
        result["state"]="attempted";receipts.emplace(token,result);attempted_world=world;reserved=true;
        const local::SaveScopeApi api{reinterpret_cast<local::SaveScopeCall>(w.original.image+0x4a03a0),reinterpret_cast<local::SaveScopeCall>(w.original.image+0x49f6f0)};
        ok=local::with_save_scope(api,w.original.origin.mode,body,exit_guard,&w,scope,e);
        result["save_scope"]={{"enter_attempted",scope.enter_attempted},{"enter_returned",scope.enter_returned},{"leave_attempted",scope.leave_attempted},
            {"leave_returned",scope.leave_returned},{"exit_guard_confirmed",scope.exit_guard_confirmed},{"unknown",scope.unknown},{"body_cause",scope.body_cause},{"cleanup_cause",scope.cleanup_cause}};
        if(ok){Snapshot closed;ok=read(w,false,closed,e)&&completed(w.before,closed,owners->award,owners->campaign,e)&&owners_same(w,closed,e,ClockPhase::Closed);
            if(ok){result["after"]=report(closed);result["closed_world_readback_confirmed"]=true;}}
    }catch(const std::exception& ex){e="Marketing probe exception: "+std::string(ex.what());ok=false;w.unknown=reserved;}
    catch(...){e="Marketing probe unknown C++ exception";ok=false;w.unknown=true;}
    if(!ok&&e.empty())e="Marketing probe condition failed without confirmed readback";
    w.unknown=w.unknown||scope.unknown||(!ok&&reserved);
    result["state"]=ok?"confirmed":w.unknown?"unknown":"rejected";result["cause"]=ok?"":e;
    result["award_handoff_confirmed"]=owners->award_handed_off;result["campaign_handoff_confirmed"]=owners->campaign_handed_off;
    result["private_owners_retained"]=w.unknown;result["business_unchanged"]=ok;result["software_unchanged"]=ok;
    if(w.unknown){owners->next=std::move(retained);retained=std::move(owners);}
    if(reserved)receipts[token]=result;return ok;
}
}
