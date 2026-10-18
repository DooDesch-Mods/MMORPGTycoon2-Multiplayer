#include "mt2mp/net/snapshot_sync.h"
#include <iostream>

int main() {
    using namespace mt2mp::net;
    using namespace std::chrono_literals;
    using json=nlohmann::json;
    json checks=json::array();
    auto require=[&](bool value,const char* cause) { if(!value) throw std::runtime_error(cause); checks.push_back(cause); };
    try {
        const auto start=SnapshotLoadWatch::Clock::time_point{};
        SnapshotLoadWatch watch;
        require(snapshot_data_idle_timeout_seconds==60&&snapshot_native_load_timeout_seconds==900,"data and native deadlines distinct");
        require(!watch.expired(start+1000s)&&watch.elapsed_ms(start)==0,"inactive watch does not expire");
        watch.begin(start);
        require(watch.observe(2,3,0,start),"first native mode observation retained");
        require(!watch.observe(2,3,0,start+65s)&&watch.progress_events==1,"heartbeat does not count as load progress");
        require(!watch.expired(start+65s)&&watch.elapsed_ms(start+65s)==65000,"native load may exceed data idle deadline");
        require(watch.progress_idle_ms(start+65s)==65000,"native idle remains visible without false timeout");
        require(watch.observe(4,3,0,start+70s),"native mode transition counts as progress");
        require(watch.observe(4,3,100,start+80s)&&watch.registrations==100,"registration increase counts as progress");
        require(!watch.observe(4,3,100,start+800s),"unchanged late observation does not reset progress");
        require(!watch.expired(start+899s)&&watch.expired(start+900s),"absolute deadline does not move with progress");
        require(watch.observe(4,3,1000,start+901s)&&watch.expired(start+901s),"late genuine progress does not revive expired load");
        require(watch.timeout_cause(start+901s).find("elapsed_ms=901000 required<900s")!=std::string::npos,"timeout exposes measured and required duration");
        bool regressed=false;
        try { watch.observe(4,3,999,start+902s); }
        catch(const std::runtime_error& error) { regressed=std::string(error.what()).find("actual=999 required>=1000")!=std::string::npos; }
        require(regressed,"registration regression rejected with cause");
        watch.begin(start+1000s);
        require(watch.registrations==0&&watch.progress_events==0&&!watch.completed,"new explicit load resets observation");
        watch.observe(3,3,42,start+1010s); watch.finish(start+1020s);
        require(!watch.expired(start+2000s)&&watch.elapsed_ms(start+2000s)==20000&&watch.progress_idle_ms(start+2000s)==10000,"confirmed completion freezes observation duration");
        std::cout<<json{{"passed",true},{"scope","virtual monotonic native-load deadline, no game"},{"check_count",checks.size()},{"checks",checks}}.dump()<<std::endl;
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"error: load deadline fixture cause="<<error.what()<<std::endl; return 1;
    }
}
