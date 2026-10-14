#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include "effect_cleanup.h"

namespace placement_kiss {
struct Pose { std::array<float,3> position{}; std::array<float,4> quaternion{0,0,0,1}; std::array<float,3> scale{1,1,1}; bool operator==(const Pose&) const = default; };
struct Request { std::string action_uuid, type, variant; Pose pose; std::uint64_t world{}, epoch{}, generation{}, sequence{}; bool operator==(const Request&) const = default; };
enum class Phase { RejectedNoEffect, Prepared, Attempted, Confirmed, Unknown };
struct Receipt {
    std::uint64_t world{}, epoch{}, generation{}, sequence{}, native_uid{};
    std::string action_uuid, type, variant;
    Pose pose;
    std::uint32_t purchase_calls{}, add_calls{}, undo_calls{};
    std::int64_t cash_before{}, cash_after{}, native_cost{};
    std::uint32_t exception{};
};
struct Result { Phase phase{Phase::RejectedNoEffect}; bool original_attempted{}, original_returned{}, destructor_attempted{}, destructor_returned{}; std::uint32_t exception{}; std::string cause; Receipt receipt; };
struct PrivateBuild { void* receiver{}; std::uint32_t stage{}, exception{}; void* shard{}; effect_cleanup::Receipt preview_effect; };
class Services {
public:
    virtual ~Services() = default;
    // The controller owns authorization; this reader must check the current engine world.
    virtual bool read_current_context(const Request&, void*& shard, std::string& cause) = 0;
    // An existing action ledger must retain the full immutable request before allocation.
    virtual bool reserve_action_once(const Request&, std::string& cause) = 0;
    virtual bool prepare_private_build(const Request&, void* shard, PrivateBuild&, std::string& cause) = 0;
    virtual bool call_original_update(PrivateBuild&, std::uint32_t& exception) = 0;
    virtual bool read_actual_receipt(const Request&, Receipt&, std::string& cause) = 0;
    virtual bool destroy_private_build_once(PrivateBuild&, std::uint32_t& exception) = 0;
    virtual void record_result(const Request&, const Result&) noexcept = 0;
};
bool valid_request(const Request&, std::string& cause);
bool valid_scalars(const Request&, std::string& cause);
bool valid_receipt(const Request&, const Receipt&, std::string& cause);
class Adapter {
public:
    explicit Adapter(Services& services) : services_(services) {}
    Result execute(const Request&);
private:
    Services& services_;
    std::atomic_flag active_=ATOMIC_FLAG_INIT;
};
}
