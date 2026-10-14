#pragma once
#include "typed_action/action_session.h"
#include <array>

namespace mt2mp::action_session {
// Slot IDs stay 1..4 so the existing Controller and ACK masks are unchanged.
class SteamTransport final:public Transport {
public:
    bool initialize(std::string&);
    bool claim(Handler,std::string&) override;
    bool send(uint64_t,const std::vector<uint8_t>&,std::string&) override;
    bool poll(std::string&);
private:
    void* interface_=nullptr;
    Handler handler_;
    std::array<uint64_t,5> slots_{};
    uint64_t lobby_=0,host_=0,local_=0;
    std::string failure_;
    bool membership(std::string&);
};
class SteamSession final:public Session {
public:
    bool current(Context&,std::string&) override;
};
}
