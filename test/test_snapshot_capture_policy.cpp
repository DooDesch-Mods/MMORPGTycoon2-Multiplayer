
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <utility>
struct World { void* game{}; void* mode{}; void* state{}; void* world{}; void* map{}; };
World candidate_world, observed_world;
bool capture_snapshot_identities = true, armed = true;
std::string uuid="local-save", sha="local-sha", error, reason="retained timeout";
struct Port {
    std::string bound="network", digest="network-sha", failure;
    int arms=0, seals=0, cancels=0, stages=0;
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

Port monsters, bosses, districts, parties, network;
int dungeon_scenes=0, classifications=0, companion_writes=0, network_reads=0;
std::vector<int> scene_exteriors;
bool network_read_ok=true, companion_ok=true, snapshot_network_observer_begun=false;
std::string id="local-save", digest="local-sha", world_name="MPTestFixture", snapshot_path="owned.vrz";
uint64_t length=100;
struct Identity {std::string error;} identity;
namespace dungeon_scene_identity_runtime {
bool classify_capture(const std::string&,const std::string&,World,const std::vector<int>&,int&,std::string&){++classifications;return true;}
}
#define PARTY_CAPTURE_PORT(NAME,PORT) namespace NAME { \
bool arm_host(const std::string& value,World,std::string& e){return PORT.arm(value,e);} \
bool seal_host(const std::string&,const std::string& value,World,std::string& e){return PORT.seal(value,e);} \
bool stage_scenes(int,std::string&){++PORT.stages;return true;} \
void cancel(const std::string& cause){PORT.cancel(cause);} }
PARTY_CAPTURE_PORT(monster_saved_identity_runtime,monsters)
PARTY_CAPTURE_PORT(boss_saved_identity_runtime,bosses)
PARTY_CAPTURE_PORT(district_saved_identity_runtime,districts)
PARTY_CAPTURE_PORT(party_saved_identity_runtime,parties)
#undef PARTY_CAPTURE_PORT
namespace game {
namespace native_network {
struct State {uint32_t node_slots=2,null_node_slots=0;};
bool read_state(State&,std::string& e){++network_reads;if(!network_read_ok){e="original Network census failure";return false;}return true;}
}
namespace network_identity {
bool begin_host_capture(const std::string& value,uint32_t count,std::string& e,const std::string& name){assert(count==2&&name==world_name);return network.arm(value,e);}
bool seal_host_capture(const std::string&,const std::string& value,std::string& e){return network.seal(value,e);}
void cancel_pending(const std::string& cause){network.cancel(cause);}
}
namespace monster_saved_identity_runtime {
bool write_companion(const std::string&,const std::string&,uint64_t,const std::string&,std::string& e){++companion_writes;if(!companion_ok){e="original companion failure";return false;}return true;}
}
}
#include "snapshot_capture_policy_body.inc"

void reset(bool bind) { quest={}; npc={}; bugs={}; monsters={};bosses={};districts={};parties={};network={};classifications=companion_writes=network_reads=0;network_read_ok=companion_ok=true;snapshot_network_observer_begun=false; error.clear(); armed=true; capture_snapshot_identities=bind; }
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
    // Local fixture save has complete native file observation but no published alias observers.
    reset(routing(false));
    assert(actual_arm()&&actual_other_arm()&&actual_stage()&&actual_seal()&&actual_other_seal());
    actual_network_arm(false);actual_network_seal(false);actual_network_cancel(false);actual_cancel();actual_other_cancel();
    for(const auto* p:{&quest,&npc,&bugs,&monsters,&bosses,&districts,&parties,&network})
        assert(p->bound=="network"&&p->digest=="network-sha"&&p->arms==0&&p->seals==0&&p->cancels==0&&p->stages==0);
    assert(classifications==0&&network_reads==0&&companion_writes==0&&!snapshot_network_observer_begun);
    for(bool first_origin:{false,true}){
        net::id=first_origin?"":"network";reset(routing(!first_origin));
        assert(actual_arm()&&actual_other_arm()&&actual_stage()&&actual_seal()&&actual_other_seal());
        actual_network_arm(true);actual_network_seal(true);
        for(const auto* p:{&quest,&npc,&bugs,&monsters,&bosses,&districts,&parties,&network})assert(p->arms==1&&p->seals==1);
        assert(classifications==1&&network_reads==1&&companion_writes==1&&snapshot_network_observer_begun);
        actual_cancel();actual_other_cancel();actual_network_cancel(true);
        for(const auto* p:{&quest,&npc,&bugs,&monsters,&bosses,&districts,&parties,&network})assert(p->cancels==1);
    }
    net::id="network";
    reset(true);network.arm_ok=false;try{actual_network_arm(true);assert(false);}catch(const std::runtime_error& cause){assert(std::string(cause.what()).find("original arm failure")!=std::string::npos);}
    assert(!snapshot_network_observer_begun&&network.seals==0&&companion_writes==0);
    reset(true);network.seal_ok=false;try{actual_network_seal(true);assert(false);}catch(const std::runtime_error& cause){assert(std::string(cause.what())=="Network snapshot alias seal incomplete: original seal failure");}
    assert(companion_writes==0);
    reset(true);companion_ok=false;try{actual_network_seal(true);assert(false);}catch(const std::runtime_error& cause){assert(std::string(cause.what())=="Monster snapshot companion incomplete: original companion failure");}
    reset(true);parties.arm_ok=false;assert(!actual_other_arm()&&error=="Party identity capture arm failed: original arm failure");
    reset(true);monsters.seal_ok=false;assert(!actual_other_seal()&&error=="Monster identity snapshot seal failed: original seal failure");
    std::cout << "PASS actual capture routing and arm/seal/cancel boundaries\n";
}
