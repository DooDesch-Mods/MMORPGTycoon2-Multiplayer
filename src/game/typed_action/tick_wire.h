#pragma once
#include "tick_commands.h"

namespace mt2mp::action_session::tick_commands::wire {
// This envelope travels through the existing authenticated action channel.
// Command bodies are the existing Request codecs, not a second action schema.
enum class SnapshotPhase:uint8_t {Prepared=1,Release=2,Released=3};
struct SnapshotControl {
    uint64_t session=0,tick=0,order=0;std::string snapshot_uuid,nonce;
    SnapshotPhase phase=SnapshotPhase::Prepared;uint8_t ack_mask=0;
    std::array<uint8_t,32> witness{};
    bool operator==(const SnapshotControl&)const=default;
};
struct PeriodicRequest {
    uint64_t session=0,tick=0,invocation=0,peer=0;
    std::string snapshot_uuid;
    bool operator==(const PeriodicRequest&)const=default;
};
struct Packet {
    uint8_t peer_mask=0;
    std::variant<Input,Batch,Receipt,SnapshotControl,PeriodicRequest> value;
    bool operator==(const Packet&)const=default;
};
bool encode(const Packet&,std::vector<uint8_t>&,std::string&);
bool decode(const uint8_t*,size_t,Packet&,std::string&);
bool current(Session&,const Context&,std::string&);
Admission offer(Queue&,uint64_t authenticated_sender,const Input&,Assigned&,std::string&);
// Encodes before sealing. On failure no batch is sealed and no bytes escape.
bool seal(Queue&,Batch&,std::vector<uint8_t>&,std::string&);
// The existing controller owns the handler and logs the returned failure.
// No receive branch executes an engine command or advances simulation.
Admission receive(Queue&,Session&,const Context&,uint64_t authenticated_sender,
                  const uint8_t*,size_t,Assigned&,std::string&);
// Caller retains the exact packet on send failure. Success is transport enqueue,
// not proof that a peer consumed a batch or completed a native operation.
bool send(Transport&,Session&,const Context&,uint64_t target,
          const std::vector<uint8_t>&,std::string&);
}
