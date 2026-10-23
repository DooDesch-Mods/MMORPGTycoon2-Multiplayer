#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace mt2mp::slow_frame {
constexpr uint64_t threshold_ns=100'000'000, interval_ns=5'000'000'000;
constexpr size_t outer_count=8, inner_count=12, counter_count=outer_count+inner_count;
using Counters=std::array<uint64_t,counter_count>;
struct Sample {
    uint64_t frame=0,elapsed_ns=0;
    Counters work{};
};
inline Sample delta(uint64_t frame,uint64_t elapsed,const Counters& before,const Counters& after) noexcept {
    Sample result{frame,elapsed,{}};
    for(size_t i=0;i<counter_count;++i)result.work[i]=after[i]>=before[i]?after[i]-before[i]:0;
    return result;
}
inline size_t dominant(const Sample& value,size_t begin,size_t end) noexcept {
    size_t result=begin;
    for(size_t i=begin+1;i<end;++i)if(value.work[i]>value.work[result])result=i;
    return result;
}
struct Report {Sample first,worst;uint64_t count=0;};
// Retain the first and worst stall between log writes, including a final stall
// followed by healthy frames. Nested counters are inclusive, not additive.
class Tracker {
    bool emitted_=false;
    uint64_t last_emit_=0;
    std::optional<Report> pending_;
public:
    std::optional<Report> observe(uint64_t now,const Sample& sample) noexcept {
        if(sample.elapsed_ns>=threshold_ns){
            if(!pending_)pending_=Report{sample,sample,1};
            else {++pending_->count;if(sample.elapsed_ns>pending_->worst.elapsed_ns)pending_->worst=sample;}
        }
        if(!pending_||(emitted_&&now>=last_emit_&&now-last_emit_<interval_ns))return {};
        auto result=pending_;pending_.reset();emitted_=true;last_emit_=now;return result;
    }
};
}
