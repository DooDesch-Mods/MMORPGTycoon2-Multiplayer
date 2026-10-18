#include "save_cache_refresh.h"
#include <iostream>
#include <stdexcept>
using Refresh=mt2mp::game::SaveCacheRefresh;
void require(bool condition,const char* cause){if(!condition)throw std::runtime_error(cause);}
int main(){
    try{
        Refresh value;const auto now=Refresh::Clock::time_point{};
        value.begin(now);
        require(value.tick(true,now)==Refresh::Action::Wait,"pre-existing scan was adopted");
        require(value.tick(false,now)==Refresh::Action::Queue,"fresh scan was not queued after previous scan");
        require(value.tick(true,now)==Refresh::Action::Wait,"queued task was not observed");
        require(value.tick(false,now)==Refresh::Action::Ready,"completed no-change scan was not accepted");
        require(value.tick(false,now)==Refresh::Action::Idle,"completed scan was replayed");
        value.begin(now);
        require(value.tick(false,now)==Refresh::Action::Queue,"idle refresh was not queued");
        require(value.tick(false,now)==Refresh::Action::Ready,"fast completed scan was not accepted");
        value.begin(now);value.cancel();
        require(value.tick(false,now)==Refresh::Action::Idle,"cancelled observer queued a task");
        value.begin(now);
        require(value.tick(true,now+std::chrono::seconds(120))==Refresh::Action::TimedOut,"stalled scan had no bound");
        require(value.tick(false,now)==Refresh::Action::Idle,"timed out scan was replayed");
        std::cout<<"PASS fresh scan, existing task, no-change completion, cancellation and timeout\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
