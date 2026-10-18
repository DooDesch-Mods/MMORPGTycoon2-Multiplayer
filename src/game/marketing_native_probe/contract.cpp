#include "contract.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <set>

namespace mt2mp::game::marketing_native_probe {
namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
Json tok(int32_t type,uint32_t bits,const std::string& text){return {{"type",type},{"bits",bits},{"text",text}};}
Json records(const char* root,const std::vector<std::pair<std::string,Json>>& fields){
    Json rows=Json::array({{{"parent",nullptr},{"child_index",0},{"label",tok(1,0,root)},{"tokens",Json::array()}}});
    for(size_t i=0;i<fields.size();++i)rows.push_back({{"parent",0},{"child_index",i},{"label",tok(1,0,fields[i].first)},{"tokens",Json::array({fields[i].second})}});
    return rows;
}
Json award_records(){return records("mmoAward",{{"name",tok(2,0,"{award_moststable_title}")},{"source",tok(2,0,"News Source XYZ")},
    {"day",tok(4,2,"")},{"impressions",tok(4,15,"")},{"conversions",tok(4,14,"")},{"isGood",tok(2,0,"true")}});}
Json campaign_records(bool positive){return records("mmoAdCampaign",{{"customName",tok(2,0,"")},{"type",tok(2,0,positive?"PositiveStreamer":"Launch")},
    {"startedOn",tok(4,positive?2u:1u,"")},{"duration",tok(4,positive?5u:3u,"")},{"impressions",tok(4,0,"")},{"conversions",tok(4,0,"")},{"hype",tok(4,0,"")}});}
Records decode(const Json& rows){
    Records out;
    for(const auto& j:rows){mt2mp::subscriber_lifecycle::Record r;
        r.parent=j.at("parent").is_null()?SIZE_MAX:j.at("parent").get<size_t>();r.child_index=j.at("child_index").get<size_t>();
        const auto t=[](const Json& v){return mt2mp::subscriber_lifecycle::Token{v.at("type").get<int32_t>(),v.at("bits").get<uint32_t>(),v.at("text").get<std::string>()};};
        r.label=t(j.at("label"));for(const auto& v:j.at("tokens"))r.tokens.push_back(t(v));out.push_back(std::move(r));
    }return out;
}
template<class T>bool at(Memory& m,uintptr_t p,size_t offset,T& out,std::string& e){
    if(!p||p>UINTPTR_MAX-offset||p+offset>UINTPTR_MAX-sizeof out)return fail(e,"Marketing read extent invalid");
    return m.read(p+offset,&out,sizeof out,e);
}
bool ap(Memory& m,uintptr_t image,uintptr_t p,uintptr_t rva,std::string& e){uintptr_t v=0;return at(m,p,0,v,e)&&(v==image+rva||fail(e,"Marketing typed addresspoint differs"));}
bool array(Memory& m,uintptr_t p,size_t offset,std::vector<uintptr_t>& out,std::string& e){
    uintptr_t data=0;int32_t count=-1,capacity=-1;
    if(!at(m,p,offset,data,e)||!at(m,p,offset+8,count,e)||!at(m,p,offset+12,capacity,e))return false;
    if(count<0||count>8||capacity<count||capacity>512||(count&&!data)||static_cast<size_t>(count)>(UINTPTR_MAX-data)/8)return fail(e,"Marketing proof array bounds differ");
    std::set<uintptr_t> unique;out.clear();
    for(int32_t i=0;i<count;++i){uintptr_t value=0;if(!at(m,data,static_cast<size_t>(i)*8,value,e))return false;
        if(!value||!unique.insert(value).second)return fail(e,"Marketing proof array null/duplicate");out.push_back(value);}
    return true;
}
bool locstring(Memory& m,uintptr_t p,std::string& out,std::string& e){
    uintptr_t data=0,begin=0,end=0,cap=0;uint64_t length=0;
    if(!at(m,p,0,data,e)||!at(m,p,8,length,e)||!at(m,p,0x20,begin,e)||!at(m,p,0x28,end,e)||!at(m,p,0x30,cap,e))return false;
    if(!data||length>256||data>UINTPTR_MAX-length||begin!=end||cap<end)return fail(e,"Marketing proof LocString text/arguments unsupported");
    out.resize(static_cast<size_t>(length));if(length&&!m.read(data,out.data(),out.size(),e))return false;
    char last=1;if(!at(m,data,static_cast<size_t>(length),last,e))return false;
    return !last||fail(e,"Marketing LocString terminator differs");
}
Json campaign_value(bool positive){return {{"customName",""},{"type",positive?4:6},{"startedOn",positive?2:1},{"duration",positive?5:3},
    {"impressions",0},{"conversions",0},{"hype",0},{"active",1}};}
bool panes_match(const Snapshot& s,std::string& e){
    if(s.views!=s.panes||s.bindings.size()!=s.awards.size()+s.campaigns.size())return fail(e,"Marketing current Boost pane count/membership differs");
    std::set<uintptr_t> awards,campaigns;
    for(const auto& b:s.bindings){const auto a=b.at("award").get<uintptr_t>(),c=b.at("campaign").get<uintptr_t>();
        if(bool(a)==bool(c))return fail(e,"BoostView must bind exactly one current owner");
        if(a&&(std::find(s.awards.begin(),s.awards.end(),a)==s.awards.end()||!awards.insert(a).second))return fail(e,"Boost award binding stale/duplicate");
        if(c&&(std::find(s.campaigns.begin(),s.campaigns.end(),c)==s.campaigns.end()||!campaigns.insert(c).second))return fail(e,"Boost campaign binding stale/duplicate");
    }
    return awards.size()==s.awards.size()&&campaigns.size()==s.campaigns.size()||fail(e,"Boost current owner missing a pane");
}
}
bool parse_payload(const Json& j,Payload& out,std::string& e){
    out={};
    const Json expected={{"schema",1},{"source_sha256",source_sha},{"baseline_sha256",baseline_sha},{"day",2},
        {"award",award_records()},{"campaign",campaign_records(true)},{"launch",campaign_records(false)}};
    if(j!=expected)return fail(e,"Marketing proof artifact differs from the two retained typed Records");
    out={decode(j.at("award")),decode(j.at("campaign")),decode(j.at("launch"))};e.clear();return true;
}
bool current_campaign(int32_t day,int32_t start,int32_t duration,uint8_t active,std::string& e){
    const auto end=int64_t(start)+int64_t(duration);
    if(duration<=0||end>INT32_MAX||end<INT32_MIN||active!=1||start>day||int64_t(day)>=end)return fail(e,"Campaign Resolve current-day branch guard failed");
    return true;
}
bool same_clock(const ClockSample& before,const ClockSample& actual,ClockPhase phase,Json& diagnostic,std::string& e){
    const char* name=phase==ClockPhase::Before?"before":phase==ClockPhase::Held?"held":"closed";
    const auto values=[](const ClockSample& c){return Json{{"day",c.day},{"seconds",c.seconds},{"time_scale",c.time_scale},
        {"pause_scale",c.pause_scale},{"paused",c.paused},{"time_scale_bits",std::bit_cast<uint32_t>(c.time_scale)},{"pause_scale_bits",std::bit_cast<uint32_t>(c.pause_scale)}};};
    const float required_pause=phase==ClockPhase::Before?before.pause_scale:0.f;
    diagnostic={{"phase",name},{"before",values(before)},{"measured",values(actual)},
        {"required",{{"day",2},{"seconds",before.seconds},{"time_scale",0.f},{"pause_scale",required_pause},{"paused",true}}},{"confirmed",false}};
    const auto reject=[&](const char* field,const Json& measured,const Json& required){
        diagnostic["failed_field"]=field;e="Marketing Clock "+std::string(field)+" differs phase="+name+" measured="+measured.dump()+" required="+required.dump();return false;
    };
    if(phase!=ClockPhase::Before&&phase!=ClockPhase::Held&&phase!=ClockPhase::Closed)return reject("phase",int(phase),"before/held/closed");
    if(before.day!=2)return reject("baseline.day",before.day,2);
    if(before.time_scale!=0||!before.paused||!std::isfinite(before.pause_scale))return reject("baseline.paused_clock",values(before),"finite pause scale, time_scale=0, paused=true");
    if(actual.day!=before.day)return reject("day",actual.day,before.day);
    if(actual.seconds!=before.seconds)return reject("seconds",actual.seconds,before.seconds);
    if(actual.time_scale!=0||std::bit_cast<uint32_t>(actual.time_scale)!=std::bit_cast<uint32_t>(before.time_scale))return reject("time_scale_bits",std::bit_cast<uint32_t>(actual.time_scale),std::bit_cast<uint32_t>(before.time_scale));
    if(!actual.paused)return reject("paused",actual.paused,true);
    // Native SetPauseTimeScale compares floats before writing. Either signed zero is unchanged zero.
    if(!std::isfinite(actual.pause_scale)||(phase==ClockPhase::Before?
       std::bit_cast<uint32_t>(actual.pause_scale)!=std::bit_cast<uint32_t>(before.pause_scale):actual.pause_scale!=0))return reject("pause_scale",actual.pause_scale,required_pause);
    diagnostic["confirmed"]=true;e.clear();return true;
}
Json expected_values(bool inserted){
    Json campaigns=Json::array({campaign_value(false)}),awards=Json::array();
    if(inserted){campaigns.push_back(campaign_value(true));awards.push_back({{"name","{award_moststable_title}"},{"source","News Source XYZ"},
        {"day",2},{"impressions",15},{"conversions",14},{"isGood",true}});}
    return {{"awards",awards},{"campaigns",campaigns},{"positiveBuzz",6},{"negativeBuzz",0}};
}
bool read_snapshot(Memory& m,uintptr_t image,uintptr_t state,uintptr_t mode,Snapshot& s,std::string& e){
    s={};uintptr_t singleton=0;
    if(!at(m,state,0x1d0,s.marketing,e)||!at(m,image,0x13df2c0,singleton,e)||singleton!=s.marketing||!ap(m,image,s.marketing,0x16dfa60,e))return fail(e,e.empty()?"Marketing GameState owner differs":e.c_str());
    if(!at(m,mode,0x128,s.hud,e)||!at(m,image,0x13df8b0,singleton,e)||singleton!=s.hud||!ap(m,image,s.hud,0x1742750,e))return fail(e,e.empty()?"Marketing HUD owner differs":e.c_str());
    if(!at(m,s.hud,0xb8,s.window,e)||!ap(m,image,s.window,0x16fa4d0,e)||!ap(m,image,s.window+0x508,0x16e1770,e)||
       !at(m,image,0x13df9a0,s.clock,e)||!s.clock)return fail(e,e.empty()?"Marketing current Window/Clock missing":e.c_str());
    if(!array(m,s.marketing,0x40,s.awards,e)||!array(m,s.marketing,0x70,s.campaigns,e)||!array(m,s.window,0x510,s.views,e)||!array(m,s.window,0x300,s.panes,e)||
       !at(m,s.window,0x138,s.width,e)||!at(m,s.window,0x13c,s.height,e))return false;
    s.values={{"awards",Json::array()},{"campaigns",Json::array()}};s.bindings=Json::array();
    int32_t positive=0,negative=0;if(!at(m,s.marketing,0x98,positive,e)||!at(m,s.marketing,0x9c,negative,e))return false;
    s.values["positiveBuzz"]=positive;s.values["negativeBuzz"]=negative;
    for(auto p:s.awards){std::string name,source;int32_t day=0,impressions=0,conversions=0;uint8_t good=2;
        if(!ap(m,image,p,0x1749880,e)||!locstring(m,p+0x28,name,e)||!locstring(m,p+0x60,source,e)||!at(m,p,0x98,day,e)||
           !at(m,p,0x9c,impressions,e)||!at(m,p,0xa0,conversions,e)||!at(m,p,0xa4,good,e)||good>1)return fail(e,e.empty()?"Marketing Award bool invalid":e.c_str());
        s.values["awards"].push_back({{"name",name},{"source",source},{"day",day},{"impressions",impressions},{"conversions",conversions},{"isGood",good!=0}});
    }
    for(auto p:s.campaigns){std::string name;std::array<int32_t,6> v{};uint8_t active=2;
        if(!ap(m,image,p,0x16e3d50,e)||!locstring(m,p+0x18,name,e)||!m.read(p+0x50,v.data(),sizeof v,e)||!at(m,p,0x68,active,e)||active>1)return fail(e,e.empty()?"Marketing Campaign active bool invalid":e.c_str());
        s.values["campaigns"].push_back({{"customName",name},{"type",v[0]},{"startedOn",v[1]},{"duration",v[2]},
            {"impressions",v[3]},{"conversions",v[4]},{"hype",v[5]},{"active",active}});
    }
    for(auto p:s.views){uintptr_t a=0,c=0,parent=0,window=0;
        if(!ap(m,image,p,0x16dd8a0,e)||!at(m,p,0x320,a,e)||!at(m,p,0x328,c,e)||!at(m,p,0x278,parent,e)||!at(m,p,0x280,window,e))return false;
        s.bindings.push_back({{"view",p},{"award",a},{"campaign",c},{"parent",parent},{"window",window}});
    }e.clear();return true;
}
Json report(const Snapshot& s){return {{"marketing",s.marketing},{"hud",s.hud},{"window",s.window},{"clock",s.clock},{"award_owners",s.awards},
    {"campaign_owners",s.campaigns},{"views",s.views},{"panes",s.panes},{"bindings",s.bindings},{"width_bits",s.width},{"height_bits",s.height},{"values",s.values}};}
bool baseline(const Snapshot& s,std::string& e){
    if(s.values!=expected_values(false)||!s.awards.empty()||s.campaigns.size()!=1||s.width!=std::bit_cast<uint32_t>(400.f)||s.height!=std::bit_cast<uint32_t>(45.f))return fail(e,"Marketing native expected-before fixture differs");
    return panes_match(s,e);
}
bool completed(const Snapshot& a,const Snapshot& b,uintptr_t award,uintptr_t campaign,std::string& e){
    if(b.marketing!=a.marketing||b.hud!=a.hud||b.window!=a.window||b.clock!=a.clock||b.width!=a.width||b.height!=a.height||
       b.values!=expected_values(true)||b.awards!=std::vector<uintptr_t>{award}||b.campaigns!=std::vector<uintptr_t>{a.campaigns.at(0),campaign}||!award||!campaign||campaign==a.campaigns[0])return fail(e,"Marketing native desired-after rows/owners differ");
    return panes_match(b,e);
}
}
