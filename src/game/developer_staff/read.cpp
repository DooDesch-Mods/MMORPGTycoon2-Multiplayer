#include "read.h"
#include <algorithm>
#include <cmath>
#include <climits>
#include <set>
namespace mt2mp::game::developer_staff {
namespace {
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
template<class T>bool at(Port& p,uintptr_t address,size_t offset,T& value,std::string& e){
    if(!address||address>UINTPTR_MAX-offset||address+offset>UINTPTR_MAX-sizeof(T))return fail(e,"Developer read address range invalid");
    return p.copy(address+offset,&value,sizeof(T),e);
}
bool type(Port& p,uintptr_t address,uintptr_t expected,std::string& e){uintptr_t actual=0;return at(p,address,0,actual,e)&&(actual==expected||fail(e,"Developer owned class addresspoint differs"));}
bool array(Port& p,uintptr_t owner,size_t offset,std::vector<uintptr_t>& rows,int32_t& capacity,std::string& e){
    uintptr_t data=0;int32_t count=0;
    if(!at(p,owner,offset,data,e)||!at(p,owner,offset+8,count,e)||!at(p,owner,offset+12,capacity,e))return false;
    if(count<0||count>50||capacity<count||capacity>4096||(capacity&&!data))return fail(e,"Developer owned array bounds differ");
    rows.clear();std::set<uintptr_t> unique;
    for(int32_t i=0;i<count;++i){uintptr_t value=0;if(!at(p,data,size_t(i)*8,value,e))return false;if(!value||!unique.insert(value).second)return fail(e,"Developer owned array contains null/duplicate receiver");rows.push_back(value);}
    return true;
}
bool observe(Port& p,const Pin& pin,uintptr_t image,State& out,Local& local,std::string& e){
    out={};local={};uintptr_t singleton=0;const auto state=reinterpret_cast<uintptr_t>(pin.origin.state);
    if(!at(p,state,0x180,local.lounge,e)||!at(p,state,0x1c0,local.business,e)||
       !type(p,local.lounge,image+0x16de6c0,e)||!type(p,local.business,image+0x16d97e0,e)||
       !at(p,image,0x13df290,singleton,e))return false;
    if(singleton!=local.lounge)return fail(e,"Developer actual lounge singleton/GameState differs");
    if(!at(p,local.business,0x4c,out.business_count,e)||!at(p,local.lounge,0x310,out.lounge_count,e)||
       !at(p,local.lounge,0x70,local.data,e)||!array(p,local.lounge,0x70,local.developers,local.capacity,e)||
       !at(p,local.lounge,0x98,local.indoor,e)||!at(p,local.lounge,0xa0,local.outdoor,e))return false;
    if(out.business_count<0||out.business_count>50||out.lounge_count<0||out.lounge_count>50||local.developers.size()!=50||local.indoor==local.outdoor)
        return fail(e,"Developer fixed pool/count/manager owners differ");
    int32_t capacity=0;
    if(!type(p,local.indoor,image+0x16e47f0,e)||!type(p,local.outdoor,image+0x16e47f0,e)||
       !array(p,local.indoor,0x38,local.inside,capacity,e)||!array(p,local.outdoor,0x38,local.outside,capacity,e))return false;
    std::set<uintptr_t> members(local.inside.begin(),local.inside.end());
    for(auto value:local.outside)if(!members.insert(value).second)return fail(e,"Developer appears in both native managers");
    if(members!=std::set<uintptr_t>(local.developers.begin(),local.developers.end()))return fail(e,"Developer manager union differs from actual50-entry catalogue");
    std::set<int32_t> ids;int32_t slot=0;
    for(auto address:local.developers){Row row;uint8_t employed=0,active=0,on_map=0;uintptr_t job=0;row.slot=slot++;
        if(!type(p,address,image+0x17422e0,e)||!at(p,address,0x4c0,row.id,e)||!at(p,address,0x4c4,row.state,e)||
           !at(p,address,0x4d8,employed,e)||!at(p,address,0x4d9,active,e)||!at(p,address,0x368,on_map,e)||!at(p,address,0x4c8,job,e))return false;
        for(size_t i=0;i<3;++i){
            if(!at(p,address,0x110+i*4,row.position[i],e))return false;
            if(!std::isfinite(row.position[i]))return fail(e,"Developer native position is not finite");
        }
        if(row.id<0||row.id>49||!ids.insert(row.id).second||row.state<0||row.state>7||employed>1||active>1||on_map>1||bool(employed)!=(row.slot<out.lounge_count))
            return fail(e,"Developer persisted ID/state/employment prefix differs");
        if(bool(on_map)!=(std::find(local.outside.begin(),local.outside.end(),address)!=local.outside.end()))return fail(e,"Developer map flag and actual manager membership differ");
        row.employed=employed!=0;row.active=active!=0;row.on_map=on_map!=0;row.job_link=job!=0;out.rows.push_back(row);
    }
    return true;
}
}
bool same_origin(const Origin& a,const Origin& b){return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&a.generation==b.generation&&a.world_epoch==b.world_epoch&&a.bytes==b.bytes&&a.uuid==b.uuid&&a.sha256==b.sha256;}
bool read(Port& p,Capture& out,std::string& e){
    out={};e.clear();Capture first,second;
    if(!p.resolver(first.image,first.thread,e)||!first.image||first.image>UINTPTR_MAX-0x1800000||!first.thread)return fail(e,e.empty()?"Developer actual resolver image/thread unavailable":e);
    if(!p.pin(first.pin,e)||!observe(p,first.pin,first.image,first.state,first.local,e)||!p.pin(second.pin,e))return false;
    if(!same_origin(first.pin.origin,second.pin.origin)||first.pin.ledger!=second.pin.ledger||first.pin.time_scale!=second.pin.time_scale||first.pin.pause_scale!=second.pin.pause_scale)
        return fail(e,"Developer full native Origin/ledger/Clock changed during census");
    if(!observe(p,second.pin,first.image,second.state,second.local,e))return false;
    if(first.local!=second.local||first.state!=second.state)return fail(e,"Developer owned pool changed during second census");
    Pin final;if(!p.pin(final,e))return false;
    if(!same_origin(second.pin.origin,final.origin)||second.pin.ledger!=final.ledger||second.pin.time_scale!=final.time_scale||second.pin.pause_scale!=final.pause_scale)
        return fail(e,"Developer full native Origin/ledger/Clock changed after second census");
    out=std::move(first);e.clear();return true;
}
nlohmann::json encode(const Capture& c){
    auto rows=nlohmann::json::array();for(const auto& r:c.state.rows)rows.push_back({{"id",r.id},{"slot",r.slot},{"state",r.state},{"employed",r.employed},{"active",r.active},{"on_map",r.on_map},{"job_link",r.job_link},{"position",r.position}});
    const auto& l=c.pin.ledger;
    return {{"schema",1},{"snapshot_uuid",c.pin.origin.uuid},{"world_epoch",c.pin.origin.world_epoch},{"generation",c.pin.origin.generation},
        {"origin_sha256",c.pin.origin.sha256},{"origin_bytes",c.pin.origin.bytes},{"time_scale",c.pin.time_scale},{"pause_scale",c.pin.pause_scale},
        {"ledger",{{"cash",l.cash},{"flat_day",l.flat_day},{"record_present",l.record_present},{"record_day",l.record_day},{"record_cash",l.record_cash},
                   {"record_income",l.record_income},{"record_expense",l.record_expense},{"line_counts",l.line_counts},{"line_income",l.line_income}}},
        {"business_count",c.state.business_count},{"lounge_count",c.state.lounge_count},{"count_settled",c.state.business_count==c.state.lounge_count},
        {"indoor_count",c.local.inside.size()},{"outdoor_count",c.local.outside.size()},{"developers",rows}};
}
bool payment(Command command,const action_session::Ledger& before,const action_session::Ledger& after,std::string& e){
    if(command==Command::Fire){if(before!=after)return fail(e,"Developer Fire changed native full ledger");e.clear();return true;}
    if(command!=Command::Hire)return fail(e,"Developer command is not Hire/Fire");
    if(before.record_present!=1||before.flat_day!=before.record_day||before.cash!=before.record_cash||before.cash<500||before.line_counts[10]==INT32_MAX||
       int64_t(before.record_expense)+500>INT32_MAX||int64_t(before.line_income[10])-500<INT32_MIN)return fail(e,"Developer Hire actual current record/affordability/overflow rejected");
    auto expected=before;expected.cash-=500;expected.record_cash-=500;expected.record_expense+=500;++expected.line_counts[10];expected.line_income[10]-=500;
    if(expected!=after)return fail(e,"Developer Hire original500/category10 full ledger differs");e.clear();return true;
}
}
