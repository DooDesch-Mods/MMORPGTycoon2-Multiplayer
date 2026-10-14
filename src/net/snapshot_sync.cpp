#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/net/snapshot_buffer.h"
#include "mt2mp/net/software_snapshot.h"
#include "mt2mp/net/software_sync.h"
#include "mt2mp/net/sidecar_client.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/net/local_session.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/game/normal_load_identity.h"
#include "../game/network_identity.h"
#ifndef MT2MP_SNAPSHOT_TEST
#include "../game/time_integration.h"
#include "../game/time_input_diagnostics.h"
#include "../game/native_open_runtime.h"
#include "../game/hud_time_runtime.h"
#include "../game/hud_calendar_runtime.h"
#endif
#include "mt2mp/net/network_identity_metadata.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/core/hash.h"
#include "vendor/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cmath>
#include <map>

namespace mt2mp::net {
using json=nlohmann::json;
using clock=std::chrono::steady_clock;
#ifdef MT2MP_SNAPSHOT_TEST
static std::chrono::seconds test_clock_offset{0};
#endif
static clock::time_point snapshot_now() {
#ifdef MT2MP_SNAPSHOT_TEST
    return clock::now()+test_clock_offset;
#else
    return clock::now();
#endif
}
static SnapshotBuffer buffer;
static std::string incoming_id,incoming_hash,status="Kein Snapshot empfangen";
static bool receiving=false,load_pending=false;
static clock::time_point incoming_started;
static SnapshotLoadWatch incoming_load;
static clock::time_point load_observed_at;
static bool load_observation_sent=false;
static uint64_t incoming_origin_generation=0;
static std::string outbound_id;
static SnapshotFileSender outbound;
struct SnapshotPeer { uint32_t next=0; bool verified=false,loaded=false,network_verified=false; std::string network_error; json loaded_ack; SnapshotLoadWatch load; };
static std::map<uint64_t,SnapshotPeer> outbound_peers;
static size_t outbound_expected_peers=0;
static bool outbound_active=false;
static clock::time_point outbound_progress;
static std::string last_loaded;
static game::native::SoftwareCache incoming_software;
static uint64_t incoming_epoch=0;
static bool snapshot_failed=false;
static bool snapshot_world_valid=false;
static uint64_t discarded_after_failure=0;
static std::string snapshot_failure;
static uint32_t outbound_queued_chunks=0;
namespace network_meta=network_identity_metadata;
static std::optional<network_meta::Metadata> incoming_network,outbound_network;
static network_meta::Receipt outbound_network_receipt;
static std::string incoming_network_error;
static json last_loaded_ack;
static network_meta::Receipt network_receipt() {
    const auto value=game::network_identity::status_json(0);
    network_meta::Receipt receipt;
    receipt.ready=value.at("ready").get<bool>();
    const auto role=value.at("role").get<std::string>();
    receipt.role=role=="host"?network_meta::Role::Host:role=="client"?network_meta::Role::Client:network_meta::Role::Unknown;
    receipt.uuid=value.at("uuid").get<std::string>(); receipt.snapshot_sha256=value.at("snapshot_sha256").get<std::string>();
    receipt.keys_sha256=value.at("keys_sha256").get<std::string>(); receipt.node_count=value.at("staged_nodes").get<uint64_t>();
    return receipt;
}
static uint64_t software_epoch(const std::string& id) {
    std::vector<uint8_t> bytes(id.begin(),id.end());
    auto epoch=std::stoull(sha256(bytes).substr(0,16),nullptr,16);
    if(!epoch) throw std::runtime_error("snapshot software epoch must be nonzero");
    return epoch;
}
static bool valid_id(const std::string& id) { return id.size()==36&&id.find_first_not_of("0123456789abcdef-")==std::string::npos; }
static void message(const json& value,uint64_t target=0) {
    auto bytes=value.dump();
    if(!session_wire::send(4,reinterpret_cast<const uint8_t*>(bytes.data()),uint32_t(bytes.size()),target)) throw std::runtime_error("snapshot control queue rejected");
}
static std::filesystem::path write_root() {
    auto module=GetModuleHandleA("libphysfs.dll");
    auto function=module?reinterpret_cast<const char*(*)()>(GetProcAddress(module,"PHYSFS_getWriteDir")):nullptr;
    const char* path=function?function():nullptr;
    if(!path) throw std::runtime_error("PHYSFS write directory unavailable");
    auto root=std::filesystem::weakly_canonical(path);
    auto owned=std::filesystem::weakly_canonical(Config::instance().user_data_dir);
    auto relative=root.lexically_relative(owned);
    if(relative.empty()||relative.native().find(L"..")!=std::wstring::npos) throw std::runtime_error("PHYSFS directory outside owned test root");
    return root;
}
static void fail(const std::string& error) {
    if(snapshot_failed) {
        ++discarded_after_failure;
        if(discarded_after_failure==1) LOG_NET_WARN("[SNAPSHOT] Late transfer event discarded cause={}; first failure retained",error);
        return;
    }
    snapshot_failed=true; discarded_after_failure=0; snapshot_failure=error;
    snapshot_world_valid=false;
    game::network_identity::invalidate_world("snapshot transfer failed: "+error);
    receiving=false; load_pending=false; outbound_active=false; outbound.stop(); buffer.stop(); status="Snapshot fehlgeschlagen: "+error;
    LOG_NET_ERROR("[SNAPSHOT] {}",error);
    if(!Config::instance().is_host) {
        try { message({{"type","snapshot_error"},{"id",incoming_id},{"cause",error}},1); }
        catch(const std::exception& send_error) { LOG_NET_ERROR("[SNAPSHOT] Failure acknowledgement rejected: {}",send_error.what()); }
    }
}
static void receive_control(uint64_t sender,const uint8_t* data,uint32_t length) {
    try {
        if(length>8192) throw std::runtime_error("control exceeds 8192 bytes");
        auto value=json::parse(data,data+length); auto type=value.at("type").get<std::string>();
        if(Config::instance().is_host) {
            if(type=="snapshot_ready"||type=="snapshot_chunk_ack"||type=="snapshot_verified"||type=="snapshot_load_progress"||type=="snapshot_loaded"||type=="snapshot_error") {
                auto id=value.at("id").get<std::string>();
                if(!outbound_active&&id==outbound_id&&(type=="snapshot_verified"||type=="snapshot_loaded")) {
                    const auto completed=outbound_peers.find(sender);
                    if(completed!=outbound_peers.end()&&completed->second.loaded) {
                        if(type=="snapshot_loaded"&&value!=completed->second.loaded_ack) throw std::runtime_error("completed native load acknowledgement differs from original");
                        if(type=="snapshot_verified"&&value.at("sha256").get<std::string>()!=incoming_hash) throw std::runtime_error("completed file verification SHA differs");
                        LOG_NET_INFO("[SNAPSHOT] Completed snapshot acknowledgement skipped peer={} type={} id={}",sender,type,id); return;
                    }
                }
                if(sender<2||sender>4||id!=outbound_id||!outbound_active) throw std::runtime_error("acknowledgement snapshot identity or active peer differs");
                if(type=="snapshot_error") throw std::runtime_error("peer="+std::to_string(sender)+" snapshot rejected: "+value.at("cause").get<std::string>());
                if(type=="snapshot_ready") {
                    if(outbound_peers.contains(sender)||outbound_peers.size()>=outbound_expected_peers) throw std::runtime_error("duplicate or unexpected snapshot-ready peer");
                    outbound_peers.emplace(sender,SnapshotPeer{}); outbound_progress=snapshot_now();
                    LOG_NET_INFO("[SNAPSHOT] Ready peer={} id={} ready={} required={}",sender,id,outbound_peers.size(),outbound_expected_peers); return;
                }
                auto found=outbound_peers.find(sender);
                if(found==outbound_peers.end()) throw std::runtime_error("snapshot acknowledgement before peer readiness");
                auto& peer=found->second;
                if(type=="snapshot_loaded"&&peer.loaded) {
                    if(value!=peer.loaded_ack) throw std::runtime_error("native load acknowledgement replay conflicts with original");
                    LOG_NET_INFO("[SNAPSHOT] Matching loaded acknowledgement skipped peer={} id={}",sender,id); return;
                }
                if(type=="snapshot_chunk_ack") {
                    const auto next=value.at("next").get<uint32_t>();
                    if(next<=peer.next||next>outbound.next_index()) throw std::runtime_error("snapshot acknowledgement offset actual="+std::to_string(next)+" required>"+std::to_string(peer.next)+" and<="+std::to_string(outbound.next_index()));
                    uint32_t before=outbound.next_index(); for(const auto& [id,state]:outbound_peers) before=std::min(before,state.next);
                    peer.next=next;
                    uint32_t after=outbound.next_index(); for(const auto& [id,state]:outbound_peers) after=std::min(after,state.next);
                    if(after>before) outbound_progress=snapshot_now();
                    return;
                }
                if(peer.next!=outbound.chunks()) throw std::runtime_error("snapshot completion acknowledgement before all chunks");
                if(type=="snapshot_load_progress") {
                    if(!peer.verified||peer.loaded) throw std::runtime_error("snapshot native load progress outside verified pending load");
                    if(!value.at("active_mode").is_number_integer()||!value.at("requested_mode").is_number_integer()||value.at("active_mode")<0||value.at("active_mode")>=30||value.at("requested_mode")<0||value.at("requested_mode")>=30||!value.at("registrations").is_number_unsigned()) throw std::runtime_error("snapshot native load progress fields invalid");
                    const auto mode=value.at("active_mode").get<int32_t>(),requested=value.at("requested_mode").get<int32_t>();
                    if(!peer.load.observe(mode,requested,value.at("registrations").get<uint64_t>(),snapshot_now())) throw std::runtime_error("snapshot native load progress repeated unchanged observation");
                    LOG_NET_INFO("[SNAPSHOT] Native load progress peer={} id={} mode={}/{} registrations={} elapsed_ms={} deadline_s={}",sender,id,mode,requested,peer.load.registrations,peer.load.elapsed_ms(snapshot_now()),snapshot_native_load_timeout_seconds);
                    return;
                }
                if(type=="snapshot_verified") {
                    if(peer.verified||value.at("sha256").get<std::string>()!=incoming_hash) throw std::runtime_error("duplicate verification or acknowledged SHA256 differs");
                    peer.verified=true; peer.load.begin(snapshot_now());
                    LOG_NET_INFO("[SNAPSHOT] Native load wait peer={} id={} deadline_s={} data_idle_s={}",sender,id,snapshot_native_load_timeout_seconds,snapshot_data_idle_timeout_seconds);
                } else {
                    if(!peer.verified||peer.loaded) throw std::runtime_error("snapshot-loaded acknowledgement before verification or duplicate");
                    if(value.contains("network_identity")&&value.contains("network_identity_error")) throw std::runtime_error("native network acknowledgement contains conflicting success and failure");
                    std::string network_error;
                    if(outbound_network&&value.contains("network_identity_error")&&!value.contains("network_identity")) {
                        if(value.at("sha256")!=incoming_hash||!value.at("network_identity_error").is_string()) throw std::runtime_error("degraded native network acknowledgement shape/SHA invalid");
                        network_error=value.at("network_identity_error").get<std::string>();
                        if(network_error.empty()||network_error.size()>1024) throw std::runtime_error("degraded native network acknowledgement cause length invalid");
                        peer.network_error=network_error;
                        LOG_NET_ERROR("[SNAPSHOT] File loaded peer={} id={} but native network map failed cause={}",sender,id,network_error);
                    } else {
                        if(outbound_network&&!snapshot_world_valid) throw std::runtime_error("native network acknowledgement belongs to retired host world");
                        if(!network_meta::verify_loaded_ack(value,sender,network_receipt(),outbound_network,peer.network_verified,network_error)) throw std::runtime_error(network_error);
                    }
                    peer.loaded_ack=value;
                    peer.loaded=true; peer.load.finish(snapshot_now());
                }
                if(type=="snapshot_verified") outbound_progress=snapshot_now();
                LOG_NET_INFO("[SNAPSHOT] Ack peer={} type={} id={}",sender,type,id);
                status=type+" Peer "+std::to_string(sender);
            }
            return;
        }
        if(sender!=1||type!="snapshot_begin") throw std::runtime_error("snapshot manifest must come from host");
        auto id=value.at("id").get<std::string>(); auto hash=value.at("sha256").get<std::string>();
        if(!valid_id(id)||hash.size()!=64||hash.find_first_not_of("0123456789abcdef")!=std::string::npos) throw std::runtime_error("invalid snapshot id or SHA256");
        std::optional<network_meta::Metadata> metadata; std::string metadata_error;
        if(!network_meta::parse_begin(value,sender,metadata,metadata_error)) throw std::runtime_error(metadata_error);
        if(id==last_loaded) {
            if(!snapshot_world_valid||snapshot_failed) throw std::runtime_error("completed snapshot belongs to retired world; require new snapshot UUID");
            if(hash!=incoming_hash||metadata.has_value()!=incoming_network.has_value()||(metadata&&(metadata->expected_nodes!=incoming_network->expected_nodes||metadata->keys_sha256!=incoming_network->keys_sha256))||last_loaded_ack.is_null()) throw std::runtime_error("completed snapshot manifest differs from original");
            if(last_loaded_ack.contains("network_identity")) {
                json current_ack; network_meta::Disposition disposition;
                if(!network_meta::make_loaded_ack(id,hash,incoming_network,network_receipt(),true,current_ack,disposition,metadata_error)||current_ack!=last_loaded_ack) throw std::runtime_error("completed native network map no longer current: "+metadata_error);
            }
            message(last_loaded_ack,1); return;
        }
        if(receiving||load_pending) throw std::runtime_error("snapshot transfer already active");
        if(value.at("transport_revision")!=2) throw std::runtime_error("snapshot stream requires transport revision 2");
        if(!value.at("bytes").is_number_integer()||!value.at("chunks").is_number_integer()) throw std::runtime_error("snapshot size and chunks require integer fields");
        const auto chunks=value.at("chunks").get<uint64_t>();
        if(chunks>std::numeric_limits<uint32_t>::max()) throw std::runtime_error("snapshot chunk count exceeds uint32");
        auto software=parse_initial_software_cache(value.at("software"));
        std::string error;
        auto path=write_root()/"save"/"MPTestSession"/("snapshot-"+id+".vrz"); std::filesystem::create_directories(path.parent_path());
        auto staging=path; staging+=".incoming";
        if(!buffer.begin(value.at("bytes").get<uint64_t>(),uint32_t(chunks),staging,path,hash,error)) throw std::runtime_error(error);
        if(snapshot_failed) LOG_NET_INFO("[SNAPSHOT] New transfer accepted after failure id={} discarded_events={}",id,discarded_after_failure);
        snapshot_failed=false; discarded_after_failure=0; snapshot_failure.clear();
        incoming_id=id; incoming_hash=hash; incoming_software=software; incoming_epoch=software_epoch(id); incoming_started=snapshot_now(); incoming_load={}; receiving=true; status="Snapshot wird empfangen";
        incoming_network=std::move(metadata); incoming_network_error.clear(); last_loaded_ack=nullptr;
        snapshot_world_valid=false;
        LOG_NET_INFO("[SNAPSHOT] Manifest accepted id={} bytes={}",id,buffer.size());
        message({{"type","snapshot_ready"},{"id",id}},1);
    } catch(const std::exception& error) { fail(error.what()); }
}
static void receive_chunk(uint64_t sender,const uint8_t* data,uint32_t length) {
    if(Config::instance().is_host) return;
    try {
        if(sender!=1||length<40||!receiving) throw std::runtime_error("unexpected snapshot chunk");
        std::string id(reinterpret_cast<const char*>(data),36); uint32_t index; memcpy(&index,data+36,4);
        if(id!=incoming_id) throw std::runtime_error("snapshot chunk identity differs");
        std::string error;
        if(!buffer.add(index,data+40,length-40,error)) throw std::runtime_error(error);
        incoming_started=snapshot_now();
        message({{"type","snapshot_chunk_ack"},{"id",id},{"next",buffer.received_count()}},1);
        if(!buffer.complete()) return;
        if(!buffer.commit(error)) throw std::runtime_error(error);
        auto path=buffer.target();
        auto runtime=std::filesystem::path(Config::instance().runtime_root);
        { std::ofstream stream(runtime/"diagnostics"/("snapshot-"+id+".json")); stream<<json{{"snapshot_id",id},{"bytes",buffer.size()},{"sha256",incoming_hash},{"path",path.string()}}.dump(2); stream.close(); if(!stream) throw std::runtime_error("snapshot manifest write failed"); }
        receiving=false; status="Snapshot vollständig geprüft";
        LOG_NET_INFO("[SNAPSHOT] VERIFIED id={} bytes={} sha256={}",id,buffer.size(),incoming_hash);
        message({{"type","snapshot_verified"},{"id",id},{"sha256",incoming_hash}},1);
        incoming_load.begin(snapshot_now()); load_observation_sent=false;
#ifndef MT2MP_SNAPSHOT_TEST
        incoming_origin_generation=0;
        if(!game::native_open_runtime::prepare(runtime,id,incoming_hash,buffer.size(),incoming_origin_generation,error))
            throw std::runtime_error("native client snapshot origin preparation failed: "+error);
        try {
        game::time_input_diagnostics::suspend("verified snapshot replaces client world");
        if(!game::hud_time_runtime::suspend("verified snapshot replaces client world",error))
            throw std::runtime_error("HUD authority retire before client load failed: "+error);
        if(!game::hud_calendar_runtime::suspend("verified snapshot replaces client world",error))
            throw std::runtime_error("Calendar authority retire before client load failed: "+error);
        if(!game::time_integration::instance().suspend("verified snapshot replaces client world",error))
            throw std::runtime_error("time authority retire before client load failed: "+error);
#endif
        if(!game::native::arm_software_replica(incoming_epoch,error)) throw std::runtime_error("software replica arm before load failed: "+error);
        if(!game::normal_load_identity::begin(incoming_hash,id,error)) {
            auto identity_error=error;
            if(!game::native::disarm_software_replica(incoming_epoch,error)) LOG_NET_ERROR("[SNAPSHOT] Replica disarm after identity arm rejection failed: {}",error);
            throw std::runtime_error("normal-load identity arm failed: "+identity_error);
        }
        game::network_identity::invalidate_world("verified snapshot replaces client world");
        if(incoming_network&&!game::network_identity::begin_client_load(id,incoming_hash,incoming_network->expected_nodes,error)) {
            incoming_network_error="native network identity arm failed: "+error;
            LOG_NET_ERROR("[SNAPSHOT] {} id={}",incoming_network_error,id);
        } else if(!incoming_network) LOG_NET_INFO("[SNAPSHOT] Native network identity skipped: host offered file-only snapshot id={}",id);
#ifndef MT2MP_SNAPSHOT_TEST
        } catch(...) {
            std::string cleanup_error;
            if(!game::native_open_runtime::retire_unstarted(incoming_origin_generation,cleanup_error))
                LOG_NET_ERROR("[NativeOpen] Unstarted client load retirement failed generation={} cause={}",incoming_origin_generation,cleanup_error);
            throw;
        }
        if(!game::native_open_runtime::before_request(incoming_origin_generation,error))
            throw std::runtime_error("native client origin request binding failed: "+error);
#endif
        if(!game::native::request_load("save/MPTestSession/snapshot-"+id+".vrz",error)) {
            auto load_error=error;
            game::normal_load_identity::cancel(load_error);
            if(!game::native::disarm_software_replica(incoming_epoch,error)) LOG_NET_ERROR("[SNAPSHOT] Replica disarm after rejected load failed: {}",error);
            throw std::runtime_error(load_error);
        }
        load_pending=true; status="Snapshot-Laden angefordert";
        LOG_NET_INFO("[SNAPSHOT] Native load requested id={} deadline_s={} data_idle_s={}",id,snapshot_native_load_timeout_seconds,snapshot_data_idle_timeout_seconds);
    } catch(const std::exception& error) { fail(error.what()); }
}
void initialize_snapshot_sync() { session_wire::register_handler(4,receive_control); session_wire::register_handler(5,receive_chunk); }
bool distribute_snapshot(const std::string& id,const std::string& digest,const std::filesystem::path& path,std::string& error) {
    if(!Config::instance().is_host||!valid_id(id)||outbound_active) {
        error="snapshot distribution requires idle host"; LOG_NET_ERROR("[SNAPSHOT] Distribution rejected: {}",error); return false;
    }
    try {
        outbound_id=id; incoming_hash=digest; outbound_peers.clear(); outbound_expected_peers=0; outbound_queued_chunks=0;
        snapshot_failed=false; discarded_after_failure=0; snapshot_failure.clear();
        const auto peers=session_wire::peers();
        if(!peers||peers>3) throw std::runtime_error("snapshot distribution requires 1..3 connected peers");
        if(!outbound.begin(path,digest,error)) throw std::runtime_error(error);
        game::native::SoftwareState software;
        if(!game::native::read_software_state(software,error)) throw std::runtime_error("host software state unavailable: "+error);
        begin_software_sync(software_epoch(id),{software.rating,software.fun});
        snapshot_failed=false; discarded_after_failure=0; snapshot_failure.clear();
        outbound_id=id; incoming_hash=digest; outbound_expected_peers=peers; outbound_peers.clear(); outbound_active=true; outbound_progress=snapshot_now();
        json begin={{"type","snapshot_begin"},{"transport_revision",2},{"id",id},{"sha256",digest},{"bytes",outbound.size()},{"chunks",outbound.chunks()},
            {"software",{{"revision",1},{"rating",software.rating},{"fun",software.fun}}}};
        outbound_network_receipt=network_receipt(); network_meta::Disposition disposition;
        if(!network_meta::attach_begin(begin,outbound_network_receipt,disposition,error)||!network_meta::parse_begin(begin,1,outbound_network,error)) throw std::runtime_error(error);
        if(disposition==network_meta::Disposition::LegacyFileOnly) LOG_NET_WARN("[SNAPSHOT] Native network writes disabled: host map unavailable id={}",id);
        message(begin);
        snapshot_world_valid=true;
        status="Host-Snapshot wird übertragen"; return true;
    } catch(const std::exception& failure) { error=failure.what(); fail(error); LOG_NET_ERROR("[SNAPSHOT] Distribution rejected: {}",error); return false; }
}
static void observe_native_load(clock::time_point now) {
    std::string error; game::native::WorldStatus world;
    if(!game::native::read_world_status(world,error)) throw std::runtime_error("snapshot native load mode observation failed: "+error);
    if(world.crash_report_active) throw std::runtime_error("snapshot native load entered crash report mode; no dismiss requested");
    const auto identity=game::normal_load_identity::status();
    if(identity.snapshot_uuid!=incoming_id) throw std::runtime_error("snapshot native load registration observation identity differs");
    const bool changed=incoming_load.observe(world.active_mode_index,world.requested_mode_index,identity.registrations,now);
    load_observed_at=now; load_observation_sent=true;
    if(!changed) return;
    message({{"type","snapshot_load_progress"},{"id",incoming_id},{"active_mode",incoming_load.active_mode},
        {"requested_mode",incoming_load.requested_mode},{"registrations",incoming_load.registrations}},1);
    LOG_NET_INFO("[SNAPSHOT] Native load progress id={} mode={}/{} registrations={} elapsed_ms={} deadline_s={}",incoming_id,incoming_load.active_mode,incoming_load.requested_mode,incoming_load.registrations,incoming_load.elapsed_ms(now),snapshot_native_load_timeout_seconds);
}
void tick_snapshot_sync() {
    try {
        if((receiving||load_pending||outbound_active)&&!session_wire::connected()) throw std::runtime_error("snapshot transport ended: "+session_wire::failure_reason());
        uint32_t acknowledged=outbound.next_index();
        for(const auto& [peer,state]:outbound_peers) acknowledged=std::min(acknowledged,state.next);
        for(int sent=0;sent<4&&outbound_active&&outbound.active()&&outbound_peers.size()==outbound_expected_peers&&outbound.next_index()<outbound.chunks()&&outbound.next_index()-acknowledged<snapshot_window_chunks;++sent) {
            const auto index=outbound.next_index(); std::vector<uint8_t> chunk; std::string error;
            if(!outbound.next(chunk,error)) throw std::runtime_error(error);
            std::vector<uint8_t> packet(40+chunk.size()); memcpy(packet.data(),outbound_id.data(),36); memcpy(packet.data()+36,&index,4); memcpy(packet.data()+40,chunk.data(),chunk.size());
            if(!session_wire::send(5,packet.data(),uint32_t(packet.size()))) throw std::runtime_error("snapshot chunk queue rejected");
            ++outbound_queued_chunks;
        }
        if(outbound_active&&outbound.active()&&acknowledged==outbound.chunks()) {
            std::string error; if(!outbound.finish(error)) throw std::runtime_error(error);
            LOG_NET_INFO("[SNAPSHOT] Transfer byte-acknowledged id={} bytes={} chunks={}",outbound_id,outbound.size(),outbound.chunks());
        }
        if(outbound_active&&outbound_peers.size()==outbound_expected_peers) {
            bool loaded=true; for(const auto& [peer,state]:outbound_peers) loaded=loaded&&state.loaded;
            if(loaded) { outbound_active=false; LOG_NET_INFO("[SNAPSHOT] All peers loaded id={} peers={}",outbound_id,outbound_peers.size()); }
        }
        const auto now=snapshot_now();
        bool awaiting_data=outbound_peers.size()!=outbound_expected_peers;
        for(const auto& [peer,state]:outbound_peers) {
            awaiting_data=awaiting_data||!state.verified;
            if(outbound_active&&state.verified&&!state.loaded&&state.load.expired(now)) throw std::runtime_error("peer="+std::to_string(peer)+" "+state.load.timeout_cause(now));
        }
        if(outbound_active&&awaiting_data&&now-outbound_progress>std::chrono::seconds(snapshot_data_idle_timeout_seconds)) throw std::runtime_error("snapshot acknowledgement progress timeout acknowledged="+std::to_string(acknowledged)+" sent="+std::to_string(outbound.next_index())+" required idle<="+std::to_string(snapshot_data_idle_timeout_seconds)+"s");
        if(receiving&&now-incoming_started>std::chrono::seconds(snapshot_data_idle_timeout_seconds)) throw std::runtime_error("snapshot receive progress timeout bytes="+std::to_string(buffer.written_bytes())+" required idle<="+std::to_string(snapshot_data_idle_timeout_seconds)+"s");
        if(load_pending&&incoming_load.expired(now)) throw std::runtime_error(incoming_load.timeout_cause(now));
        if(load_pending&&(!load_observation_sent||now-load_observed_at>=std::chrono::seconds(1))) observe_native_load(now);
        if(load_pending&&game::native::in_game()) {
            observe_native_load(now);
            std::string error;
#ifndef MT2MP_SNAPSHOT_TEST
            bool origin_pending=false;
            if(!game::native_open_runtime::finish(incoming_origin_generation,origin_pending,error))
                throw std::runtime_error("native client snapshot origin completion failed: "+error);
            if(origin_pending) return;
#endif
            game::normal_load_identity::Status identity; std::string identity_error;
            if(!game::normal_load_identity::finalize(identity,identity_error)) LOG_NET_WARN("[SNAPSHOT] Loaded world identity observation incomplete id={}: {}",incoming_id,identity_error);
            if(incoming_network&&incoming_network_error.empty()&&!game::network_identity::finalize_client_load(error)) incoming_network_error="native network map finalization failed: "+error;
            if(!game::native::set_clock(true,0,error)) throw std::runtime_error("client calendar pause after load failed: "+error);
            game::native::ClockState actual;
            if(!game::native::read_clock(actual,error)||actual.time_scale!=0) throw std::runtime_error("client calendar pause readback failed: "+error);
            game::native::SoftwareState software;
            if(!game::native::read_software_state(software,error)) throw std::runtime_error("client software state unavailable after load: "+error);
            game::native::SoftwareCacheReceipt applied;
            game::native::SoftwareCache expected{software.initial_rating,software.initial_fun};
            if(!game::native::apply_software_cache(incoming_epoch,1,expected,incoming_software,applied,error)) throw std::runtime_error("host software cache application failed: "+error);
            begin_software_sync(incoming_epoch,incoming_software);
            LOG_NET_INFO("[SNAPSHOT] SOFTWARE_CACHE_APPLIED id={} epoch={} revision={} written={} before={}/{} after={}/{}",incoming_id,incoming_epoch,applied.revision,applied.written,applied.before.rating,applied.before.fun,applied.after.rating,applied.after.fun);
            load_pending=false; last_loaded=incoming_id; status="Snapshot-Welt aktiv";
            network_meta::Disposition disposition;
            if(incoming_network_error.empty()&&!network_meta::make_loaded_ack(incoming_id,incoming_hash,incoming_network,network_receipt(),true,last_loaded_ack,disposition,error)) incoming_network_error=error;
            if(!incoming_network_error.empty()) {
                LOG_NET_ERROR("[SNAPSHOT] File world active but native network identity failed id={} cause={}",incoming_id,incoming_network_error);
                last_loaded_ack={{"type","snapshot_loaded"},{"id",incoming_id},{"sha256",incoming_hash},{"network_identity_error",incoming_network_error.substr(0,1024)}};
            }
            message(last_loaded_ack,1);
            snapshot_world_valid=true;
            incoming_load.finish(snapshot_now());
            LOG_NET_INFO("[SNAPSHOT] WORLD_ACTIVE after native snapshot load id={}",incoming_id);
            LOG_NET_INFO("[SNAPSHOT] Client calendar paused day={} seconds={}; simulation callbacks remain unproven",actual.flat_day,actual.seconds);
        }
    } catch(const std::exception& error) { fail(error.what()); }
}
std::string snapshot_sync_status() { return status; }
std::string current_snapshot_id() { return Config::instance().is_host?outbound_id:last_loaded; }
uint64_t current_world_epoch() {
    if(snapshot_failed||!snapshot_world_valid) return 0;
    if(Config::instance().is_host) return valid_id(outbound_id)?software_epoch(outbound_id):0;
    return !receiving&&!load_pending&&!last_loaded.empty()?incoming_epoch:0;
}
void invalidate_snapshot_world(const std::string& reason) {
    snapshot_world_valid=false;
    last_loaded_ack=nullptr;
    for(auto& [peer,state]:outbound_peers) {
        state.network_verified=false; state.network_error="world authority invalidated: "+reason;
    }
    if(outbound_active||receiving||load_pending) fail("active snapshot world invalidated: "+reason);
    LOG_NET_INFO("[SNAPSHOT] World authority invalidated cause={}",reason);
}
std::string pending_snapshot_load_id() { return load_pending&&!snapshot_failed?incoming_id:std::string{}; }
static json load_watch_json(const SnapshotLoadWatch& watch,clock::time_point now) {
    return {{"started",watch.active},{"observed",watch.observed},{"completed",watch.completed},{"deadline_seconds",snapshot_native_load_timeout_seconds},
        {"elapsed_ms",watch.elapsed_ms(now)},{"progress_idle_ms",watch.progress_idle_ms(now)},{"active_mode",watch.active_mode},
        {"requested_mode",watch.requested_mode},{"registrations",watch.registrations},{"progress_events",watch.progress_events}};
}
nlohmann::json snapshot_progress() {
    const bool host=Config::instance().is_host;
    const auto total=host?outbound.size():buffer.size();
    const auto chunks=uint32_t((total+snapshot_chunk_bytes-1)/snapshot_chunk_bytes);
    uint32_t acknowledged=outbound_queued_chunks;
    size_t verified=0,loaded=0; json peer_loads=json::array(); const auto now=snapshot_now();
    for(const auto& [peer,state]:outbound_peers) {
        acknowledged=std::min(acknowledged,state.next); verified+=state.verified; loaded+=state.loaded;
        auto observation=load_watch_json(state.load,now); observation["peer"]=peer; observation["verified"]=state.verified; observation["loaded"]=state.loaded;
        observation["network_verified"]=state.network_verified; observation["network_error"]=state.network_error;
        peer_loads.push_back(std::move(observation));
    }
    if(outbound_peers.size()!=outbound_expected_peers||!outbound_expected_peers) acknowledged=0;
    std::string phase="idle";
    if(snapshot_failed) phase="failed";
    else if(host&&!outbound_id.empty()) {
        if(outbound_active&&outbound_peers.size()!=outbound_expected_peers) phase="awaiting_receivers";
        else if(acknowledged<chunks) phase="transferring";
        else if(verified<outbound_expected_peers) phase="awaiting_verification";
        else if(loaded<outbound_expected_peers) phase="awaiting_load";
        else phase="active";
    } else if(!host) {
        if(receiving) phase="transferring";
        else if(load_pending) phase="awaiting_load";
        else if(!last_loaded.empty()&&last_loaded==incoming_id) phase="active";
    }
    return {{"direction",host?"send":"receive"},{"id",host?outbound_id:incoming_id},{"phase",phase},{"state",status},
        {"bytes_total",total},{"chunks_total",chunks},{"bytes_queued",host?std::min<uint64_t>(total,uint64_t(outbound_queued_chunks)*snapshot_chunk_bytes):0},
        {"chunks_queued",host?outbound_queued_chunks:0},{"bytes_written",host?0:buffer.written_bytes()},{"chunks_received",host?0:buffer.received_count()},
        {"acknowledged_min",host?acknowledged:0},{"bytes_acknowledged_min",host?std::min<uint64_t>(total,uint64_t(acknowledged)*snapshot_chunk_bytes):0},
        {"peers_expected",host?outbound_expected_peers:0},{"peers_ready",host?outbound_peers.size():0},{"peers_verified",host?verified:0},{"peers_loaded",host?loaded:0},
        {"data_idle_timeout_seconds",snapshot_data_idle_timeout_seconds},{"native_load_timeout_seconds",snapshot_native_load_timeout_seconds},
        {"native_load",host?json(nullptr):load_watch_json(incoming_load,now)},{"peer_loads",peer_loads},
        {"world_authority_valid",snapshot_world_valid&&!snapshot_failed},
        {"network_manifest_offered",host?outbound_network.has_value():incoming_network.has_value()},{"network_identity_error",host?std::string{}:incoming_network_error},
        {"pending_load",load_pending},{"transfer_active",host?outbound_active:receiving},{"first_failure",snapshot_failure},{"discarded_after_failure",discarded_after_failure}};
}
#ifdef MT2MP_SNAPSHOT_TEST
namespace testing { void advance_snapshot_clock(uint32_t seconds) { test_clock_offset+=std::chrono::seconds(seconds); } }
#endif
}
