#pragma once
#include "vendor/nlohmann/json.hpp"
#include <array>
#include <chrono>
#include <cstdint>
namespace mt2mp::game::live_host_state::apply_timing {
enum class Part:size_t {Take,BeforeDay,DayTake,AfterDay,Preflight,FullBefore,Scalar,Motion,FullFinal,EconomyRead,DayRead,MotionOriginal,MemoryVirtualQuery,Count};
struct Totals {uint64_t count=0,total_ns=0,max_ns=0;};
// The diagnostic reader sees its game-thread aggregates. Rejected foreign-thread
// calls keep separate counters and cannot race with the bound thread.
inline thread_local std::array<Totals,size_t(Part::Count)> totals{};
inline thread_local size_t take_depth=0;
inline void record(Part part,uint64_t ns){auto& t=totals[size_t(part)];++t.count;t.total_ns+=ns;if(ns>t.max_ns)t.max_ns=ns;}
class Scope {
    Part part_;
    std::chrono::steady_clock::time_point started_=std::chrono::steady_clock::now();
public:
    explicit Scope(Part part):part_(part){if(part_==Part::Take)++take_depth;}
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
    ~Scope(){const auto ns=uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started_).count());if(part_==Part::Take)--take_depth;record(part_,ns);}
};
inline nlohmann::json snapshot(){
    static constexpr const char* names[]={"client_take","before_day","day_take","after_day","preflight","full_before","scalar","motion","full_final","economy_native_read","day_native_read","motion_original_calls","memory_virtual_query"};
    static_assert(std::size(names)==size_t(Part::Count));
    nlohmann::json out=nlohmann::json::object();
    for(size_t i=0;i<totals.size();++i){const auto& t=totals[i];out[names[i]]={{"count",t.count},{"total_ns",t.total_ns},{"max_ns",t.max_ns}};}
    return out;
}
}
