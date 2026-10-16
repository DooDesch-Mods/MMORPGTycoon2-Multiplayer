#pragma once
#include "../bug_authority/job_state.h"
#include <optional>
namespace mt2mp::action_session {
struct BugFixIntent {
    game::bug_authority::BugIdentity bug;
    bool schedule=true;
    std::optional<game::bug_authority::Birth> expected_job;
    bool operator==(const BugFixIntent&) const=default;
};
}
