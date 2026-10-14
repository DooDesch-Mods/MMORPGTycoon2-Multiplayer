#include "native_free_diagnostics.h"
#include "object_identity/native_free_runtime/native_free_runtime.h"
#include "native_open_runtime.h"
#include "mt2mp/core/logger.h"
#include <map>
#include <regex>
#include <Windows.h>

namespace mt2mp::game::native_free_diagnostics {
namespace {
using json=nlohmann::json;
std::map<std::string,json> receipts;
bool positive(const json& value,uint64_t& result) {
    if(!value.is_number_unsigned()&&!value.is_number_integer())return false;
    if(!value.is_number_unsigned()&&value.get<int64_t>()<=0)return false;
    result=value.get<uint64_t>();return result!=0;
}
json free_receipt(const native_free::Receipt& value) {
    return {{"phase",int(value.phase)},{"allocation_generation",value.permit.allocation_generation},
        {"call",value.permit.call},{"world_generation",value.permit.world_generation},
        {"delete_attempted",value.delete_attempted},{"delete_returned",value.delete_returned},
        {"sized_attempted",value.sized_attempted},{"sized_returned",value.sized_returned},
        {"free_attempted",value.free_attempted},{"free_returned",value.free_returned},
        {"seh_code",value.seh_code},{"cause",value.cause.data()},{"cause_truncated",value.cause_truncated}};
}
}
nlohmann::json status() {
    const auto value=native_free_runtime::Runtime::instance().status();
    json result={{"phase",int(value.phase)},{"process",value.process},{"main_thread",value.main_thread},
        {"hook_mask",value.hook_mask},{"crt_confirmed",value.crt_confirmed},
        {"hooks_confirmed",value.hooks_confirmed},{"native_game_proven",value.native_game_proven},
        {"originals_published",value.originals_published},{"trampoline_route",value.trampoline_route},
        {"iat_protection_restored",value.iat_protection_restored},{"entry_inflight",value.entry_inflight},
        {"original_inflight",value.original_inflight},{"world_epoch",value.world.epoch},
        {"world_generation",value.world.generation},{"win32_error",value.win32_error},
        {"minhook_status",value.minhook_status},{"seh_code",value.seh_code},{"cause",value.cause.data()},
        {"executable_sha256",value.executable.sha256.data()},
        {"crt_sha256",value.heap.imported_crt.sha256.data()},
        {"malloc_owner_sha256",value.heap.malloc_owner.sha256.data()},
        {"free_owner_sha256",value.heap.free_owner.sha256.data()}};
    if(value.main_thread&&value.main_thread==GetCurrentThreadId())result["probe_receipts"]=receipts;
    return result;
}
bool probe(const json& args,json& out,std::string& error) {
    out=json::object();error.clear();
    if(!args.is_object()||args.size()!=4||!args.contains("token")||!args.contains("kind")||
       !args.contains("expected_world_epoch")||!args.contains("expected_world_generation")) {
        error="native holder probe requires token, kind and expected world epoch/generation";return false;
    }
    if(!args.at("token").is_string()||!args.at("kind").is_string()) {
        error="native holder token and kind require strings";return false;
    }
    const auto token=args.at("token").get<std::string>(),kind=args.at("kind").get<std::string>();
    static const std::regex pattern("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    uint64_t epoch=0,generation=0;
    if(!std::regex_match(token,pattern)||(kind!="prop"&&kind!="building")||
       !positive(args.at("expected_world_epoch"),epoch)||!positive(args.at("expected_world_generation"),generation)) {
        error="native holder probe arguments invalid";return false;
    }
    if(const auto found=receipts.find(token);found!=receipts.end()) {
        out=found->second;error="native holder token already attempted; observe retained receipt, no replay";return false;
    }
    building_mixed_identity::opened_file::CurrentWorld world;
    if(!native_open_runtime::read_current_world(world,error))return false;
    if(world.world_epoch!=epoch||world.generation!=generation) {
        error="native holder expected world differs from fresh NativeOpen witness";return false;
    }
    // Retain the attempt before hook installation or the first native allocation.
    auto [position,inserted]=receipts.emplace(token,json{{"token",token},{"kind",kind},
        {"world_epoch",epoch},{"world_generation",generation},{"attempted",true},{"confirmed",false}});
    if(!inserted){error="native holder token reservation failed";return false;}
    auto& receipt=position->second;
    auto& runtime=native_free_runtime::Runtime::instance();
    bool okay=false;
    try {
        if(runtime.status().phase==native_free_runtime::Phase::Idle&&!runtime.prepare_and_enable(error)) {
            receipt["stage"]="installation";
        } else {
            native_free_runtime::ProbeReceipt result;
            okay=runtime.probe_null_holder(kind=="prop"?native_free_runtime::HolderKind::Prop:native_free_runtime::HolderKind::Building,result,error);
            receipt["stage"]="native_holder";receipt["confirmed"]=okay&&result.confirmed;
            receipt["allocation_generation"]=result.allocation_generation;receipt["call"]=result.call;
            receipt["factory_attempted"]=result.factory_attempted;receipt["factory_returned"]=result.factory_returned;
            receipt["constructor_confirmed"]=result.constructor_confirmed;receipt["deletion"]=free_receipt(result.deletion);
        }
    } catch(const std::exception& failure) {error=failure.what();}
    catch(...) {error="native holder diagnostic nonstandard exception";}
    if(!okay&&error.empty())error="native holder diagnostic failed without cause";
    receipt["error"]=error;auto runtime_receipt=status();runtime_receipt.erase("probe_receipts");
    receipt["runtime"]=std::move(runtime_receipt);
    out=receipt;
    if(okay)LOG_INFO("[NativeFree] Probe confirmed token={} kind={} allocation={} call={}",token,kind,receipt.at("allocation_generation").dump(),receipt.at("call").dump());
    else LOG_ERROR("[NativeFree] Probe failed token={} kind={} cause={}",token,kind,error);
    drain_logs();return okay;
}
void drain_logs() {
    native_free_runtime::JournalEntry entries[64];uint64_t dropped=0;
    const auto count=native_free_runtime::Runtime::instance().journal().drain(entries,64,dropped);
    for(size_t i=0;i<count;++i) {
        const auto& value=entries[i];
        if(value.level==native_free_runtime::JournalEntry::Level::Info)LOG_INFO("[NativeFree] operation={} thread={} cause={} SEH={}",value.operation.data(),value.thread,value.original_cause.data(),value.seh_code);
        else if(value.level==native_free_runtime::JournalEntry::Level::Warn)LOG_WARN("[NativeFree] operation={} thread={} cause={} SEH={}",value.operation.data(),value.thread,value.original_cause.data(),value.seh_code);
        else LOG_ERROR("[NativeFree] operation={} thread={} cause={} SEH={}",value.operation.data(),value.thread,value.original_cause.data(),value.seh_code);
    }
    if(dropped)LOG_ERROR("[NativeFree] Journal overflow dropped={}",dropped);
}
}
