#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace mt2mp::game::node_observer {
enum class Leaf : uint8_t { Presentation, Network, Finish, Drain, Charge, Count };
inline constexpr std::array<uintptr_t,5> rvas{0x42d570,0x42c8f0,0x42cb10,0x42cc10,0x42cc30};
struct Scalars {
    std::array<float,4> values{};
    std::array<uint8_t,3> flags{};
    bool operator==(const Scalars&)const=default;
};
struct Observation {
    uint64_t sequence=0;
    uint32_t thread=0,read_before_fault=0,read_after_fault=0,original_fault=0;
    uintptr_t caller_rva=0;
    float argument=0;
    bool original_returned=false,before_present=false,after_present=false,argument_present=false;
    Scalars before,after;
};
struct LeafState {
    uint64_t entered=0,returned=0,original_failed=0,foreign_thread=0,read_failed=0,changed=0;
    Observation first,last;
};
struct Status {
    bool prepared=false,bound=false,fresh_owner_confirmed=false;
    uint32_t main_thread=0,hook_mask=0,inflight=0;
    uint64_t rejected=0,foreign_receivers=0;
    uint64_t binding_revision=0;
    uint64_t invalid_leaf=0,ticket_exhausted=0,observation_capacity=0,unknown_ticket=0,invalid_caller=0;
    std::string action_uuid,snapshot,cause;
    bool snapshot_binding=false;
    int32_t region_id=-1,node_index=-1;
    std::array<LeafState,5> leaves{};
};
// Ticket zero requires no observation completion; the caller still invokes Original once.
using Ticket=uint64_t;
bool prepare(uintptr_t image,const std::string& exe_sha,uint32_t main_thread,std::string&);
bool set_hook_mask(uint32_t,std::string&);
bool bind_action(const std::string& action_uuid,std::string&);
bool bind_snapshot(const std::string& snapshot,int32_t region_id,int32_t node_index,const std::string& graph_sha256,std::string&);
Ticket enter(Leaf,void* receiver,uintptr_t caller_rva,float argument)noexcept;
void leave(Ticket,bool original_returned,uint32_t original_fault)noexcept;
bool read_status(Status&,std::string&);
bool suspend(const std::string& reason,std::string&);
void* entrypoint(Leaf)noexcept;
}
