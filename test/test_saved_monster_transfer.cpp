#include "mt2mp/net/snapshot_buffer.h"
#include "vendor/nlohmann/json.hpp"
#include <fstream>
#include <iostream>
#include <optional>
#include <cstring>
#include <stdexcept>
#define LOG_NET_INFO(...) ((void)0)
#define LOG_NET_ERROR(...) ((void)0)
namespace fixture {
using namespace mt2mp::net;
using json=nlohmann::json;
struct Config{bool is_host=false;std::string runtime_root;static Config& instance(){static Config c;return c;}};
SnapshotBuffer buffer,monster_buffer;
json incoming_monsters=nullptr;
#include "saved_monster_manifest_actual.inc"
std::string incoming_id="11111111-1111-4111-8111-111111111111",incoming_hash,incoming_world_name="MPTestSession",status,incoming_network_error,failure;
bool receiving=true,load_pending=false,load_observation_sent=false;
uint64_t incoming_epoch=1,incoming_origin_generation=0,incoming_started=0;
constexpr uint32_t snapshot_native_load_timeout_seconds=900,snapshot_data_idle_timeout_seconds=60;
uint64_t snapshot_now(){return 1;}
struct Watch{void begin(uint64_t){}}incoming_load;
struct Network{uint32_t expected_nodes=0;};std::optional<Network> incoming_network;
std::vector<json> messages;
int prepares=0,loads=0,arms=0;
void message(const json& value,uint64_t){messages.push_back(value);}
void fail(const std::string& cause){failure=cause;receiving=false;buffer.stop();monster_buffer.stop();}
namespace runtime_paths{std::filesystem::path diagnostics(const std::filesystem::path& p){return p;}}
namespace snapshot_world_path{std::string utf8(const std::filesystem::path& p){return p.string();}std::string load(const std::string&,const std::string&){return "original-native-load";}}
void need(bool v,const std::string& e){if(!v)throw std::runtime_error(e);}
namespace game {
namespace native_open_runtime {
bool prepare(const std::filesystem::path&,const std::string&,const std::string&,uint64_t,uint64_t&,std::string&,const std::string&){need(buffer.committed()&&(incoming_monsters.is_null()||monster_buffer.committed()),"native preparation before both commits");++prepares;return true;}
bool retire_unstarted(uint64_t,std::string&){return true;}bool before_request(uint64_t,std::string&){return true;}
}
namespace tutorial_replication{bool arm_client_load(const std::string&,uint64_t,std::string&){return true;}}
namespace time_input_diagnostics{void suspend(const std::string&) {}}
namespace hud_time_runtime{bool suspend(const std::string&,std::string&){return true;}}
namespace hud_calendar_runtime{bool suspend(const std::string&,std::string&){return true;}}
namespace time_integration{struct Time{bool suspend(const std::string&,std::string&){return true;}};Time& instance(){static Time t;return t;}}
namespace native{bool arm_software_replica(uint64_t,std::string&,const std::string&){return true;}bool disarm_software_replica(uint64_t,std::string&){return true;}bool request_load(const std::string&,std::string&){++loads;return true;}}
namespace normal_load_identity{bool begin(const std::string&,const std::string&,std::string&){++arms;return true;}void cancel(const std::string&) {}}
namespace network_identity{void invalidate_world(const std::string&){}bool begin_client_load(const std::string&,const std::string&,uint32_t,std::string&){return true;}}
}
#include "saved_monster_receive_actual.inc"
std::string digest(const std::string& bytes){SnapshotSha256 h;h.add(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size());return h.finish();}
const std::string native_bytes="native snapshot bytes",side_bytes="hash-bound identity companion";
unsigned case_number=0;
void setup(const std::filesystem::path& root,bool companion=true){
    buffer.stop();monster_buffer.stop();failure.clear();messages.clear();prepares=loads=arms=0;receiving=true;load_pending=false;
    auto path=root/std::to_string(GetCurrentProcessId())/std::to_string(++case_number);std::filesystem::create_directories(path);Config::instance().runtime_root=path.string();
    incoming_hash=digest(native_bytes);std::string e;
    need(buffer.begin(native_bytes.size(),1,path/"native.incoming",path/"native.vrz",incoming_hash,e),e);
    incoming_monsters=companion?json{{"revision",1},{"bytes",uint64_t(side_bytes.size())},{"chunks",uint32_t(1)},{"sha256",digest(side_bytes)}}:json(nullptr);
    if(companion)need(monster_buffer.begin(side_bytes.size(),1,path/"companion.incoming",path/"native.vrz.monsters.json",digest(side_bytes),e),e);
}
void chunk(uint32_t index,const std::string& bytes,uint64_t sender=1){std::vector<uint8_t> packet(40+bytes.size());std::memcpy(packet.data(),incoming_id.data(),36);std::memcpy(packet.data()+36,&index,4);std::memcpy(packet.data()+40,bytes.data(),bytes.size());receive_chunk(sender,packet.data(),uint32_t(packet.size()));}
void tests(const std::filesystem::path& root){
    setup(root);auto meta=parse_monsters({{"monster_identity",incoming_monsters}});need(monster_chunks(meta)==1,"metadata chunk count");
    chunk(0,native_bytes);need(failure.empty()&&buffer.committed()&&!monster_buffer.committed()&&!loads&&!prepares&&receiving,"native-only commit admitted load");
    need(messages.size()==1&&messages.back().at("next")==1,"native chunk ACK differs");
    chunk(1,side_bytes);need(failure.empty()&&prepares==1&&loads==1&&arms==1&&load_pending&&!receiving,"paired transaction did not load exactly once");
    need(messages.size()==3&&messages[1].at("next")==2&&messages[2].at("type")=="snapshot_verified","paired ACK order differs");monster_ack(meta,messages.back());
    for(int mode=0;mode<5;++mode){bool rejected=false;try{auto ack=messages.back();if(mode==0)ack.erase("monster_identity_sha256");if(mode==1)ack["monster_identity_sha256"]=std::string(64,'f');if(mode<2)monster_ack(meta,ack);else if(mode==2)monster_ack(nullptr,ack);else{auto bad=meta;bad[mode==3?"revision":"chunks"]=mode==3?json(1.0):json(uint32_t(2));parse_monsters({{"monster_identity",bad}});}}catch(const std::exception&){rejected=true;}need(rejected,"invalid companion metadata/ACK accepted");}
    setup(root);chunk(1,side_bytes);need(!failure.empty()&&!loads&&!prepares,"companion-before-native admitted");
    setup(root);chunk(0,native_bytes);auto bad=side_bytes;bad[0]='X';chunk(1,bad);need(failure=="snapshot SHA256 mismatch"&&!loads&&!prepares&&!monster_buffer.committed(),"corrupt companion loaded");
    setup(root);chunk(0,native_bytes);chunk(2,side_bytes);need(failure.find("chunk order")!=std::string::npos&&!loads,"skipped companion index loaded");
    setup(root);chunk(0,native_bytes,2);need(!failure.empty()&&!loads,"foreign source loaded");
    setup(root,false);need(parse_monsters(json::object()).is_null(),"legacy absence changed");chunk(0,native_bytes);need(failure.empty()&&loads==1&&prepares==1,"explicit legacy transaction failed");monster_ack(nullptr,messages.back());
}
}
int main(int argc,char** argv)try{if(argc!=2)throw std::runtime_error("temporary fixture directory required");fixture::tests(argv[1]);std::cout<<"actual receive body paired commits, ACK, corruption, order and legacy cases passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
