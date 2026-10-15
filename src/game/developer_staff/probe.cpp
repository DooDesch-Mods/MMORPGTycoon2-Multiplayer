#include "probe.h"
#include <climits>
#include <exception>
namespace mt2mp::game::developer_staff {
namespace {
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool uuid(const std::string& value){
    if(value.size()!=36)return false;
    for(size_t i=0;i<value.size();++i){const char c=value[i];if(i==8||i==13||i==18||i==23){if(c!='-')return false;}else if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;}return true;
}
bool same(const Capture& a,const Capture& b){return a.image==b.image&&a.thread==b.thread&&same_origin(a.pin.origin,b.pin.origin)&&a.pin.ledger==b.pin.ledger&&a.pin.time_scale==b.pin.time_scale&&a.pin.pause_scale==b.pin.pause_scale&&a.state==b.state&&a.local==b.local;}
bool before(const Request& request,const Capture& c,std::string& e){
    if(c.pin.origin.uuid!=request.origin_uuid||c.pin.origin.world_epoch!=request.epoch||c.pin.origin.generation!=request.generation)
        return fail(e,"Developer fixture exact expected native Origin differs");
    if(c.state.business_count!=request.expected_count||c.state.lounge_count!=request.expected_count||c.state.rows.size()!=50||c.pin.time_scale!=0)
        return fail(e,"Developer fixture requires expected settled count50 and selected Clock0");
    if(request.command==Command::Hire){if(request.expected_count>=50)return fail(e,"Developer fixture native pool already50");
        // Validate ranges before forming the remaining expected native accounting values.
        if(c.pin.ledger.cash<500||c.pin.ledger.record_present!=1||c.pin.ledger.flat_day!=c.pin.ledger.record_day||c.pin.ledger.cash!=c.pin.ledger.record_cash||
           c.pin.ledger.line_counts[10]==INT32_MAX||int64_t(c.pin.ledger.record_expense)+500>INT32_MAX||int64_t(c.pin.ledger.line_income[10])-500<INT32_MIN)
            return fail(e,"Developer fixture Hire native affordability/current record/overflow rejected");
    }else if(request.command==Command::Fire&&request.expected_count<=0)return fail(e,"Developer fixture native count already0");
    return true;
}
bool effect(Command command,const Capture& a,const Capture& b,std::string& e){
    if(command==Command::Show)return same(a,b)||fail(e,"Developer fixture Show changed native pool/ledger/Origin");
    if(a.image!=b.image||a.thread!=b.thread||!same_origin(a.pin.origin,b.pin.origin)||a.local!=b.local||a.pin.time_scale!=b.pin.time_scale||a.pin.pause_scale!=b.pin.pause_scale)
        return fail(e,"Developer fixture actual native Origin/owners/Clock changed during original call");
    auto expected=a.state;expected.business_count+=command==Command::Hire?1:-1;
    if(b.state!=expected)return fail(e,"Developer fixture original immediate Business count effect differs; no active retirement claimed");
    return payment(command,a.pin.ledger,b.pin.ledger,e);
}
}
bool Probe::run(const Request& request,ProbePort& port,Receipt& out,std::string& e){
    out={};e.clear();
    if(!uuid(request.token)||!uuid(request.origin_uuid)||!request.epoch||!request.generation||request.expected_count<0||request.expected_count>50||
       (request.command!=Command::Hire&&request.command!=Command::Fire&&request.command!=Command::Show))return fail(e,"Developer fixture fixed request invalid");
    if(const auto found=retained_.find(request.token);found!=retained_.end()){out=found->second;out.replay_rejected=true;return fail(e,"Developer fixture token consumed; inspect retained receipt, no replay");}
    if(busy_||locked_)return fail(e,locked_?cause_:"Developer fixture already executing");
    if(retained_.size()>=64)return fail(e,"Developer fixture retained receipt limit reached64");
    uintptr_t window=0,again_window=0;Capture first,again;
    if(!port.authorize(first,window,e)||!window||!before(request,first,e))return false;
    if(!port.authorize(again,again_window,e))return false;
    if(window!=again_window||!same(first,again))return fail(e,"Developer fixture owners/state changed before original call");
    out.token=request.token;out.command=request.command;out.before=first;out.unknown=true;out.cause="Developer fixture original command outstanding; no replay";
    retained_[request.token]=out;busy_=true;struct Reset{bool& value;~Reset(){value=false;}} reset{busy_};
    bool called=false;
    try {
        called=port.call(request.command,first.image,window,out.native,e);
        if(called&&(!out.native.attempted||!out.native.returned||out.native.seh_code||out.native.cleanup_exception||
           (request.command!=Command::Show&&(!out.native.constructed||!out.native.label_confirmed||!out.native.accepted||!out.native.cleanup_attempted||!out.native.cleanup_confirmed))))
            called=fail(e,"Developer fixture original call/record cleanup receipt incomplete");
        if(called){called=port.after(out.after,e);if(called)called=effect(request.command,first,out.after,e);}
    }catch(const std::exception& ex){called=false;e=std::string("Developer fixture callback exception: ")+ex.what();}
    catch(...){called=false;e="Developer fixture callback unknown exception";}
    if(!called){if(e.empty())e="Developer fixture original command failed without cause";out.cause=e;locked_=true;cause_=e;retained_[request.token]=out;return false;}
    out.immediate_effect=true;out.unknown=false;out.cause.clear();retained_[request.token]=out;e.clear();return true;
}
nlohmann::json encode(const Receipt& r){
    const int32_t slot=r.command==Command::Hire?r.before.state.business_count:r.command==Command::Fire?r.before.state.business_count-1:-1;
    int32_t id=-1;if(slot>=0&&size_t(slot)<r.before.state.rows.size())id=r.before.state.rows[size_t(slot)].id;
    return {{"token",r.token},{"command",r.command==Command::Hire?"Hire":r.command==Command::Fire?"Fire":"Show"},{"affected_slot",slot},{"affected_id",id},
    {"native_attempted",r.native.attempted},{"native_returned",r.native.returned},{"native_accepted",r.native.accepted},
    {"record_constructed",r.native.constructed},{"record_label_confirmed",r.native.label_confirmed},{"record_cleanup_attempted",r.native.cleanup_attempted},
    {"record_cleanup_confirmed",r.native.cleanup_confirmed},{"seh_code",r.native.seh_code},{"cleanup_exception",r.native.cleanup_exception},
    {"immediate_effect",r.immediate_effect},{"unknown",r.unknown},{"replay_rejected",r.replay_rejected},{"cause",r.cause},
    {"before",encode(r.before)},{"after",encode(r.after)},{"fixture_only",true},{"retirement_confirmed",false},{"peer_ack",false}};}
}
