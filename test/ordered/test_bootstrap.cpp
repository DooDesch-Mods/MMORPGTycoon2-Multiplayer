#include "mt2mp/net/snapshot_sync.h"
#include "native_steps_draft.h"
#include <optional>
#include <map>
#include <algorithm>
#include <cassert>
#include <iostream>
#define LOG_NET_INFO(...) ((void)0)
namespace mt2mp::net {
using json=nlohmann::json;
std::optional<OrderedSnapshotIdentity> ordered_snapshot;
json ordered_manifest;
struct Peer{bool loaded=true,ordered_loaded=true;};std::map<uint64_t,Peer> outbound_peers;
bool ordered_load_acknowledged=true;uint64_t ordered_step_lease=7;uint32_t ordered_thread=123;
std::optional<MT2FW_MT2_StepServiceDraftV1> ordered_step_service;
std::array<std::string,5> ordered_bootstrap_hashes,ordered_covered_hashes;
std::string ordered_bootstrap_ready_hash,ordered_covered_ready_hash,ordered_covered_evidence_path,terminal;
bool ordered_bootstrap_sent=false,ordered_bootstrap_ready=false,ordered_covered_attempted=false;
SnapshotLoadWatch ordered_covered_watch;
MT2FW_MT2_StepReceipt receipt{};
SnapshotLoadWatch::Clock::time_point now{};
int reads=0,witness_calls=0,exports=0;bool fail_witness=false,zero_witness=false,mutate_world=false,mutate_receipt=false;
std::vector<std::pair<uint64_t,json>> messages;
auto snapshot_now(){return now;}
void fail(const std::string& error){terminal=error;}
bool ordered_fresh(std::string& error){error=terminal;return terminal.empty();}
bool read_ordered_snapshot(OrderedSnapshotIdentity& out,std::string& error){if(!ordered_fresh(error))return false;out=*ordered_snapshot;return true;}
bool valid_snapshot_digest(const std::string& x){return x.size()==64&&x.find_first_not_of("0123456789abcdef")==std::string::npos;}
void ordered_attach_ack(json& out){out["ordered_simulation"]=ordered_manifest;}
void message(const json& value,uint64_t peer=0){messages.push_back({peer,value});}
static void ordered_export_covered(const OrderedSnapshotIdentity&,const std::string&,const json&){++exports;ordered_covered_evidence_path="owned/evidence.json";}
MT2FW_MT2_Result MT2FW_CALL read(void*,uint64_t lease,MT2FW_MT2_StepReceipt* out){
 ++reads;*out=receipt;if(lease!=7)return MT2FW_MT2_INVALID;
 if(mutate_receipt&&reads==2)++out->world_epoch;return MT2FW_MT2_OK;
}
bool witness(std::array<uint8_t,32>& digest,json& details,std::string& error){
 ++witness_calls;if(fail_witness){error="reader original cause";return false;}
 digest.fill(zero_witness?0:0x22);details={{"canonical",{{"buildings",1}}},{"complete_world_state",false}};
 if(mutate_world)++ordered_snapshot->world_epoch;return true;
}
#include "ordered_bootstrap_body.inc"
void reset(uint64_t peer=1,uint8_t mask=3){
 ordered_snapshot=OrderedSnapshotIdentity{};auto& p=*ordered_snapshot;p.members={0,100,200,300,400};
 for(size_t i=1;i<5;++i)if(!(mask&(1u<<(i-1))))p.members[i]=0;
 p.lobby=88;p.session=99;p.local_peer=peer;p.peer_mask=mask;p.region_rng_mode=1;
 p.snapshot_uuid="00000000-0000-4000-8000-000000000001";p.sha256=std::string(64,'a');p.bytes=999;p.seed=42;p.native_generation=4;p.world_epoch=5;
 p.seed_present=p.file_verified=p.native_prepared=p.request_attempted=p.loaded=true;
 ordered_manifest={{"session",99},{"members",p.members}};outbound_peers.clear();for(uint64_t i=2;i<5;++i)if(p.members[i])outbound_peers.emplace(i,Peer{});
 ordered_bootstrap_hashes={};ordered_covered_hashes={};ordered_bootstrap_ready_hash.clear();ordered_covered_ready_hash.clear();ordered_covered_evidence_path.clear();terminal.clear();
 ordered_bootstrap_sent=ordered_bootstrap_ready=ordered_covered_attempted=false;ordered_covered_watch={};
 receipt={};receipt.size=sizeof(receipt);receipt.phase=MT2FW_MT2_STEP_ARMED;receipt.outcome=MT2FW_MT2_STEP_NOT_ATTEMPTED;
 receipt.actual_role=peer==1?MT2FW_MT2_SESSION_HOST:MT2FW_MT2_SESSION_CLIENT;receipt.lease=7;receipt.session=99;receipt.thread=123;receipt.generation=4;receipt.world_epoch=5;
 receipt.dt_bits=MT2FW_MT2_STEP_FIXED_DT_BITS;receipt.seed_attempted=receipt.seed_returned=receipt.bootstrap_held=receipt.workers_held=receipt.callbacks_inflight=1;
 receipt.bootstrap_coverage=MT2FW_MT2_STEP_COVERAGE_INITIAL_DEVELOPERS;std::fill(std::begin(receipt.initial_witness_sha256),std::end(receipt.initial_witness_sha256),0x11);
 MT2FW_MT2_StepServiceDraftV1 service{};service.size=sizeof(service);service.version=MT2FW_MT2_STEP_DRAFT_VERSION;service.context=(void*)1;service.read=&read;ordered_step_service=service;
 reads=witness_calls=exports=0;messages.clear();now={};fail_witness=zero_witness=mutate_world=mutate_receipt=false;ordered_load_acknowledged=true;
}
json packet(const char* type="ordered_bootstrap"){
 json p={{"type",type},{"id",ordered_snapshot->snapshot_uuid},{"initial_witness_sha256",std::string(64,'1')},
         {"covered_witness_sha256",std::string(64,'2')}};ordered_attach_ack(p);return p;
}
}
using namespace mt2mp::net;int checks=0;
void ack(bool success,bool expected_ready){bool ready=true;std::string error;
 assert(acknowledge_ordered_bootstrap(*ordered_step_service,7,&witness,ready,error)==success);++checks;
 assert(ready==expected_ready);++checks;if(!success){assert(!error.empty());++checks;}}
