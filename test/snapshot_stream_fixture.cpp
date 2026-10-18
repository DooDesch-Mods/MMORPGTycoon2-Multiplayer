#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/net/snapshot_buffer.h"
#include "mt2mp/net/local_session.h"
#include "mt2mp/net/sidecar_client.h"
#include "mt2mp/net/software_sync.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/overlay/lobby_overlay.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/game/normal_load_identity.h"
#include "../src/game/network_identity.h"
#include "vendor/nlohmann/json.hpp"
#include <windows.h>
#include <psapi.h>
#include <chrono>
#include <atomic>
#include <iostream>
#include <thread>
using json=nlohmann::json;
using namespace std::chrono_literals;
static DWORD main_thread;
static bool loaded=false,identity_armed=false,cache_applied=false;
static uint64_t armed_epoch=0;
static int load_calls=0,software_sync_calls=0;
static std::string fixture_mode;
static std::string network_variant;
static std::chrono::steady_clock::time_point load_requested_at;
static std::string expected_digest;
static std::atomic<bool> all_peers_loaded{false};
static std::filesystem::path runtime;
static const std::string snapshot_id="50000000-0000-4000-8000-000000000001";
static void require(bool value,const std::string& reason) { if(!value) throw std::runtime_error(reason); }
static void engine_thread() { require(GetCurrentThreadId()==main_thread,"native fixture called outside mainthread"); }
namespace mt2mp {
Config& Config::instance() { static Config value; return value; }
Logger& Logger::instance() { static Logger value; return value; }
void Logger::log(LogCategory,LogLevel,const char*,int,const std::string& text) {
    if(text.starts_with("[SNAPSHOT] All peers loaded")) all_peers_loaded=true;
    std::lock_guard lock(main_mutex_); std::cout<<"log: "<<text<<std::endl;
}
}
namespace mt2mp::overlay {
LobbyOverlay& LobbyOverlay::instance() { static LobbyOverlay value; return value; }
void LobbyOverlay::set_visible(bool value) { engine_thread(); visible_=value; }
}
namespace mt2mp::net {
void begin_software_sync(uint64_t epoch,const game::native::SoftwareCache& initial) {
    engine_thread(); require(epoch!=0&&initial.rating==700&&initial.fun==42,"software-sync epoch or initial metadata changed");
    if(!Config::instance().is_host) require(cache_applied,"software sync began before initial revision1 application");
    ++software_sync_calls;
}
}
namespace mt2mp::game::native {
bool in_game() {
    engine_thread();
    if(fixture_mode=="load_slow"&&load_calls&&std::chrono::steady_clock::now()-load_requested_at>=2200ms) loaded=true;
    return mt2mp::Config::instance().is_host||loaded;
}
bool read_world_status(WorldStatus& state,std::string&) {
    engine_thread(); state.active_mode_index=loaded?3:4; state.requested_mode_index=3; state.world_present=loaded; return true;
}
bool read_software_state(SoftwareState& state,std::string&) {
    engine_thread(); state.rating=700; state.fun=42; state.initial_rating=600; state.initial_fun=40; return true;
}
bool arm_software_replica(uint64_t epoch,std::string&) { engine_thread(); require(!armed_epoch&&epoch,"replica already armed"); armed_epoch=epoch; return true; }
bool disarm_software_replica(uint64_t epoch,std::string&) { engine_thread(); require(epoch==armed_epoch,"replica disarm epoch changed"); armed_epoch=0; return true; }
bool apply_software_cache(uint64_t epoch,uint64_t revision,const SoftwareCache& expected,const SoftwareCache& host,SoftwareCacheReceipt& receipt,std::string&) {
    engine_thread(); require(loaded&&identity_armed&&armed_epoch==epoch&&revision==1,"initial cache identity/epoch/revision order changed");
    require(expected.rating==600&&expected.fun==40&&host.rating==700&&host.fun==42,"initial software cache metadata changed");
    receipt.written=true; receipt.epoch=epoch; receipt.revision=revision; receipt.before=expected; receipt.after=host; cache_applied=true; return true;
}
bool request_load(const std::string& filename,std::string&) {
    engine_thread(); require(identity_armed&&armed_epoch&&filename=="save/MPTestSession/snapshot-"+snapshot_id+".vrz","normal load guard order changed");
    auto path=runtime/"data"/filename; std::string actual,error; uint64_t size=0;
    require(net::hash_snapshot_file(path,actual,size,error)&&actual==expected_digest,"native fixture load received unverified data: "+error);
    require(!std::filesystem::exists(path.string()+".incoming"),"native fixture load before atomic commit");
    ++load_calls; load_requested_at=std::chrono::steady_clock::now();
    loaded=fixture_mode!="load_slow"&&fixture_mode!="load_timeout"&&fixture_mode!="host_load_timeout"; return true;
}
bool set_clock(bool paused,float speed,std::string&) { engine_thread(); require(paused&&speed==0,"client pause changed"); return true; }
bool read_clock(ClockState& state,std::string&) { engine_thread(); state.time_scale=0; state.paused=true; return true; }
}
namespace mt2mp::game::normal_load_identity {
bool begin(const std::string& digest,const std::string& id,std::string&) {
    engine_thread(); require(digest==expected_digest&&id==snapshot_id&&armed_epoch,"normal-load identity guards changed"); identity_armed=true; return true;
}
bool finalize(Status&,std::string&) { engine_thread(); require(loaded&&identity_armed,"normal-load finalize order changed"); return true; }
void cancel(const std::string& reason) { engine_thread(); identity_armed=false; std::cout<<"warn: fixture identity cancel cause="<<reason<<std::endl; }
Status status() {
    engine_thread(); Status result; result.snapshot_uuid=snapshot_id; result.load_pending=load_calls!=0;
    result.registrations=load_calls?uint64_t(100+std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-load_requested_at).count()/500):0;
    return result;
}
}
namespace mt2mp::game::network_identity {
static bool map_armed=false,map_ready=false;
void cancel_pending(const std::string& reason) { engine_thread(); map_armed=false; std::cout<<"warn: fixture network cancel cause="<<reason<<std::endl; }
void invalidate_world(const std::string& reason) { engine_thread(); map_armed=false; map_ready=false; std::cout<<"info: fixture network invalidated cause="<<reason<<std::endl; }
bool begin_client_load(const std::string& id,const std::string& hash,uint32_t count,std::string& error) {
    engine_thread(); require(identity_armed&&!load_calls&&id==snapshot_id&&hash==expected_digest&&count==240,"network arm before native load or identity differs");
    if(network_variant=="armfail") { error="deliberate fixture network arm failure"; return false; }
    map_armed=true; return true;
}
bool finalize_client_load(std::string&) { engine_thread(); require(loaded&&map_armed&&load_calls==1,"network finalize native load order differs"); map_ready=true; map_armed=false; return true; }
json status_json(uint32_t) {
    engine_thread(); const bool host=mt2mp::Config::instance().is_host;
    return {{"ready",network_variant!="fileonly"&&(host||map_ready)},{"role",host?"host":"client"},{"uuid",snapshot_id},{"snapshot_sha256",expected_digest},{"staged_nodes",240},{"keys_sha256",std::string(64,!host&&network_variant=="digestwrong"?'d':'c')}};
}
}
static json without_observation_clocks(json value) {
    auto strip=[](json& observation) { if(observation.is_object()) { observation.erase("elapsed_ms"); observation.erase("progress_idle_ms"); } };
    strip(value["native_load"]); for(auto& peer:value["peer_loads"]) strip(peer); return value;
}
int main(int argc,char** argv) {
    if(argc!=9) { std::cerr<<"error: arguments peer port runtime source digest mode budget-seconds players\n"; return 2; }
    main_thread=GetCurrentThreadId();
    auto& transport=mt2mp::net::SidecarClient::instance();
    try {
        auto& config=mt2mp::Config::instance(); config.peer_id=std::stoull(argv[1]); config.is_host=config.peer_id==1;
        config.local_test=true; config.session_id=500; config.session_token=std::string(32,'b'); config.sidecar_control_port=uint16_t(std::stoi(argv[2]));
        runtime=std::filesystem::absolute(argv[3]); config.user_data_dir=(runtime/"data").string(); config.runtime_root=runtime.string();
        std::filesystem::create_directories(runtime/"data"); std::filesystem::create_directories(runtime/"diagnostics");
        SetEnvironmentVariableA("MT2MP_RUNTIME_ROOT",runtime.string().c_str()); SetEnvironmentVariableA("SNAPSHOT_FIXTURE_WRITE_ROOT",config.user_data_dir.c_str());
        const auto physfs=std::filesystem::path(argv[0]).parent_path()/"libphysfs.dll";
        require(LoadLibraryW(physfs.c_str())!=nullptr,"fixture PhysFS load Win32="+std::to_string(GetLastError()));
        expected_digest=argv[5]; const std::string mode=argv[6]; fixture_mode=mode;
        char variant[32]{}; GetEnvironmentVariableA("MT2MP_FIXTURE_NETWORK_MAP",variant,sizeof(variant)); network_variant=variant;
        const auto budget=std::chrono::seconds(std::stoi(argv[7]));
        const auto players=size_t(std::stoi(argv[8]));
        mt2mp::net::initialize_snapshot_sync(); require(mt2mp::net::start_local_session(),"fixture session init failed");
        const auto start=std::chrono::steady_clock::now(); bool distributed=false,done=false,clock_advanced=false,slow_pending_observed=false,retired_handshake=false;
        while(std::chrono::steady_clock::now()-start<budget) {
            mt2mp::net::tick_local_session();
            if(config.is_host&&!distributed&&mt2mp::net::local_session_connected()&&mt2mp::net::local_session_peers()==players-1) {
                std::string error; require(mt2mp::net::distribute_snapshot(snapshot_id,expected_digest,std::filesystem::absolute(argv[4]),error),"distribution failed: "+error); distributed=true;
            }
            mt2mp::net::tick_snapshot_sync();
            auto progress=mt2mp::net::snapshot_progress();
            if(mode=="retired_replay"&&!config.is_host&&!retired_handshake&&progress.at("phase")=="active") {
                require(load_calls==1&&mt2mp::game::network_identity::map_ready,"retired fixture before original map confirmation");
                mt2mp::net::invalidate_snapshot_world("deliberate fixture client world retirement");
                mt2mp::game::network_identity::invalidate_world("deliberate fixture client world retirement");
                const auto handshake=json{{"type","fixture_retired_ready"}}.dump();
                require(transport.send(4,reinterpret_cast<const uint8_t*>(handshake.data()),uint32_t(handshake.size()),1),"retired fixture handshake queue rejected");
                retired_handshake=true;
                progress=mt2mp::net::snapshot_progress();
            }
            if(!clock_advanced) {
                if((mode=="load_slow"||mode=="load_timeout"||mode=="host_load_timeout")&&progress.at("phase")=="awaiting_load") {
                    if(mode=="load_slow"||(mode=="load_timeout"&&!config.is_host)||(mode=="host_load_timeout"&&config.is_host)) mt2mp::net::testing::advance_snapshot_clock(mode=="load_slow"?65:901);
                    clock_advanced=true;
                } else if(mode=="data_idle"&&progress.at("phase")=="transferring"&&(!config.is_host||progress.at("chunks_queued")>0)) {
                    mt2mp::net::testing::advance_snapshot_clock(61); clock_advanced=true;
                }
            }
            if(mode=="load_slow"&&progress.at("phase")=="awaiting_load"&&clock_advanced) {
                require(progress.at("peers_loaded")==0,"slow pending load falsely confirmed peer");
                if(!config.is_host) require(load_calls==1&&!cache_applied&&mt2mp::net::pending_snapshot_load_id()==snapshot_id,"slow pending load replayed or cache applied before activation");
                slow_pending_observed=true;
            }
            require(progress.at("direction")== (config.is_host?"send":"receive"),"progress direction differs");
            const auto total=progress.at("bytes_total").get<uint64_t>();
            require(progress.at("bytes_queued").get<uint64_t>()<=total&&progress.at("bytes_written").get<uint64_t>()<=total&&progress.at("bytes_acknowledged_min").get<uint64_t>()<=total,"progress bytes exceed manifest");
            require(progress.at("acknowledged_min").get<uint32_t>()<=progress.at("chunks_queued").get<uint32_t>(),"progress acknowledgement exceeds queued chunks");
            require(without_observation_clocks(progress)==without_observation_clocks(mt2mp::net::snapshot_progress()),"progress inspection mutated state");
            const auto state=mt2mp::net::snapshot_sync_status();
            if(mode=="valid"||mode=="load_slow") {
                if(config.is_host) done=distributed&&all_peers_loaded;
                else done=mt2mp::net::current_snapshot_id()==snapshot_id&&!transport.connected();
            } else done=state.starts_with("Snapshot fehlgeschlagen:")&&(config.is_host||!transport.connected());
            if(done) break;
            std::this_thread::sleep_for(config.peer_id==4?50ms:1ms);
        }
        require(done,"fixture observation timeout actual="+mt2mp::net::snapshot_sync_status()+" required<="+std::string(argv[7])+"s");
        const auto progress=mt2mp::net::snapshot_progress();
        if(mode=="valid"||mode=="load_slow") {
            require(progress.at("phase")=="active"&&progress.at("first_failure")==""&&!progress.at("pending_load").get<bool>(),"valid progress final state differs");
            require(progress.at("network_manifest_offered")== (network_variant!="fileonly"),"network manifest offer differs from native host map");
            if(config.is_host) {
                for(const auto& peer:progress.at("peer_loads")) {
                    require(peer.at("network_verified")== (network_variant=="confirmed"),"native network write authorization differs from map proof");
                    require(peer.at("network_error").get<std::string>().empty()== (network_variant=="confirmed"||network_variant=="fileonly"),"degraded map original cause missing");
                }
            } else require(progress.at("network_identity_error").get<std::string>().empty()== (network_variant=="confirmed"||network_variant=="fileonly"),"client degraded map cause missing");
            if(config.is_host) require(progress.at("peers_loaded")==players-1&&progress.at("bytes_acknowledged_min")==progress.at("bytes_total"),"progress did not confirm all peers");
            else require(progress.at("bytes_written")==progress.at("bytes_total"),"progress did not confirm complete staging write");
        } else require(progress.at("phase")=="failed"&&!progress.at("first_failure").get<std::string>().empty(),"failure progress lost first cause");
        if(mode=="load_slow") {
            require(slow_pending_observed&&clock_advanced,"slow native load phase never observed");
            const auto observation=config.is_host?progress.at("peer_loads")[0]:progress.at("native_load");
            require(observation.at("elapsed_ms")>=65000&&observation.at("progress_events")>=1,"native load over data deadline not retained");
        }
        if(mode=="valid"||mode=="load_slow") require(software_sync_calls==1&&(config.is_host||(load_calls==1&&cache_applied)),"native stub initial flow not completed");
        else if(!config.is_host) {
            if(mode=="retired_replay") require(retired_handshake&&load_calls==1&&loaded&&cache_applied&&!mt2mp::game::network_identity::map_ready,"retired world replayed native load or retained ready map");
            else if(mode=="load_timeout"||mode=="host_load_timeout") require(load_calls==1&&!loaded&&!cache_applied,"native load timeout replayed load or completed cache");
            else {
                require(load_calls==0&&!loaded,"failed transfer invoked native load");
                require(!std::filesystem::exists(runtime/"data/save/MPTestSession"/("snapshot-"+snapshot_id+".vrz")),"failed transfer accepted final file");
            }
        }
        PROCESS_MEMORY_COUNTERS_EX memory{}; memory.cb=sizeof(memory);
        require(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory))!=0,"memory measurement failed");
        const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
        std::cout<<"checkpoint: explicit-owner-shutdown"<<std::endl; transport.shutdown();
        std::cout<<json{{"passed",true},{"scope","transport and stub load guards, no game"},{"peer",config.peer_id},{"mode",mode},{"elapsed_ms",elapsed},{"peak_working_set_bytes",memory.PeakWorkingSetSize},{"private_usage_bytes",memory.PrivateUsage},{"native_load_calls",load_calls},{"software_sync_calls",software_sync_calls},{"status",mt2mp::net::snapshot_sync_status()},{"snapshot_progress",progress}}.dump()<<std::endl;
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"error: snapshot fixture cause="<<error.what()<<std::endl; transport.shutdown(); return 1;
    }
}
