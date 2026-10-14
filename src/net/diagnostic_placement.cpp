#include "mt2mp/net/diagnostic_placement.h"
#include "mt2mp/net/sidecar_client.h"
#include "mt2mp/net/local_session.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/net/action_order.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include <chrono>
#include <map>
#include <deque>
#include <cmath>

namespace mt2mp::net {
using json=nlohmann::json;
using clock=std::chrono::steady_clock;
static std::string world,status="Kein Platzierungsnachweis angefordert",pending_id,pending_error;
static json pending_result;
static clock::time_point pending_started;
static uint64_t request_sequence=0,revision=0;
static ActionOrder<json> host_order;
static std::map<std::string,json> confirmed;
static std::map<std::string,std::string> attempted;
static std::deque<std::string> confirmation_order;
static bool valid_id(const std::string& id) { return id.size()==36&&id.find_first_not_of("0123456789abcdef-")==std::string::npos; }
static void send(const json& value,uint64_t target=0) {
    auto bytes=value.dump();
    if(!SidecarClient::instance().send(6,reinterpret_cast<const uint8_t*>(bytes.data()),uint32_t(bytes.size()),target))
        throw std::runtime_error("diagnostic action queue rejected target="+std::to_string(target));
}
static void observe_world() {
    const auto current=current_world_epoch()?current_snapshot_id():std::string{};
    if(current==world) return;
    if(!pending_id.empty()) pending_error="Snapshot wurde während der Platzierungsanfrage gewechselt";
    world=current; host_order.reset(); confirmed.clear(); attempted.clear(); confirmation_order.clear(); revision=0;
    LOG_NET_INFO("[ACTION] Diagnostic world identity changed snapshot={}",world);
}
static void validate(const json& arguments) {
    if(!arguments.is_object()||arguments.size()!=4) throw std::runtime_error("placement requires exactly type, variant, x, z");
    for(const auto* key:{"type","variant"}) {
        const auto value=arguments.at(key).get<std::string>();
        if(value.empty()||value.size()>256||value.find('\0')!=std::string::npos) throw std::runtime_error("invalid placement catalog identifier");
    }
    for(const auto* key:{"x","z"}) if(!arguments.at(key).is_number()||!std::isfinite(arguments.at(key).get<float>())) throw std::runtime_error("invalid placement coordinate");
}
static std::string key(uint64_t peer,const std::string& id) { return std::to_string(peer)+":"+id; }
static json ledger_json(const game::native::SimulationState& value) {
    return {{"day",value.clock.flat_day},{"cash",value.cash},{"record_present",value.record_present},{"record_day",value.record_day},{"record_cash",value.record_cash},{"record_income",value.record_income},{"record_expense",value.record_expense},{"line_counts",value.line_counts},{"line_income",value.line_income}};
}
static game::native::SimulationState ledger_state(const json& value) {
    game::native::SimulationState result;
    result.clock.flat_day=value.at("day"); result.cash=value.at("cash"); result.record_present=value.at("record_present");
    result.record_day=value.at("record_day"); result.record_cash=value.at("record_cash"); result.record_income=value.at("record_income"); result.record_expense=value.at("record_expense");
    if(value.at("line_counts").size()!=20||value.at("line_income").size()!=20) throw std::runtime_error("ledger requires exactly 20 categories");
    result.line_counts=value.at("line_counts").get<std::array<int32_t,20>>(); result.line_income=value.at("line_income").get<std::array<int32_t,20>>();
    return result;
}
static json host_execute(uint64_t sender,const json& request) {
    const auto id=request.at("id").get<std::string>();
    const auto seq=request.at("sequence").get<uint64_t>();
    const auto payload=request.at("arguments").dump();
    json previous; std::string claim_error;
    const auto claim=host_order.claim(sender,seq,id,payload,previous,claim_error);
    if(claim==ActionClaim::Cached) return previous;
    if(claim==ActionClaim::Rejected) throw std::runtime_error(claim_error);
    json result={{"version",1},{"session",Config::instance().session_id},{"snapshot",world},{"type","result"},{"id",id},{"requester",sender}};
    try {
        validate(request.at("arguments"));
        const auto& arguments=request.at("arguments");
        game::native::BuildingState created; std::string error;
        game::native::SimulationState before,after;
        if(!game::native::read_simulation_state(before,error)) throw std::runtime_error(error);
        if(!game::native::diagnostic_place_building(arguments.at("type"),arguments.at("variant"),arguments.at("x"),arguments.at("z"),created,error)) throw std::runtime_error(error);
        if(!game::native::read_simulation_state(after,error)) throw std::runtime_error("placement executed but ledger observation failed: "+error);
        result["ok"]=true; result["revision"]=++revision;
        result["ledger_before"]=ledger_json(before); result["ledger_after"]=ledger_json(after);
        result["building"]={{"uid",created.prop.uid},{"type",created.type},{"variant",created.variant},{"x",created.prop.x},{"y",created.prop.y},{"z",created.prop.z}};
        LOG_NET_INFO("[ACTION] HOST_EXECUTED id={} requester={} revision={} uid={}",id,sender,revision,created.prop.uid);
    } catch(const std::exception& error) { result["ok"]=false; result["cause"]=error.what(); LOG_NET_ERROR("[ACTION] Host execution rejected id={} requester={} cause={}",id,sender,error.what()); }
    host_order.confirm(sender,seq,id,payload,result);
    return result;
}
static void accept_result(const json& message) {
    const auto id=message.at("id").get<std::string>();
    if(!message.at("ok").get<bool>()) {
        const auto cause=message.at("cause").get<std::string>();
        if(id==pending_id) pending_error=cause;
        status="Platzierung abgelehnt: "+cause;
        return;
    }
    const auto next=message.at("revision").get<uint64_t>();
    const auto cache_key=key(message.at("requester").get<uint64_t>(),id);
    if(confirmed.contains(cache_key)) return;
    if(attempted.contains(cache_key)) throw std::runtime_error("replica previously attempted; fresh snapshot required: "+attempted.at(cache_key));
    if(next!=revision+1) throw std::runtime_error("diagnostic revision gap actual="+std::to_string(next)+" required="+std::to_string(revision+1));
    const auto& building=message.at("building");
    game::native::BuildingState host,local;
    host.type=building.at("type"); host.variant=building.at("variant");
    host.prop={building.at("uid"),building.at("x"),building.at("y"),building.at("z")};
    std::string error;
    const auto before=ledger_state(message.at("ledger_before"));
    const auto after=ledger_state(message.at("ledger_after"));
    if(attempted.size()>=64) throw std::runtime_error("unconfirmed replica limit reached; fresh snapshot required");
    attempted.emplace(cache_key,"native replica outcome not yet confirmed");
    if(!game::native::diagnostic_replicate_building(host,local,error)) throw std::runtime_error(error);
    attempted[cache_key]="building inserted; ledger outcome not yet confirmed";
    game::native::SimulationState actual;
    if(!game::native::diagnostic_reconcile_building_ledger(before,after,actual,error)) throw std::runtime_error("building inserted but ledger apply failed; no replay: "+error);
    revision=next;
    json mapping={{"operation_id",id},{"revision",revision},{"host_uid",host.prop.uid},{"local_uid",local.prop.uid},{"type",local.type},{"variant",local.variant},{"x",local.prop.x},{"y",local.prop.y},{"z",local.prop.z}};
    mapping["ledger"]=ledger_json(actual);
    confirmed.emplace(cache_key,mapping); confirmation_order.push_back(cache_key);
    attempted.erase(cache_key);
    if(confirmation_order.size()>64) { confirmed.erase(confirmation_order.front()); confirmation_order.pop_front(); }
    if(id==pending_id) pending_result=mapping;
    status="Host-Platzierung lokal angewendet";
    LOG_NET_INFO("[ACTION] CLIENT_APPLIED id={} revision={} host_uid={} local_uid={}",id,revision,host.prop.uid,local.prop.uid);
    send({{"version",1},{"session",Config::instance().session_id},{"snapshot",world},{"type","applied"},{"id",id},{"local_uid",local.prop.uid}},1);
}
static void receive(uint64_t sender,const uint8_t* data,uint32_t length) {
    std::string id;
    try {
        observe_world();
        if(!local_session_connected()||sender<1||sender>4||length>8192||world.empty()) throw std::runtime_error("diagnostic action requires connected snapshot session and bounded packet");
        auto message=json::parse(data,data+length); id=message.at("id");
        if(!valid_id(id)||message.at("version")!=1||message.at("session").get<uint64_t>()!=Config::instance().session_id||message.at("snapshot")!=world) throw std::runtime_error("diagnostic action identity mismatch");
        const auto type=message.at("type").get<std::string>();
        if(Config::instance().is_host) {
            if(type=="request"&&sender!=1) send(host_execute(sender,message));
            else if(type=="applied") { status="Client-Platzierung bestätigt Peer "+std::to_string(sender); LOG_NET_INFO("[ACTION] REPLICA_ACK id={} peer={} local_uid={}",id,sender,message.at("local_uid").get<uint64_t>()); }
            else throw std::runtime_error("unexpected diagnostic host message");
        } else if(sender==1&&type=="result") accept_result(message);
        else throw std::runtime_error("diagnostic result must come from host");
    } catch(const std::exception& error) {
        status="Platzierungsnachweis fehlgeschlagen: "+std::string(error.what());
        if(id==pending_id&&!pending_id.empty()) pending_error=error.what();
        LOG_NET_ERROR("[ACTION] Packet failed sender={} id={} cause={}",sender,id,error.what());
    }
}
void initialize_diagnostic_placement() { SidecarClient::instance().register_handler(6,receive); }
bool begin_diagnostic_placement(const std::string& id,const json& arguments,std::string& error) {
    try {
        observe_world(); validate(arguments);
        if(!valid_id(id)||!pending_id.empty()||world.empty()||!local_session_connected()||!game::native::in_game()) throw std::runtime_error("diagnostic placement requires idle active snapshot session");
        pending_id=id; pending_result=nullptr; pending_error.clear(); pending_started=clock::now(); status="Host-Platzierung angefordert";
        json request={{"version",1},{"session",Config::instance().session_id},{"snapshot",world},{"type","request"},{"id",id},{"sequence",++request_sequence},{"arguments",arguments}};
        if(Config::instance().is_host) {
            auto result=host_execute(1,request); send(result);
            if(result.at("ok").get<bool>()) pending_result=result;
            else pending_error=result.at("cause");
        } else send(request,1);
        return true;
    } catch(const std::exception& failure) { error=failure.what(); pending_id.clear(); LOG_NET_ERROR("[ACTION] Request failed id={} cause={}",id,error); return false; }
}
bool poll_diagnostic_placement(const std::string& id,bool& complete,json& result,std::string& error) {
    complete=false;
    if(id!=pending_id) { error="diagnostic pending request identity differs"; return false; }
    if(!pending_error.empty()) { error=pending_error; pending_id.clear(); return false; }
    if(!pending_result.is_null()) { result=pending_result; pending_id.clear(); complete=true; return true; }
    return true;
}
void tick_diagnostic_placement() {
    observe_world();
    if(!pending_id.empty()&&pending_error.empty()&&pending_result.is_null()) {
        if(!local_session_connected()) pending_error="Sitzung während der Platzierungsanfrage beendet";
        else if(clock::now()-pending_started>std::chrono::seconds(15)) pending_error="Keine bestätigte Platzierung nach 15 Sekunden";
        if(!pending_error.empty()) LOG_NET_ERROR("[ACTION] Observation stopped id={} cause={}; no replay",pending_id,pending_error);
    }
}
std::string diagnostic_placement_status() { return status; }
}
