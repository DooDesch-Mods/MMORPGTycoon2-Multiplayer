#include "character_calendar_apply.h"
#include "subscriber_demand_state.h"
#include "character_membership_reader.h"
#include "subscriber_intervention.h"
#include "world_zone_runtime.h"
#include "live_host_state/state.h"
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
namespace {
namespace c=mt2mp::game::character_calendar;
int checks=0;
void expect(bool value,const char* name){++checks;if(!value)throw std::runtime_error(name);}
c::State initial(){
    c::State result;c::Definition definition;definition.kind=0;definition.id=0;definition.statistics=c::Statistics{};
    definition.statistics->durations[0]={3,{15,30}};definition.statistics->durations[1]={3,{90}};
    definition.demands[0]=c::Demand{"buff",{10}};definition.demands[1]=c::Demand{"nerf",{20}};
    result.definitions.push_back(definition);result.demand_order={c::demand_key(definition,1),c::demand_key(definition,0)};
    c::Zone zone;zone.key="zone:actual:1";zone.scene="root:67";zone.type=1;zone.constructed={0,3,3};
    zone.counts={12,2,1};zone.kills={std::vector<int32_t>{1,2},std::vector<int32_t>{0,1}};result.zones.push_back(zone);
    return result;
}
struct Port {
    c::State value;int calls=0,samples=0,fail_call=0;bool reject_owner=false,bad_readback=false;
    bool sample(c::State& out,std::string& error){++samples;out=value;if(bad_readback&&samples>1)++out.zones[0].counts[0];error.clear();return true;}
    bool preflight(const c::State&,std::string& error){if(reject_owner){error="retained native owner changed";return false;}return true;}
    bool effect(c::Receipt& receipt,std::string& error){++calls;receipt.attempted=true;++receipt.attempted_stores;
        if(calls==fail_call){error="original membership readback failed";return false;}++receipt.stores;return true;}
    bool statistics(size_t index,const c::Statistics& before,const c::Statistics& target,c::Receipt& receipt,std::string& error){
        if(value.definitions.at(index).statistics!=before)throw std::runtime_error("test statistics before differs");
        if(!effect(receipt,error))return false;value.definitions[index].statistics=target;return true;}
    bool demand(size_t definition,size_t index,const c::Demand& before,const c::Demand& target,c::Receipt& receipt,std::string& error){
        if(value.definitions.at(definition).demands.at(index)!=before)throw std::runtime_error("test demand before differs");
        if(!effect(receipt,error))return false;value.definitions[definition].demands[index]=target;return true;}
    bool zone(size_t index,const c::Zone& before,const c::Zone& target,c::Receipt& receipt,std::string& error){
        if(value.zones.at(index)!=before)throw std::runtime_error("test Zone before differs");
        if(!effect(receipt,error))return false;value.zones[index]=target;return true;}
};
void run(){std::string error;const auto baseline=initial();c::State decoded;
    expect(c::valid(baseline,error),"complete native catalogue and reversed actual DemandManager order valid");
    expect(c::decode(c::encode(baseline),decoded,error)&&decoded==baseline,"full leaf wire roundtrip retains all histories/ordered members");
    auto wire=c::encode(baseline);wire["definitions"][0]["id"]=true;
    expect(!c::decode(wire,decoded,error),"Boolean is not a native definition ID");
    wire=c::encode(baseline);wire["definitions"][0]["statistics"]["rating"][0][1]=uint32_t(0x7f800000);
    expect(!c::decode(wire,decoded,error),"nonfinite rating rejected even when unset");
    wire=c::encode(baseline);wire["definitions"][0]["statistics"]["optimal_dps"]=uint32_t(0x7fc00000);
    expect(!c::decode(wire,decoded,error)&&error.find("optimal DPS")!=std::string::npos,"nonfinite native DPS cause retained");
    wire=c::encode(baseline);wire["definitions"][0]["statistics"]["durations"][0][0]=1;
    expect(!c::decode(wire,decoded,error),"history count cannot exceed original limit");
    auto bad=baseline;bad.definitions[0].id=1;expect(!c::valid(bad,error),"global IDs cannot be renumbered to compact live rows");
    bad=baseline;bad.definitions[0].demands[1]->members={10};expect(!c::valid(bad,error),"Subscriber cannot join both embedded Demands");
    bad=baseline;bad.demand_order.pop_back();expect(!c::valid(bad,error),"complete original DemandManager membership required");
    bad=baseline;bad.demand_order[1]=bad.demand_order[0];expect(!c::valid(bad,error),"duplicate Manager member rejected");
    bad=baseline;bad.demand_order[1]="character:4:4/demand/buff";expect(!c::valid(bad,error),"foreign Demand owner rejected");
    bad=baseline;bad.zones[0].kills[0].resize(11);expect(!c::valid(bad,error),"original rolling history bounded to ten days");
    bad=baseline;bad.zones[0].counts[1]=-1;expect(!c::valid(bad,error),"negative native daily kill count rejected");
    bad=baseline;bad.definitions[0].deleted=true;bad.definitions[0].statistics.reset();
    expect(c::valid(bad,error),"registered embedded Demand survives native deleted definition tombstone");
    bad.definitions[0].statistics=c::Statistics{};expect(!c::valid(bad,error),"deleted native definition cannot expose freed statistics");
    auto target=baseline;target.definitions[0].statistics->tiers={2,3};target.definitions[0].statistics->tier_valid=true;
    target.definitions[0].statistics->durations[0].values={30,45,60};target.definitions[0].demands[0]->members={};
    target.definitions[0].demands[1]->members={10,20};target.zones[0].counts={13,0,0};target.zones[0].kills[0].push_back(3);
    Port port{baseline};c::Receipt receipt;
    expect(c::project(port,target,receipt,error)&&receipt.confirmed&&!receipt.unknown&&port.value==target,"Host FOTM/history/Demand/daily Zone values project together");
    expect(port.calls==4&&receipt.stores==4,"only changed native leaves execute");
    port=Port{target};expect(c::project(port,target,receipt,error)&&receipt.confirmed&&!receipt.attempted&&port.calls==0,"identical authoritative frame has no repeated effects");
    port=Port{baseline};port.reject_owner=true;expect(!c::project(port,target,receipt,error)&&!receipt.attempted&&!receipt.unknown&&error=="retained native owner changed","owner failure preserves original pre-effect cause");
    port=Port{baseline};auto foreign=target;foreign.zones[0].scene="dungeon:other";
    expect(!c::project(port,foreign,receipt,error)&&port.calls==0,"same key cannot change current Zone scene lifetime");
    port=Port{baseline};foreign=target;foreign.definitions[0].statistics->durations[0].limit=4;
    expect(!c::project(port,foreign,receipt,error)&&port.calls==0,"Host values cannot replace original IntegerHistory policy");
    port=Port{baseline};foreign=target;std::swap(foreign.demand_order[0],foreign.demand_order[1]);
    expect(!c::project(port,foreign,receipt,error)&&port.calls==0,"frame does not synthesize original Manager lifecycle reorder");
    port=Port{baseline};port.fail_call=2;
    expect(!c::project(port,target,receipt,error)&&receipt.attempted&&receipt.unknown&&!receipt.confirmed&&port.calls==2&&error=="original membership readback failed","partial original effect stops immediately and remains Unknown");
    expect(port.value.definitions[0].statistics==target.definitions[0].statistics&&port.value.zones==baseline.zones,"partial failure retains earlier effect without claiming later projection");
    port=Port{baseline};port.bad_readback=true;
    expect(!c::project(port,target,receipt,error)&&receipt.unknown&&!receipt.confirmed,"complete final readback mismatch cannot be confirmed");
    wire=c::encode(baseline);wire["ignored_runtime_pointer"]=123;
    expect(!c::decode(wire,decoded,error),"wire cannot smuggle extra native owner fields");
}
}
 #include "calendar_demand_capture_cases.inc"
int main(){try{run();calendar_demand_fixture::run();std::cout<<checks<<" Character calendar checks passed\n";return 0;}
catch(const std::exception& exception){std::cerr<<"Character calendar: "<<exception.what()<<'\n';return 1;}}
