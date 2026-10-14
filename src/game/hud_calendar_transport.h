#pragma once
#include "hud_calendar.h"
#include <array>
#include <functional>
#include <memory>
namespace mt2mp::game::hud_time::transport {
inline constexpr uint8_t channel=10;
inline constexpr size_t packet_size=56;
using Packet=std::array<uint8_t,packet_size>;
bool encode(const CalendarSample&,Packet&,std::string&);
bool decode(const uint8_t*,size_t,CalendarSample&,std::string&);
struct Context {
    bool connected=false,active=false,client=false,calendar_bound=false;
    uint8_t local_peer=0,peer_mask=0;
    uint64_t epoch=0,time_revision=0,calendar_epoch=0;
    bool operator==(const Context&) const=default;
};
using Handler=std::function<void(uint64_t,const uint8_t*,uint32_t)>;
struct Services {
    std::function<uint32_t()> thread_id;
    std::function<bool(Context&,std::string&)> context;
    std::function<bool(CalendarSample&,std::string&)> probe;
    std::function<bool(uint64_t,const CalendarSample&,std::string&)> apply;
    std::function<bool(const std::string&,std::string&)> suspend_calendar;
    std::function<bool(uint8_t,const uint8_t*,uint32_t,uint64_t,std::string&)> send;
    std::function<void(uint8_t,Handler)> register_handler;
};
struct Status {
    bool initialized=false,bound=false,pending=false,locked=false,suspend_pending=false;
    uint64_t epoch=0,retired_epoch=0,received=0,duplicates=0,superseded=0,rejected=0,
        probed=0,send_attempted=0,send_accepted=0,apply_attempted=0,apply_confirmed=0,
        applied_revision=0,last_sent_revision=0,future_waits=0,failures=0,unbound_skipped=0;
    std::string error,waiting;
};
class Adapter {
public:
    Adapter();
    ~Adapter();
    Adapter(const Adapter&)=delete;
    Adapter& operator=(const Adapter&)=delete;
    bool initialize(uint32_t game_thread,Services,std::string&);
    bool bind(std::string&);
    bool tick(uint64_t monotonic_milliseconds,std::string&);
    bool enqueue(uint64_t authenticated_sender,const uint8_t*,size_t,std::string&);
    bool suspend(const std::string&,std::string&);
    Status status() const;
private:
    struct Impl;
    std::shared_ptr<Impl> p_;
};
}
