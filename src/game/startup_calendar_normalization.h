#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

namespace mt2mp::game::startup_calendar {
enum class Result { Unsupported, Normalized, Failed };
struct State {
    uintptr_t image=0,clock=0,business=0,history=0,record=0,member=0,software=0;
    std::array<uintptr_t,5> world{};
    uint64_t generation=0,epoch=0;
    uint32_t thread=0;
    std::string uuid,sha;
    int32_t history_count=0,history_capacity=0,software_state=0;
    std::array<uint64_t,6> day_calls{};
    std::array<unsigned char,0x70> clock_bytes{};
    std::array<unsigned char,0x110> business_bytes{};
    std::array<unsigned char,0x5d0> record_bytes{};
    bool operator==(const State&) const = default;
};
template<class T,size_t N> T value(const std::array<unsigned char,N>& bytes,size_t offset) {
    T result{};std::memcpy(&result,bytes.data()+offset,sizeof result);return result;
}
template<class T,size_t N> void assign(std::array<unsigned char,N>& bytes,size_t offset,T next) {
    std::memcpy(bytes.data()+offset,&next,sizeof next);
}
inline bool supported(const State& s,std::string& error) {
    const auto reject=[&](const char* cause){error=cause;return false;};
    if(!s.generation||!s.epoch||s.uuid.empty()||s.sha.empty())return reject("completed native world origin absent");
    if(s.history_count!=1||s.history_capacity<1||!s.history||!s.record||s.member!=s.record)
        return reject("History is not one owned CurrentRecord");
    if(value<int32_t>(s.record_bytes,0x18)!=2||value<int32_t>(s.record_bytes,0x1c)!=0)
        return reject("CurrentRecord is not version2 day0");
    if(value<uintptr_t>(s.business_bytes,0x100)!=s.record||
       value<int32_t>(s.business_bytes,0x38)!=value<int32_t>(s.record_bytes,0x20))
        return reject("Business CurrentRecord pointer/cash differs");
    if(value<int32_t>(s.business_bytes,0x3c)>0)return reject("initial loan remains pending");
    if(s.software_state>0)return reject("Clock Resolve software recovery branch is active");
    if(value<int32_t>(s.clock_bytes,0x38)!=1||value<int32_t>(s.clock_bytes,0x3c)!=1||
       value<int32_t>(s.clock_bytes,0x40)!=1||value<int32_t>(s.clock_bytes,0x44)!=0||
       value<int32_t>(s.clock_bytes,0x4c)<=0)
        return reject("Clock legacy date does not select the bounded day0-to-day1 migration");
    const auto scale=value<float>(s.clock_bytes,0x58),pause=value<float>(s.clock_bytes,0x5c);
    if(scale!=0||!std::isfinite(pause)||pause<0)return reject("Clock is not stopped with a valid pause scale");
    error.clear();return true;
}

// Port reads real native state and invokes the two original functions. Never retry a
// failed call: Clock may already have changed even when its return was not observed.
template<class Port> Result normalize(Port& port,std::string& error) {
    State before,checked;
    if(!port.read(before,error))return Result::Failed;
    if(!supported(before,error))return Result::Unsupported;
    if(!port.verify_code(error)||!port.read(checked,error))return Result::Failed;
    if(checked!=before){error="startup calendar owners/state changed before original Resolve";return Result::Failed;}
    if(!port.resolve_clock(before,error))return Result::Failed;
    auto expected=before;
    assign(expected.clock_bytes,0x44,int32_t{1});
    assign(expected.clock_bytes,0x40,int32_t{2});
    if(!port.read(checked,error))return Result::Failed;
    if(checked!=expected){error="startup Clock Resolve changed fields outside the expected calendar migration";return Result::Failed;}
    if(!port.resolve_business(checked,error))return Result::Failed;
    assign(expected.record_bytes,0x1c,int32_t{1});
    if(!port.read(checked,error))return Result::Failed;
    if(checked!=expected){error="startup Business PostResolve changed owners/ledger outside the expected day migration";return Result::Failed;}
    error.clear();return Result::Normalized;
}
}