template<class F>void rejected(F f){bool threw=false;try{f();}catch(const std::exception& e){threw=!std::string(e.what()).empty();}assert(threw);++checks;}
int main(){
 reset();ack(true,false);assert(witness_calls==1&&exports==1&&messages.empty());++checks;
 ack(true,false);assert(witness_calls==1&&exports==1);++checks;
 assert(ordered_receive_bootstrap(2,packet()));++checks;ack(true,true);
 assert(messages.size()==1&&messages[0].second==packet("ordered_bootstrap_ready"));++checks;
 ack(true,true);assert(witness_calls==1&&messages.size()==1);++checks;
 reset(2);ack(true,false);assert(messages.size()==1&&messages[0].first==1&&messages[0].second==packet());++checks;
 assert(ordered_receive_bootstrap(1,packet("ordered_bootstrap_ready")));++checks;ack(true,true);assert(witness_calls==1);++checks;
 reset(2);rejected([&]{ordered_receive_bootstrap(1,packet("ordered_bootstrap_ready"));});
 reset();ack(true,false);auto p=packet();p["covered_witness_sha256"]=std::string(64,'3');rejected([&]{ordered_receive_bootstrap(2,p);});
 reset();assert(ordered_receive_bootstrap(2,packet()));++checks;p=packet();p["covered_witness_sha256"]=std::string(64,'3');rejected([&]{ordered_receive_bootstrap(2,p);});
 reset();p=packet();p.erase("covered_witness_sha256");rejected([&]{ordered_receive_bootstrap(2,p);});
 reset();p=packet();p["id"]="other";rejected([&]{ordered_receive_bootstrap(2,p);});
 reset();p=packet();p["ordered_simulation"]["session"]=2;rejected([&]{ordered_receive_bootstrap(2,p);});
 reset();rejected([&]{ordered_receive_bootstrap(3,packet());});outbound_peers[2].ordered_loaded=false;rejected([&]{ordered_receive_bootstrap(2,packet());});
 reset(2);ack(true,false);p=packet("ordered_bootstrap_ready");p["covered_witness_sha256"]=std::string(64,'3');rejected([&]{ordered_receive_bootstrap(1,p);});
 reset();fail_witness=true;ack(false,false);assert(witness_calls==1&&terminal.find("reader original cause")!=std::string::npos);++checks;ack(false,false);assert(witness_calls==1);++checks;
 reset();zero_witness=true;ack(false,false);assert(exports==0);++checks;
 reset();mutate_world=true;ack(false,false);assert(exports==0);++checks;
 reset();mutate_receipt=true;ack(false,false);assert(exports==0);++checks;
 reset();receipt.callbacks_inflight=0;ack(false,false);assert(witness_calls==0);++checks;
 reset();receipt.bootstrap_held=0;receipt.bootstrap_coverage=0;ack(true,false);assert(witness_calls==0);++checks;
 reset();ack(true,false);now+=std::chrono::seconds(snapshot_native_load_timeout_seconds+1);ack(false,false);assert(witness_calls==1&&terminal.find("timeout elapsed_ms=")!=std::string::npos);++checks;
 reset(1,1);ack(true,true);assert(witness_calls==1);++checks;
 reset(1,15);ack(true,false);for(int peer:{2,3})assert(ordered_receive_bootstrap(peer,packet()));ack(true,false);assert(ordered_receive_bootstrap(4,packet()));ack(true,true);
 reset();const auto proof=*ordered_snapshot;const auto held=receipt;
 assert(ordered_covered_same(proof,held,proof,held));++checks;
 for(int field=0;field<10;++field){auto actual=held;
  switch(field){case 0:actual.tick=1;break;case 1:actual.invocation=1;break;case 2:actual.timer_calls=1;break;case 3:actual.original_attempted=1;break;
   case 4:actual.workers_held=0;break;case 5:actual.completed_tick=1;break;case 6:actual.session++;break;case 7:actual.thread++;break;
   case 8:actual.initial_witness_sha256[0]++;break;case 9:actual.phase=MT2FW_MT2_STEP_HELD;break;}
  assert(!ordered_covered_same(proof,held,proof,actual));++checks;
 }
 for(int field=0;field<7;++field){auto actual=proof;switch(field){case 0:actual.members[2]++;break;case 1:actual.seed++;break;case 2:actual.bytes++;break;
  case 3:actual.loaded=false;break;case 4:actual.snapshot_uuid="other";break;case 5:actual.native_generation++;break;case 6:actual.world_epoch++;break;}
  assert(!ordered_covered_same(proof,held,actual,held));++checks;
 }
 std::cout<<checks<<" actual initial held bootstrap protocol checks passed\n";
}
