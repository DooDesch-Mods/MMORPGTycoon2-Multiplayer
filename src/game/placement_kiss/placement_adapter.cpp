#include "placement_adapter.h"
#include <cmath>
#include <exception>
#include <limits>

namespace placement_kiss {
static bool reject(std::string& cause, const char* text) { cause=text; return false; }
bool valid_request(const Request& r, std::string& cause) {
    if(r.action_uuid.size()!=36 || !r.world || !r.epoch || !r.generation || !r.sequence) return reject(cause,"error: placement UUID/world/epoch/generation/sequence required");
    for(std::size_t i=0;i<36;++i) { const char c=r.action_uuid[i]; if(i==8||i==13||i==18||i==23) { if(c!='-') return reject(cause,"error: placement UUID separator invalid"); } else if(!((c>='0'&&c<='9')||(c>='a'&&c<='f'))) return reject(cause,"error: placement UUID must use lowercase hex"); }
    return valid_scalars(r,cause);
}
bool valid_scalars(const Request& r, std::string& cause) {
    if(r.type.empty()||r.type.size()>256||r.type.find('\0')!=std::string::npos) return reject(cause,"error: native type identifier invalid");
    if(r.variant.empty()||r.variant.size()>256||r.variant.find('\0')!=std::string::npos) return reject(cause,"error: native variant identifier invalid");
    for(float v:r.pose.position) if(!std::isfinite(v)) return reject(cause,"error: native position is not finite");
    float norm=0;
    for(float v:r.pose.quaternion) { if(!std::isfinite(v)) return reject(cause,"error: native quaternion is not finite"); norm+=v*v; }
    if(!std::isfinite(norm)||std::abs(norm-1.0f)>0.001f) return reject(cause,"error: native quaternion must be normalized");
    for(float v:r.pose.scale) if(v!=1.0f) return reject(cause,"error: normal building candidate requires native unit scale");
    return true;
}
bool valid_receipt(const Request& r, const Receipt& v, std::string& cause) {
    if(v.world!=r.world||v.epoch!=r.epoch||v.generation!=r.generation||v.sequence!=r.sequence||v.action_uuid!=r.action_uuid) return reject(cause,"error: native placement receipt binding differs");
    if(v.exception) return reject(cause,"error: actual native receipt reports a partial failure");
    if(v.purchase_calls!=1||v.add_calls!=1||v.undo_calls!=1||!v.native_uid) return reject(cause,"error: actual Purchase/AddBuilding/Undo/new UID receipts are incomplete");
    // Native placement adjusts terrain height; the Host publishes the final registered pose.
    if(v.type!=r.type||v.variant!=r.variant||v.pose.position[0]!=r.pose.position[0]||
       v.pose.position[2]!=r.pose.position[2]||!std::isfinite(v.pose.position[1])||
       v.pose.quaternion!=r.pose.quaternion||v.pose.scale!=r.pose.scale)
        return reject(cause,"error: actual registered building identity/pose differs");
    if(v.native_cost<0||v.cash_before<std::numeric_limits<std::int64_t>::min()+v.native_cost||v.cash_after!=v.cash_before-v.native_cost) return reject(cause,"error: actual native cost/cash delta differs");
    return true;
}
Result Adapter::execute(const Request& r) {
    Result result;
    if(active_.test_and_set()) { result.cause="error: native placement reentry rejected"; services_.record_result(r,result); return result; }
    PrivateBuild build;
    void* shard=nullptr;
    void* prepared_shard=nullptr;
    bool prepared=false;
    try {
        if(!valid_request(r,result.cause)||!services_.read_current_context(r,shard,result.cause)||!shard||!services_.reserve_action_once(r,result.cause)) {
            if(result.cause.empty()) result.cause="error: native placement context or shard unavailable";
        } else {
            result.phase=Phase::Unknown;
            prepared_shard=shard;
            prepared=services_.prepare_private_build(r,shard,build,result.cause);
            result.exception=build.exception;
            if(prepared) {
                result.phase=Phase::Prepared;
                if(services_.read_current_context(r,shard,result.cause)&&shard==prepared_shard) {
                    result.phase=Phase::Attempted;
                    result.original_returned=services_.call_original_update(build,result.exception);
                    result.original_attempted=build.stage==16||build.stage==17||build.stage==18;
                    if(result.original_attempted&&result.original_returned&&!result.exception&&services_.read_actual_receipt(r,result.receipt,result.cause)&&valid_receipt(r,result.receipt,result.cause)) result.phase=Phase::Confirmed;
                    else { result.phase=Phase::Unknown; if(result.cause.empty()) result.cause="error: original native Update failed or its effects remain unacknowledged"; }
                } else { result.phase=Phase::Unknown; if(result.cause.empty()) result.cause="error: world changed before original native Update; cleanup is unsafe"; }
            } else if(result.cause.empty()) result.cause="error: native private construction failed; partial receiver retained";
        }
    } catch(const std::exception& e) { result.original_attempted=build.stage==16||build.stage==17||build.stage==18; result.phase=Phase::Unknown; result.cause=std::string("error: placement service exception: ")+e.what(); }
    catch(...) { result.original_attempted=build.stage==16||build.stage==17||build.stage==18; result.phase=Phase::Unknown; result.cause="error: placement service threw an unknown exception"; }
    if(prepared && result.phase!=Phase::Unknown) {
        try { std::uint32_t exception=0; if(!services_.read_current_context(r,shard,result.cause)||shard!=prepared_shard) { result.phase=Phase::Unknown; if(result.cause.empty()) result.cause="error: world changed before original native private Build D0"; } else { result.destructor_returned=services_.destroy_private_build_once(build,exception); result.destructor_attempted=build.stage==17||build.stage==18; if(!result.destructor_attempted||!result.destructor_returned||exception) { result.phase=Phase::Unknown; result.exception=exception; result.cause="error: original native private Build D0 failed; no cleanup replay"; } } }
        catch(const std::exception& e) { result.destructor_attempted=build.stage==17||build.stage==18; result.phase=Phase::Unknown; result.cause=std::string("error: original native cleanup exception: ")+e.what(); }
        catch(...) { result.destructor_attempted=build.stage==17||build.stage==18; result.phase=Phase::Unknown; result.cause="error: original native cleanup threw an unknown exception"; }
    }
    if(result.phase==Phase::Unknown) result.cause += "; private receiver retained, action cannot replay";
    services_.record_result(r,result);
    active_.clear();
    return result;
}
}
