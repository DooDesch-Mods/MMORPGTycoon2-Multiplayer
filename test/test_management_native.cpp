#include <windows.h>
#include <intrin.h>
#include <array>
#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>
#include <map>
#include <set>
#include <optional>
#include <cstring>
#include <atomic>
#include "analytics_state.h"
#include "subscriber_demand_state.h"
#include "toon_thought_lifetime.h"
#include "toon_thought_state.h"
#include "live_host_state/state.h"
namespace {
struct Local {struct {uintptr_t business=0;}pin;uintptr_t release=0,software=0,current_release=0,lounge=0;std::vector<uintptr_t> gm_rows;};
#include "management_scalar_body.inc"
int assertions=0;void need(bool ok,const char* cause){++assertions;if(!ok)throw std::runtime_error(cause);}
}
#include "analytics_projection_cases.inc"
#include "subscriber_demand_cases.inc"
#include "toon_thought_lifetime_cases.inc"
#include "toon_thought_destroy_cases.inc"
#include "toon_thought_frame_cases.inc"
#include "gizmo_live_adopt_cases.inc"
#include "building_uptime_fence_cases.inc"
#include "management_primary_cases.inc"
int main(){try{uptime_fence_cases();
    management_primary_fixture::run();
    analytics_projection_fixture::run();
    demand_projection_fixture::run();
    thought_lifetime_fixture::run();
    thought_destroy_fixture::run();
    thought_frame_fixture::run();
    gizmo_live_adopt_fixture::run();
    Local local;local.pin.business=0x10000;local.release=0x20000;local.software=0x30000;local.current_release=0x40000;local.lounge=0x50000;local.gm_rows={0x60000,0x70000};
    need(scalar_address(local,0)==0x10050,"business owner");need(scalar_address(local,1)==0x200b8,"budget owner");
    need(scalar_address(local,24)==0x30344&&scalar_address(local,33)==0x3034d,"byte flag extent");
    need(scalar_address(local,34)==0x30088&&scalar_address(local,39)==0x40058,"version owner extent");
    need(scalar_address(local,40)==0x40060,"release type owner");need(scalar_address(local,44)==0x500d0,"Lounge count owner");
    need(scalar_address(local,45)==0x604c4&&scalar_address(local,46)==0x604c5&&scalar_address(local,47)==0x704c4,"GameMaster fixed-slot fields");
    need(!scalar_address(local,49),"out-of-range GameMaster pointer fabricated");local.current_release=0;need(!scalar_address(local,40),"absent Release fabricated");
    LONG value=7;bool stored=false;need(!scalar_cas(uintptr_t(&value),false,7,8,stored)&&stored&&value==8,"actual native scalar CAS");
    need(!scalar_cas(uintptr_t(&value),false,7,9,stored)&&!stored&&value==8,"concurrent scalar overwritten");
    std::array<uint8_t,4> bytes{0x61,0,0x72,0x73};need(!scalar_cas(uintptr_t(&bytes[1]),true,0,1,stored)&&stored&&bytes==std::array<uint8_t,4>{0x61,1,0x72,0x73},"byte CAS corrupted adjacent fields");
    need(!scalar_cas(uintptr_t(&bytes[1]),true,0,0,stored)&&!stored&&bytes[1]==1,"concurrent byte overwritten");
    need(scalar_cas(1,false,0,1,stored)!=0,"invalid native store lost SEH cause");
    std::cout<<assertions<<" actual management scalar owner/CAS assertions passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
