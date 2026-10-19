#pragma once
#include "typed_action/action_session.h"
#include "frame_limits.h"
#include "vip_transitions.h"
#include "monster_state.h"
#include "marketing_authority/state.h"
#include "management_live_state.h"
#include "release_cycle_state.h"
#include "control_input/control_replica.h"
#include "party_frame_state.h"
#include <array>
#include <string>
#include <vector>
#include <optional>
#include "network/node_state.h"
#include "tutorial_replication/state.h"
#include "bug_authority/codec.h"
namespace mt2mp::game::live_host_state {
constexpr size_t scalar_count=52;
using Context=action_session::Context;
using Binding=action_session::Binding;
struct Values {
    std::array<int32_t,6> calendar{};
    int32_t cash=0;
    std::array<int32_t,5> record{};
    std::array<std::array<int32_t,2>,20> lines{};
    bool operator==(const Values&)const=default;
};
struct Frame {
    uint64_t session=0,epoch=0,source_generation=0,revision=0,base_action_revision=0,time_revision=0;
    std::string snapshot,for_action;
    float selected=0,pause=0;
    Values values;
    std::optional<monsters::State> monsters;
    std::optional<network::State> network;
    std::optional<tutorial_replication::State> tutorial;
    std::optional<developer_staff::PoolState> developers;
    std::optional<bug_authority::State> bugs;
    std::optional<vip::Batch> vips;
    std::optional<marketing_authority::State> marketing;
    std::optional<management_simulation::LiveState> management;
    std::optional<release_cycle::Publication> release;
    std::optional<control_input::ReplicaState> control;
    std::optional<party_frame::State> party;
    bool operator==(const Frame&)const=default;
};
struct Pin {
    Context context;Binding binding;
    uint64_t common_epoch=0;
    uintptr_t image=0,world=0,clock=0,business=0,record=0,history=0;
    uint32_t thread=0;int32_t history_count=0,history_capacity=0;
    Values values;float selected=0,pause=0;
    bool operator==(const Pin&)const=default;
};
struct Port {
    virtual ~Port()=default;
    // Only the ordinary Host publication loop may defer before capture.
    virtual bool publication_ready(uint64_t,bool& waiting,std::string& e){waiting=false;e.clear();return true;}
    virtual bool read(Pin&,std::string&)=0;
    virtual bool store(const Pin&,size_t slot,int32_t expected,int32_t target,std::string&)=0;
};
struct ApplyReceipt {
    bool attempted=false,confirmed=false,unknown=false,duplicate=false;
    uint32_t attempted_stores=0,completed_stores=0;
    Values actual;bool actual_present=false;std::string cause;
};
enum class Admission {New,Duplicate,Stale,Wait,Rejected};
bool valid(const Frame&,std::string&);
bool encode(const Frame&,std::vector<uint8_t>&,std::string&);
bool decode(const uint8_t*,size_t,Frame&,std::string&);
Admission admit(uint64_t sender,const Context& bound,uint64_t common_epoch,uint64_t source_generation,
    const Frame* last,const Frame& incoming,uint64_t action_revision,uint64_t time_revision,bool action_pending,std::string&);
action_session::Ledger ledger(const Values&);
std::array<int32_t,scalar_count> scalars(const Values&);
ApplyReceipt apply(Port&,const Pin& expected_local,const Frame& target);
Admission admit_before_result(uint64_t sender,const Context&,uint64_t common_epoch,uint64_t source_generation,
    const Frame*,const Frame&,const action_session::Message&,uint64_t previous_action_revision,uint64_t time_revision,std::string&);
}
