#include "mt2mp/game/native_network.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/game/normal_load_identity.h"
#include "mt2mp/core/logger.h"
#include "network_identity.h"
#include <windows.h>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
uintptr_t image=0;
bool active=true;
int32_t map_capacity=0;
mt2mp::game::normal_load_identity::Status loaded;
void require(bool condition,const std::string& message) {
    if(!condition) throw std::runtime_error(message);
}
template<class T> void put(void* target,size_t offset,T value) {
    std::memcpy(static_cast<uint8_t*>(target)+offset,&value,sizeof(value));
}
int32_t capacity(void*) { return map_capacity; }
int32_t zero(void*) { return 0; }
bool contains(void*,const void*,int32_t*) { return false; }
void* lookup(uint64_t) { return nullptr; }
bool owns(const void*,void*) { return false; }
void* context(void*,int32_t) { return nullptr; }
}
namespace mt2mp {
Logger& Logger::instance() { static Logger value; return value; }
void Logger::log(LogCategory,LogLevel,const char*,int,const std::string& message) {
    std::cout<<message<<'\n';
}
}
namespace mt2mp::game::native {
bool ready() { return true; }
bool in_game() { return active; }
void* entrypoint(EntryPoint) { return reinterpret_cast<void*>(image+0x463ce0); }
bool read_world_status(WorldStatus& out,std::string& error) {
    out={}; out.world_present=active; out.active_mode_index=3; out.requested_mode_index=3;
    out.saves_in_progress=0; error.clear(); return true;
}
}
namespace mt2mp::game::normal_load_identity {
Status status() { return loaded; }
}
int main() {
    using namespace mt2mp::game;
    auto* memory=static_cast<uint8_t*>(VirtualAlloc(nullptr,0x1500000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!memory) { std::cerr<<"VirtualAlloc failed "<<GetLastError()<<'\n'; return 1; }
    try {
        image=reinterpret_cast<uintptr_t>(memory);
        struct Guard { size_t rva; const char* bytes; size_t size; };
        const Guard guards[]={
            {0x32b670,"\x57\x56\x53\x48\x83\xec\x20\x8b\x81\x60\x03\x00\x00\x48\x89\xce",16},
            {0x34f860,"\x48\x83\xec\x28\x31\xc0\x80\xb9\x50\x08\x00\x00\x00\x74\x24\x83",16},
            {0x2a5140,"\x55\x57\x56\x53\x48\x83\xec\x28\x8b\x51\x28\x48\x89\xcf\x85\xd2",16},
            {0x42cee0,"\x0f\xb6\x81\xea\x02\x00\x00\xc3",8},
            {0x42cef0,"\x53\x48\x83\xec\x20\x48\x83\xb9\x58\x02\x00\x00\x00\x48\x89\xcb",16},
            {0xddda40,"\x41\x57\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53\x48\x83\xec\x48",16},
            {0x89e690,"\x57\x56\x53\x48\x83\xec\x20\x48\x89\xce\x48\x89\xd3\x4c\x89\xc7",16},
            {0x47ce20,"\x53\x45\x31\xc0\x48\x85\xc9\x74\x31\x48\x83\x3d\xa7\x95\x5b\x01",16},
            {0x2a4730,"\x4c\x63\x41\x28\x45\x85\xc0\x7e\x27\x48\x8b\x41\x20\x4a\x8d\x0c",16},
            {0x89e0f0,"\x53\x48\x8b\x09\x48\x83\x79\x18\x00\x41\x89\xd0\x75\x32\x48\x8b",16}};
        for(const auto& guard:guards) std::memcpy(memory+guard.rva,guard.bytes,guard.size);
        std::array<uint8_t,0x380> map{};
        std::array<void*,4> regions{};
        put(memory,0x13df8c0,map.data());
        put(map.data(),0,image+0x1742860);
        put(map.data(),0x1c0,image+0x174ad20);
        put(map.data(),0x208,int32_t(0));
        auto layout=[&](int32_t count,int32_t allocated,void** data) {
            put(map.data(),0x358,data); put(map.data(),0x360,count); put(map.data(),0x364,allocated);
        };
        native_network::testing::backend(image,capacity,zero,zero);
        network_identity::testing::backend(image,contains,lookup,owns,context);
        std::string error;
        require(network_identity::initialize(error),"identity initialize: "+error);
        const std::string uuid="01234567-89ab-cdef-0123-456789abcdef",sha(64,'a');
        for(const int32_t allocated:{0,4}) {
            layout(0,allocated,allocated?regions.data():nullptr);
            native_network::State state;
            require(native_network::read_state(state,error),"empty native state: "+error);
            require(state.region_count==0&&state.nodes.empty()&&state.total_capacity==0,"empty census differs");
            require(network_identity::begin_host_capture(uuid,0,error),"empty host begin: "+error);
            require(network_identity::seal_host_capture(uuid,sha,error),"empty host seal: "+error);
            const auto host=network_identity::status_json(0);
            require(host.at("ready").get<bool>()&&host.at("staged_nodes")==0,"host receipt not ready");
            loaded.snapshot_uuid=uuid; loaded.snapshot_sha256=sha;
            require(network_identity::begin_client_load(uuid,sha,0,error),"empty client begin: "+error);
            require(network_identity::finalize_client_load(error),"empty client finalize: "+error);
            const auto client=network_identity::status_json(0);
            require(client.at("ready").get<bool>()&&client.at("keys_sha256")==host.at("keys_sha256"),"host/client receipt differs");
        }
        for(const auto bad:std::array<std::array<int32_t,2>,4>{{{{-1,4}},{{1,0}},{{16385,16385}},{{0,-1}}}}) {
            layout(bad[0],bad[1],regions.data()); native_network::State state;
            require(!native_network::read_state(state,error),"invalid bounds accepted by network");
            require(!network_identity::begin_host_capture(uuid,0,error),"invalid bounds accepted by identity");
        }
        layout(0,4,nullptr); native_network::State state;
        require(!native_network::read_state(state,error),"missing allocated backing accepted");
        require(!network_identity::begin_host_capture(uuid,0,error),"identity missing allocated backing accepted");
        layout(0,4,regions.data());
        require(!network_identity::begin_host_capture(uuid,1,error),"missing expected node accepted");
        require(network_identity::begin_host_capture(uuid,0,error),"host retry: "+error);
        layout(0,0,nullptr);
        require(!network_identity::seal_host_capture(uuid,sha,error),"changed array identity accepted");
        require(network_identity::begin_client_load(uuid,sha,0,error),"client SHA check begin: "+error);
        loaded.snapshot_sha256=std::string(64,'b');
        require(!network_identity::finalize_client_load(error),"mismatched snapshot SHA accepted");
        map_capacity=1;
        require(!native_network::read_state(state,error),"nonzero native aggregate on empty census accepted");
        map_capacity=0; active=false;
        require(!native_network::read_state(state,error),"inactive world accepted");
        active=true; memory[0x32b670]=0;
        require(!native_network::read_state(state,error),"wrong build prologue accepted");
        std::cout<<"Empty network state and identity regression passed\n";
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n'; VirtualFree(memory,0,MEM_RELEASE); return 1;
    }
    VirtualFree(memory,0,MEM_RELEASE); return 0;
}
