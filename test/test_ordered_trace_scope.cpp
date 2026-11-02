#define MT2MP_FRAMEWORK_PLUGIN
#include "mt2mp/core/game_update_pair.h"
#include <cassert>
#include <stdexcept>
#include <vector>

namespace mt2mp::game_update_pair {
struct Event {Section section;bool begin;uint64_t token;};
std::vector<Event> events;
bool capturing=true;
uint64_t serial=0;
TraceToken begin_scope(Section section) noexcept {
    if(!capturing)return {};
    const auto token=++serial;events.push_back({section,true,token});return {7,token};
}
void end_scope(Section section,TraceToken token) noexcept {
    assert(token.epoch==7);events.push_back({section,false,token.token});
}
}

int main(){
    using namespace mt2mp::game_update_pair;
    static_assert(sizeof(section_names)/sizeof(*section_names)==size_t(Section::Count));
    {
        Scope service(Section::OrderedService);
        {Scope transport(Section::OrderedTransport);}
        try{Scope native(Section::OrderedNativeStep);throw std::runtime_error("native failure");}
        catch(const std::runtime_error&){}
    }
    assert(events.size()==6);
    assert(events[0].section==Section::OrderedService&&events[0].begin);
    assert(events[1].section==Section::OrderedTransport&&events[1].begin);
    assert(!events[2].begin&&events[2].token==events[1].token);
    assert(events[3].section==Section::OrderedNativeStep&&events[3].begin);
    assert(!events[4].begin&&events[4].token==events[3].token);
    assert(!events[5].begin&&events[5].token==events[0].token);
    capturing=false;
    {Scope dormant(Section::OrderedService);}
    assert(events.size()==6);
}
