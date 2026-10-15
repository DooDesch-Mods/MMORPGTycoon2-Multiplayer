
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
struct World { void* game{}; void* mode{}; void* state{}; void* world{}; void* map{}; };
World candidate_world, observed_world;
bool capture_snapshot_identities = true, armed = true;
std::string uuid="local-save", sha="local-sha", error, reason="retained timeout";
struct Port {
    std::string bound="network", digest="network-sha", failure;
    int arms=0, seals=0, cancels=0;
    bool arm_ok=true, seal_ok=true;
    bool arm(const std::string& id, std::string& e) {
        ++arms; if (!arm_ok) { e="original arm failure"; return false; }
        bound=id; return true;
    }
    bool seal(const std::string& value, std::string& e) {
        ++seals; if (!seal_ok) { e="original seal failure"; return false; }
        digest=value; return true;
    }
    void cancel(const std::string& cause) { ++cancels; failure=cause; bound.clear(); }
} quest, npc, bugs;
namespace npc_quest_target_runtime {
bool arm_host(const std::string& id, World, std::string& e) { return quest.arm(id,e); }
bool seal_host(const std::string&, const std::string& value, World, std::string& e) { return quest.seal(value,e); }
void cancel(const std::string& cause) { quest.cancel(cause); }
}
namespace npc_saved_identity_runtime {
bool arm_host(const std::string& id, World, std::string& e) { return npc.arm(id,e); }
bool seal_host(const std::string&, const std::string& value, World, std::string& e) { return npc.seal(value,e); }
void cancel(const std::string& cause) { npc.cancel(cause); }
}
namespace bug_authority {
bool arm_snapshot(const std::string& id, const std::string& digest, bool host, std::string& e) {
    assert(digest.empty() && host); return bugs.arm(id,e);
}
bool seal_snapshot(const std::string& id, const std::string& value, std::string& e) {
    assert(id==uuid); return bugs.seal(value,e);
}
void cancel_snapshot(const std::string& cause) { bugs.cancel(cause); }
}
bool fail(std::string& e, const std::string& cause) { e=cause; return false; }
bool rejected(const std::string& cause) { error=cause; return false; }
namespace net {
std::string id="network";
uint64_t epoch=1;
std::string current_snapshot_id() { return id; }
uint64_t current_world_epoch() { return epoch; }
}

#include "snapshot_capture_policy_body.inc"

void reset(bool bind) { quest={}; npc={}; bugs={}; error.clear(); armed=true; capture_snapshot_identities=bind; }
int main() {
    assert(!routing(false)); assert(routing(true));
    net::id.clear(); assert(routing(false)); net::id="network";
    net::epoch=0; assert(routing(false)); net::epoch=1;
    reset(routing(false));
    assert(actual_arm()); assert(actual_seal()); actual_cancel();
    for (const auto* p : {&quest,&npc,&bugs}) {
        assert(p->bound=="network" && p->digest=="network-sha");
        assert(p->arms==0 && p->seals==0 && p->cancels==0);
    }
    reset(routing(true)); assert(actual_arm()); assert(actual_seal());
    for (const auto* p : {&quest,&npc,&bugs}) assert(p->bound==uuid && p->digest==sha && p->arms==1 && p->seals==1);
    actual_cancel(); assert(quest.failure==reason && npc.failure==reason && bugs.failure==reason);
    reset(true); quest.arm_ok=false; assert(!actual_arm());
    assert(error=="Quest target capture arm failed: original arm failure" && npc.arms==0 && bugs.arms==0 && !armed);
    reset(true); npc.arm_ok=false; assert(!actual_arm());
    assert(error=="NPC identity capture arm failed: original arm failure" && quest.cancels==1 && bugs.arms==0 && !armed);
    reset(true); bugs.arm_ok=false; assert(!actual_arm());
    assert(error=="Bug capture arm failed: original arm failure" && quest.cancels==1 && npc.cancels==1 && !armed);
    reset(true); quest.seal_ok=false; assert(!actual_seal());
    assert(error=="Quest target snapshot seal failed: original seal failure" && npc.seals==0 && bugs.seals==0);
    reset(true); npc.seal_ok=false; assert(!actual_seal());
    assert(error=="NPC identity snapshot seal failed: original seal failure" && bugs.seals==0);
    reset(true); bugs.seal_ok=false; assert(!actual_seal());
    assert(error=="Bug snapshot seal failed: original seal failure");
    reset(false); quest.failure=bugs.failure="prior binding failure"; actual_cancel();
    assert(quest.failure=="prior binding failure" && bugs.failure=="prior binding failure");
    std::cout << "PASS actual capture routing and arm/seal/cancel boundaries\n";
}
