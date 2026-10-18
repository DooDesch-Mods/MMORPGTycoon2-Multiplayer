#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace mt2mp::game::live_host_state {
class FrameApplyAcks {
public:
    using Clock=std::chrono::steady_clock;
    struct Peer {uint64_t revision=0;std::string rows;std::optional<Clock::time_point> waiting_since;};
    static constexpr auto timeout=std::chrono::seconds(30);
    static constexpr auto cooldown=std::chrono::milliseconds(250);
private:
    std::array<Peer,4> peers_{};
    uint8_t mask_=0;
    uint64_t required_=0,observations_=0,recoveries_=0;
    std::optional<Clock::time_point> last_progress_;
public:
    void clear(){*this=FrameApplyAcks{};}
    uint64_t required()const{return required_;}
    uint64_t observations()const{return observations_;}
    uint64_t recoveries()const{return recoveries_;}
    const auto& peers()const{return peers_;}
    uint8_t pending()const{uint8_t result=0;for(size_t i=0;i<4;++i)if((mask_&(1u<<i))&&peers_[i].revision<required_)result|=uint8_t(1u<<i);return result;}
    bool cooling(Clock::time_point now)const{return !pending()&&last_progress_&&now-*last_progress_<cooldown;}
    void sent(uint64_t revision,uint8_t mask,Clock::time_point now){
        if(!revision||revision<=required_||(mask&0xf0)||(required_&&mask!=mask_))throw std::runtime_error("live Frame ACK sent revision/peer set differs");
        required_=revision;mask_=mask;
        for(size_t i=0;i<4;++i)if((mask_&(1u<<i))&&!peers_[i].waiting_since)peers_[i].waiting_since=now;
    }
    bool validate(uint64_t peer,uint64_t revision,const std::string& rows,std::string& e)const{
        if(peer<1||peer>4||!(mask_&(1u<<(peer-1)))||!revision||revision>required_){e="live applied Frame ACK peer/revision was not sent";return false;}
        const auto& old=peers_[size_t(peer-1)];
        if(revision==old.revision&&rows!=old.rows){e="live duplicate applied Frame ACK rows conflict";return false;}
        return true;
    }
    void acknowledge(uint64_t peer,uint64_t revision,std::string rows,Clock::time_point now){
        auto& row=peers_[size_t(peer-1)];if(revision<=row.revision)return;
        const bool was_pending=pending()!=0;
        row.revision=revision;row.rows=std::move(rows);last_progress_=now;
        if(revision==required_)row.waiting_since.reset();else row.waiting_since=now;
        if(was_pending&&!pending()&&recoveries_!=UINT64_MAX)++recoveries_;
    }
    bool timely(Clock::time_point now,std::string& e)const{
        for(size_t i=0;i<4;++i)if((pending()&(1u<<i))&&peers_[i].waiting_since&&now-*peers_[i].waiting_since>=timeout){
            e="live Client applied Frame ACK timeout after 30 seconds peer="+std::to_string(i+1)+" applied="+std::to_string(peers_[i].revision)+" required="+std::to_string(required_)+"; no retry";return false;}
        return true;
    }
    bool ready(Clock::time_point now,bool& waiting,std::string& e){
        waiting=false;if(!timely(now,e))return false;
        if(pending())e="info: Host publication waits for Client full Frame ACK peers="+std::to_string(pending())+" required="+std::to_string(required_);
        else if(cooling(now))e="info: Host publication waits for 250ms after Client full Frame ACK";
        else {e.clear();return true;}
        waiting=true;if(observations_!=UINT64_MAX)++observations_;return true;
    }
};
}
