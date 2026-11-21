#include "retire.h"
#include "developer_staff/probe.h"
#include "developer_staff/entry_fingerprints.h"
#include "placement_ui_probe/ui_probe.h"
#include "mt2mp/core/hash.h"
#include <map>
#include <cmath>
namespace mt2mp::game::developer_staff::retirement {
namespace {
placement_ui_probe::ProcessMemory memory;
std::map<std::string,nlohmann::json> receipts;
bool locked=false;
bool fail(std::string& e,const std::string& why){e=why;return false;}
template<class T>bool at(uintptr_t owner,size_t offset,T& out,std::string& e){
    if(!owner||owner>UINTPTR_MAX-offset||owner+offset>UINTPTR_MAX-sizeof(T))return fail(e,"Developer retirement field address overflow");
    return memory.copy(owner+offset,&out,sizeof(T),e);
}
bool detail(const Capture& c,size_t slot,Details& d,std::string& e){
    if(slot>=c.local.developers.size())return fail(e,"Developer retirement slot absent");const auto p=c.local.developers[slot];
    return at(p,0x4c8,d.job,e)&&at(p,0x4d0,d.carried,e)&&at(p,0x488,d.quadtree,e)&&at(p,0x338,d.current,e)&&at(p,0x340,d.current_index,e)&&
        at(p,0x348,d.destination,e)&&at(p,0x350,d.destination_index,e)&&at(p,0x269,d.moving,e)&&at(p,0x2b0,d.tick_delay,e);
}
bool same(const Capture& a,const Capture& b){return same_origin(a.pin.origin,b.pin.origin)&&a.pin.ledger==b.pin.ledger&&a.pin.time_scale==b.pin.time_scale&&
    a.pin.pause_scale==b.pin.pause_scale&&a.image==b.image&&a.thread==b.thread&&a.state==b.state&&a.local==b.local;}
bool snapshot(size_t slot,Capture& c,Details& d,std::string& e){
    Capture last;Details again;
    if(!read_process(c,e)||!detail(c,slot,d,e)||!read_process(last,e)||!detail(last,slot,again,e))return false;
    return (same(c,last)&&d==again)||fail(e,"Developer retirement extended double census changed");
}
bool code(const Capture& c,size_t slot,std::string& e){
    constexpr std::array<Fingerprint,8> bodies={{{0xd30ff0,32,"cfecd549f2e6fc6c689856fae8276d52e3e1b19f3f7eca1a5cb61d01400d9f6f"},
        {0x8aad90,128,"9e2324bb66601dcf36f4a52631bb4e83b6af61890ab9aca15f971d2f0d246370"},
        {0x3a9e30,1088,"6612b20d09477ad64dff5c7d9e9c1cfc89045622529c0adc1e7641c8e3fa77c1"},
        {0x3b5670,80,"28e7d924c8998107c1b463e0583e4e68a28da2759d19f26bb1e8650c03757758"},
        {0x3b6710,176,"47efd3d68295a556ba32434e985f0986acc4720121bfb587ae427f87496bc94b"},
        {0x261c00,48,"82126880703d8c8a8c813c8d72adc79985100c8482bfea94cb48c6aba370e0e8"},
        {0x264180,336,"63a5aa89a427a868cb7f44bfeb792fa8b49e877f8e006e0124a929a5fa0b41b2"},
        {0x264420,224,"bd4d79952606a1033735ff4c6df3a552caeca941691dca7ac662486f7a11980f"}}};
    try{for(const auto& b:bodies){std::vector<unsigned char> bytes(b.size);if(!memory.copy(c.image+b.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=b.sha256)return fail(e,"Developer retirement original full body differs RVA="+std::to_string(b.rva));}}
    catch(const std::exception& ex){return fail(e,std::string("Developer retirement body hash failed: ")+ex.what());}
    uintptr_t property=0,offset=0,ap=0,set_map=0;
    if(!at(c.image,0x13d4f40,property,e)||!at(c.image,0x13d4f78,offset,e)||!at(c.local.developers[slot],0,ap,e)||!at(ap,0x290,set_map,e))return false;
    return (property==c.image+0x16d87b0&&offset==0x4d9&&ap==c.image+0x1748680&&set_map==c.image+0x3b6710)||fail(e,"Developer retirement exact active property/SetOnMap binding differs");
}
nlohmann::json encode_detail(const Details& d){return {{"job_present",d.job!=0},{"carried_present",d.carried!=0},{"quadtree_present",d.quadtree!=0},
    {"moving",d.moving},{"current_present",d.current!=0},{"current_index",d.current_index},{"destination_present",d.destination!=0},
    {"destination_index",d.destination_index},{"tick_delay",d.tick_delay}};}
}
bool observe_fixture(nlohmann::json& out,std::string& e){
    out={{"observed",false}};Capture authority;uintptr_t window=0;
    if(!authorize_fixture(authority,window,e))return false;
    out={{"observed",true},{"fixture_only",true},{"peer_ack",false},{"locked",locked},{"current",encode(authority)},
        {"details",nlohmann::json::array()},{"receipts",nlohmann::json::array()}};
    for(size_t slot=0;slot<50;++slot){Details d;if(!detail(authority,slot,d,e)){out["observed"]=false;return false;}
        auto row=encode_detail(d);row["slot"]=slot;row["id"]=authority.state.rows[slot].id;out["details"].push_back(row);}
    Capture after;if(!read_process(after,e)||!same(authority,after)){out["observed"]=false;return fail(e,e.empty()?"Developer retirement observation changed actual50 census":e);}
    for(const auto& [token,receipt]:receipts){(void)token;out["receipts"].push_back(receipt);}return true;
}
bool run_fixture(const nlohmann::json& args,nlohmann::json& out,std::string& e){
    out={{"fixture_only",true},{"peer_ack",false},{"native_attempted",false},{"closure_confirmed",false},{"unknown",false}};e.clear();
    Capture authority;uintptr_t window=0;if(!authorize_fixture(authority,window,e))return false;
    if(!args.is_object()||args.size()!=7||!args.contains("token")||!args.contains("native_origin_uuid")||!args.contains("epoch")||!args.contains("generation")||
       !args.contains("expected_count")||!args.contains("slot")||!args.contains("id")||!args.at("token").is_string()||!args.at("native_origin_uuid").is_string()||
       !args.at("epoch").is_number_unsigned()||!args.at("generation").is_number_unsigned()||!args.at("expected_count").is_number_integer()||
       !args.at("slot").is_number_integer()||!args.at("id").is_number_integer())return fail(e,"Developer retirement exact seven typed fields required");
    const auto token=args.at("token").get<std::string>();const auto slot=args.at("slot").get<int64_t>(),id=args.at("id").get<int64_t>(),count=args.at("expected_count").get<int64_t>();
    if(token.empty()||token.size()>128||slot<0||slot>=50||id<0||id>=50||count<0||count>50)return fail(e,"Developer retirement token/slot/id/count bounds differ");
    if(receipts.contains(token)){out=receipts.at(token);out["replay_rejected"]=true;return fail(e,"Developer retirement token already retained; no replay executed");}
    if(locked)return fail(e,"Developer retirement fixture locked after uncertain mutation; inspect retained receipt before recovery");
    if(receipts.size()>=64)return fail(e,"Developer retirement fixture receipt capacity64 reached");
    Capture before,again;Details old,recheck;Plan plan;
    if(!snapshot(size_t(slot),before,old,e)||!same(authority,before)||!prepare(before,old,size_t(slot),int32_t(id),plan,e))return fail(e,e.empty()?"Developer retirement admission census changed":e);
    if(args.at("native_origin_uuid")!=before.pin.origin.uuid||args.at("epoch").get<uint64_t>()!=before.pin.origin.world_epoch||args.at("generation").get<uint64_t>()!=before.pin.origin.generation||count!=before.state.business_count)
        return fail(e,"Developer retirement stale Origin/epoch/generation/count");
    if(!code(before,size_t(slot),e)||!authorize_fixture(again,window,e)||!snapshot(size_t(slot),again,recheck,e)||!same(before,again)||old!=recheck)
        return fail(e,e.empty()?"Developer retirement final native admission changed":e);
    out["token"]=token;out["slot"]=slot;out["id"]=id;out["route"]=plan.route==Route::IndoorIdle?"indoor_idle":"original_arrival";
    out["before"]=encode(before);out["before_details"]=encode_detail(old);out["native_call_pending"]=true;out["unknown"]=true;receipts.emplace(token,out);
    // Keep the latch closed if native execution or later readback/serialization throws.
    locked=true;Call call;
    try{
    call.exception=invoke_original(before.image,before.local.developers[size_t(slot)],plan.route,&call);
    out["native_call_pending"]=false;
    out["native_attempted"]=call.attempted;out["record_confirmed"]=call.record_confirmed;out["arrival_returned"]=call.arrival_returned;
    out["property_returned"]=call.property_returned;out["property_accepted"]=call.property_accepted;out["cleanup_confirmed"]=call.cleanup_confirmed;
    out["exception"]=call.exception;out["cleanup_exception"]=call.cleanup_exception;
    bool ok=!call.exception&&!call.cleanup_exception&&call.record_confirmed&&call.property_returned&&call.property_accepted&&call.cleanup_confirmed;
    if(!ok)e="Developer retirement original call/record cleanup not confirmed; SEH="+std::to_string(call.exception)+" cleanup="+std::to_string(call.cleanup_exception);
    Capture after;Details now;std::string read_error;
    const bool observed=snapshot(size_t(slot),after,now,read_error);
    if(observed){out["after"]=encode(after);out["after_details"]=encode_detail(now);if(ok)ok=effect(before,old,after,now,plan,e);}
    else {ok=false;e+=(e.empty()?"":"; ")+read_error;}
    out["closure_confirmed"]=ok;out["unknown"]=!ok&&(call.attempted||call.exception||call.cleanup_exception);out["cause"]=e;
    receipts.at(token)=out;
    if(ok||(!call.attempted&&!call.exception&&!call.cleanup_exception))locked=false;
    return ok;
    }catch(const std::exception& ex){
        e=std::string("Developer retirement post-admission exception: ")+ex.what();
        out["native_attempted"]=call.attempted;out["unknown"]=true;out["closure_confirmed"]=false;out["cause"]=e;receipts.at(token)=out;return false;
    }catch(...){
        e="Developer retirement post-admission unknown C++ exception";
        out["native_attempted"]=call.attempted;out["unknown"]=true;out["closure_confirmed"]=false;out["cause"]=e;receipts.at(token)=out;return false;
    }
}
}
