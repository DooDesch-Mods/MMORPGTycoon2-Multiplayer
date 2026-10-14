#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace mt2mp::game::time_session {
constexpr uint8_t channel=8;
constexpr size_t wire_size=64, history_size=64, outbox_size=256;
enum class Role:uint8_t { Host,Client };
enum class Intent:uint8_t { Pause,Normal,Fast,Fastest,TogglePauseNormal };
enum class Kind:uint8_t { Request=1,Receipt,State,PresentationAck };
enum class Result:uint8_t { None,Confirmed,NoChange,Rejected,Unknown,Locked };
enum class Effect:uint8_t { Confirmed,RejectedNoEffect,Unknown };
enum class Code:uint16_t {
    None,Malformed,WrongRole,UntrustedSender,WrongEpoch,SequenceGap,SequenceExpired,
    ConflictingDuplicate,FutureObservation,Backpressure,NativeRead,NativeRejected,
    NativeUnknown,ReadbackMismatch,Locked,PresentationRejected,PresentationUnknown,
    WrongThread,Reentrant,NotStarted,RevisionExhausted,CallbackException,
    UnexpectedReceipt,NoState,StaleState
};
enum class Disposition:uint8_t { Accepted,Duplicate,Ignored,Rejected,Locked };
struct Clock {float selected=0,pause=0;};
struct Snapshot {
    // Revision can name a failed acknowledgement even when no clock was read.
    bool present=false,uncertain=false;
    uint64_t revision=0;
    Clock clock{};
};
struct Message {
    Kind kind=Kind::Request;
    Intent intent=Intent::Pause;
    Result result=Result::None;
    uint64_t epoch=0;
    uint8_t peer=0;
    uint64_t sequence=0,observed_revision=0;
    Snapshot state{};
    Code code=Code::None;
};
struct Report {
    Disposition disposition=Disposition::Rejected;
    Code code=Code::None;
    bool effect_confirmed=false;
    uint64_t sequence=0,revision=0;
    std::string cause;
    bool okay() const {return code==Code::None;}
};
struct Outbound {uint8_t target=0;Message message{};}; // target0 broadcasts to session peers.
struct PresentationReadback {
    uint64_t revision=0;
    Clock clock{};
    float native_clock_scale=0;
    bool items_confirmed=false;
};
struct Config {Role role=Role::Client;uint64_t epoch=0;uint8_t local_peer=0,peer_mask=0;uint32_t game_thread=0;};
struct Callbacks {
    void* user=nullptr;
    uint32_t (*thread_id)(void*)=nullptr;
    bool (*read_host_clock)(void*,uint64_t epoch,Clock&,std::string&)=nullptr;
    // RejectedNoEffect is valid only when the adapter failed before any store.
    Effect (*set_host_clock)(void*,uint64_t epoch,float target,std::string&)=nullptr;
    Effect (*apply_client_presentation)(void*,uint64_t epoch,const Snapshot&,PresentationReadback&,std::string&)=nullptr;
};
struct Status {
    bool started=false,write_locked=false;
    Snapshot observed{},applied{};
    uint64_t attempted=0,confirmed=0,failed=0,duplicates=0,published=0;
    std::array<uint64_t,4> processed_sequences{},presentation_acks{};
    Code last_code=Code::None;
    std::string last_cause;
};
bool encode(const Message&,std::array<uint8_t,wire_size>&,std::string& error);
bool decode(const uint8_t*,size_t,Message&,std::string& error);
class Controller {
public:
    Controller()=default;
    Controller(const Controller&)=delete;
    Controller& operator=(const Controller&)=delete;
    Report start(const Config&,const Callbacks&);
    Report stop();
    Report submit(Intent);
    Report receive(uint8_t authenticated_sender,const Message&);
    Report poll_host();
    // The caller retains a popped packet until the transport accepts it.
    Report pop(Outbound&,bool& available);
    Report status(Status&) const;
private:
    struct Cached {bool used=false;Message request{},reply{};std::string cause;};
    Config config_{};Callbacks callbacks_{};Status status_{};
    bool busy_=false;uint64_t local_sequence_=0,last_epoch_=0;
    std::array<std::array<Cached,history_size>,4> receipts_{};
    std::array<std::array<Cached,history_size>,4> presentation_receipts_{};
    std::array<Cached,history_size> pending_{},client_receipts_{},presentation_history_{};
    std::array<Snapshot,history_size> host_history_{};
    std::array<Outbound,outbox_size> outbox_{};size_t head_=0,queued_=0;
    Report guard() const;
    Report error(Code,const std::string&,Disposition=Disposition::Rejected);
    bool room(size_t count) const;
    void emit(uint8_t target,const Message&);
    bool read_clock(Clock&,Code&,std::string&);
    bool publish(Clock,Code&,std::string&);
    Report process_request(const Message&);
    Report process_client(const Message&);
    Report apply_state(const Snapshot&);
};
}
