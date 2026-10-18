#include "marketing_authority/state.h"
#include "marketing_authority/records.h"
#include "marketing_authority/clock_guard.h"
#include "marketing_authority/capture_guard.h"
#include "live_host_state/day_rollover/economy/codec.h"
#include <iostream>
#include <stdexcept>
namespace m=mt2mp::game::marketing_authority;
namespace live=mt2mp::game::live_host_state;
namespace economy=live::day::economy;
namespace {
size_t checks=0;
void need(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
m::Json award(int32_t day){return {{"name","{award_moststable_title}"},{"source","News Source XYZ"},{"day",day},{"impressions",15},{"conversions",14},{"isGood",true}};}
m::Json campaign(int32_t type,int32_t start,int32_t duration){return {{"customName",""},{"type",type},{"startedOn",start},{"duration",duration},{"impressions",0},{"conversions",0},{"hype",0},{"active",1}};}
m::State initial(){return {{{"awards",m::Json::array()},{"campaigns",m::Json::array({campaign(6,1,3)})},{"positiveBuzz",6},{"negativeBuzz",0}}};}
economy::State fixture(const m::State& value){economy::State s;auto& f=s.day.frame;f.session=41;f.epoch=7;f.source_generation=9;f.revision=3;
    f.snapshot="11111111-1111-4111-8111-111111111111";f.selected=1;f.pause=1;f.values.calendar={1,1,3,2,11819,7};f.values.cash=82280;f.values.record={2,82280,0,0,0};f.marketing=value;
    s.day.history={{2,live::day::record(f.values)}};return s;
}
}
int main(){try{
    std::string e;m::Plan plan;auto before=initial(),target=before;
    target.values["awards"].push_back(award(2));target.values["campaigns"].push_back(campaign(4,2,5));
    need(m::plan(before,target,2,plan,e)&&plan.add_awards==std::vector<size_t>{0}&&plan.add_campaigns==std::vector<size_t>{1},"missing measured rows: "+e);
    need(m::plan(target,target,2,plan,e)&&!plan.changed(),"unchanged rows replay work");
    auto changed=target;changed.values["awards"][0]["impressions"]=29;changed.values["campaigns"][0]["conversions"]=5;changed.values["campaigns"][0]["customName"]="Launch copy";changed.values["negativeBuzz"]=3;
    need(m::plan(target,changed,2,plan,e)&&plan.update_awards==std::vector<size_t>{0}&&plan.update_campaigns==std::vector<size_t>{0}&&plan.buzz&&plan.add_awards.empty()&&plan.abort_campaigns.empty(),"updates replaced native owner: "+e);
    auto expired=initial();expired.values["awards"].push_back(award(0));auto empty=initial();empty.values["campaigns"]=m::Json::array();
    need(m::plan(expired,empty,7,plan,e)&&plan.purge_awards&&plan.abort_campaigns==std::vector<int32_t>{6},"expiry day/purge plan: "+e);
    need(!m::plan(target,before,2,plan,e)&&e.find("nonexpired")!=std::string::npos,"nonexpired Award silently removed");
    auto reorder=target;std::swap(reorder.values["campaigns"][0],reorder.values["campaigns"][1]);need(!m::plan(target,reorder,2,plan,e),"retained owner order silently replaced");
    for(int bad=0;bad<10;++bad){auto s=target;switch(bad){case 0:s.values["extra"]=0;break;case 1:s.values["positiveBuzz"]=uint64_t(UINT32_MAX);break;case 2:s.values["awards"][0]["isGood"]=1;break;case 3:s.values["awards"].push_back(s.values["awards"][0]);break;case 4:s.values["campaigns"][0]["type"]=9;break;case 5:s.values["campaigns"][0]["startedOn"]=3;break;case 6:s.values["campaigns"][0]["duration"]=0;break;case 7:s.values["campaigns"][0]["active"]=0;break;case 8:s.values["campaigns"][0]["customName"]=std::string("a\0b",3);break;case 9:s.values["campaigns"][0]["duration"]=INT32_MAX;break;}need(!m::valid(s,2,e),"malformed state accepted "+std::to_string(bad));}
    for(int32_t type=0;type<9;++type){auto row=campaign(type,2,5);const auto record=m::records(row,false);need(record.size()==8&&record[2].tokens[0].text==m::campaign_names[size_t(type)],"native Campaign enum Record mismatch");}
    const auto a=m::records(award(2),true);need(a.size()==7&&a[6].tokens[0].text=="true"&&a[4].tokens[0].bits==15,"Award typed Record shape");
    auto unequal=a;unequal[4].tokens[0].bits=16;need(m::same_records(a,a)&&!m::same_records(a,unequal),"original Save comparison omitted counters");
    m::ClockSample run{2,11819,9,1,false},held{2,11819,9,0,true};need(m::clock_unchanged(run,held,true,e)&&!m::clock_unchanged(run,held,false,e),"Host SaveScope pause semantics");
    for(int field=0;field<5;++field){auto drift=held;if(field==0)++drift.day;if(field==1)++drift.seconds;if(field==2)drift.selected=3;if(field==3)drift.pause=1;if(field==4)drift.paused=false;need(!m::clock_unchanged(run,drift,true,e)&&e.find("measured=")!=std::string::npos,"held Clock drift lacked exact cause");}
    auto state=fixture(target);std::vector<uint8_t> bytes;economy::State decoded;
    mt2mp::game::native::SimulationState actual;
    actual.cash=82280;actual.record_present=true;actual.record_day=2;actual.record_cash=82280;
    actual.clock={1,1,2,11819,false};
    need(m::host_frame_matches(state.day.frame,actual,e),"read-only Host finance/Clock match: "+e);
    for(int field=0;field<13;++field){auto drift=actual;switch(field){
        case 0:++drift.cash;break;case 1:drift.record_present=false;break;case 2:++drift.record_day;break;case 3:++drift.record_cash;break;
        case 4:++drift.record_income;break;case 5:++drift.record_expense;break;case 6:++drift.line_counts[0];break;case 7:++drift.line_income[19];break;
        case 8:++drift.clock.flat_day;break;case 9:++drift.clock.seconds;break;case 10:drift.clock.time_scale=9;break;case 11:drift.clock.pause_scale=0;break;case 12:drift.clock.paused=true;break;}
        need(!m::host_frame_matches(state.day.frame,drift,e)&&e.find("required")!=std::string::npos&&e.find("measured")!=std::string::npos,"mixed Host Frame accepted or missing evidence field="+std::to_string(field));
    }
    mt2mp::game::native::SoftwareState software;software.rating=421.6900634765625f;software.fun=128.53921508789062f;
    const auto stable=m::finance_fields(actual,software);
    need(m::guard_equal("held","finance/Software",stable,stable,e),"unchanged finance rejected");
    for(const auto name:{"cash","record_present","record_day","record_cash","record_income","record_expense","line_counts","line_income","logged_in_subscribers","rating_bits","fun_bits"}){
        auto drift=stable;if(drift[name].is_array())drift[name][0]=1;else if(drift[name].is_boolean())drift[name]=false;else drift[name]=drift[name].get<int64_t>()+1;
        need(!m::guard_equal("held","finance/Software",stable,drift,e)&&e.find(name)!=std::string::npos,"guard omitted exact changed field "+std::string(name));
    }
    need(!m::guard_equal("held","owners",m::Json{{"marketing",uintptr_t(10)},{"world",uintptr_t(20)}},m::Json{{"marketing",uintptr_t(11)},{"world",uintptr_t(20)}},e)&&e.find("marketing")!=std::string::npos,"changed owner hidden by finance diagnosis");
    need(economy::encode(state,bytes,e)&&economy::decode(bytes.data(),bytes.size(),decoded,e)&&decoded==state,"complete Economy/Day/Frame15 roundtrip: "+e);
    auto raw=m::Json::parse(bytes);need(raw["day"]["frame"]["schema"]==15,"Marketing did not require15");
    state.day.frame.vips=live::vip::Batch{0,{{1,1,158,"Kitten Purple",true},{2,1,158,"Kitten Purple",false,std::string("{msg_streamer_disconnected}")}}};
    need(economy::encode(state,bytes,e)&&economy::decode(bytes.data(),bytes.size(),decoded,e)&&decoded==state,"Frame15 with original VIP message: "+e);
    state.day.frame.marketing.reset();need(economy::encode(state,bytes,e)&&economy::decode(bytes.data(),bytes.size(),decoded,e)&&decoded==state,"legacy14 roundtrip");
    state.day.frame.vips->rows.back().message.reset();need(economy::encode(state,bytes,e)&&economy::decode(bytes.data(),bytes.size(),decoded,e)&&decoded==state,"legacy13 roundtrip");
    std::cout<<"PASS marketing_frame "<<checks<<" checks\n";return 0;
}catch(const std::exception& x){std::cerr<<"FAIL "<<x.what()<<"\n";return 1;}}
