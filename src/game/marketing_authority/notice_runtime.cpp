#include "notice_runtime.h"
#include "notice_hook.h"
#include "notice_lifecycle.h"
#include "scenario_vip_authority/alert_client_native.h"
#include "scenario_vip_authority/disconnect_read.h"
#include "native_open_runtime.h"
#include "session_provider.h"
#include "live_host_state/economic_fence.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/core/logger.h"
#include <Windows.h>

namespace mt2mp::game::marketing_authority::notice {
namespace {
namespace live=live_host_state;
namespace native_alert=scenario_vip_authority::client_alert;
namespace graph=scenario_vip_authority::alert_probe;
namespace message=scenario_vip_authority::disconnect_read;
using World=building_mixed_identity::opened_file::CurrentWorld;
placement_ui_probe::ProcessMemory memory;
bool same_world(const World& a,const World& b){return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&a.uuid==b.uuid&&a.sha256==b.sha256&&a.bytes==b.bytes&&a.world_epoch==b.world_epoch&&a.generation==b.generation;}
Origin origin(const live::Frame& f){return {f.session,f.epoch,f.source_generation,f.snapshot};}
bool authorize(const Origin& o,const action_session::Context& c,const action_session::Binding& b,bool teardown,std::string& e){
    if(teardown||c.role!=action_session::Role::Client||o.session!=c.session||o.snapshot!=c.snapshot_uuid||o.epoch!=net::current_world_epoch()||!live::vip::valid_origin(o)||live::economic_fence().locked())return fail(e,"Client current origin/fence differs");
    action_session::Context current;World world;
    if(!action_session::selected_session().current(current,e)||!native_open_runtime::read_current_world(world,e))return false;
    return (current==c&&world.world_epoch==b.native_epoch&&world.generation==b.native_generation)||fail(e,"Client session/local world differs");
}
struct Sample {uintptr_t marketing=0;std::vector<uintptr_t> members;std::array<int32_t,6> fields{};uint8_t active=0;int32_t day=0;bool operator==(const Sample&)const=default;};
}
struct Runtime::Impl final:notice_hook::Observer {
    action_session::Context context;action_session::Binding binding;World world;
    Origin bound_origin;std::optional<Origin> source;
    uintptr_t image=0;uint32_t thread=0,image_size=0;
    bool bound=false,suspended=false,updating=false,unknown=false;
    std::string failure;Host host;Client client;Json receipts=Json::array(),last_witness=nullptr,last_receipt=nullptr;
    void latch(const std::string& e){if(failure.empty()){failure=e.empty()?"Campaign witness failed without cause":e;LOG_ERROR("[CampaignNotice] {}",failure);}}
    bool pin(std::string& e)const{
        if(!bound||suspended||GetCurrentThreadId()!=thread||!failure.empty())return fail(e,failure.empty()?"inactive/wrong-thread world":failure);
        uintptr_t base=0;uint32_t owner_thread=0;std::string sha;action_session::Context current;World now;
        if(!native::read_resolver_receipt(base,owner_thread,sha,e)||!action_session::selected_session().current(current,e)||!native_open_runtime::read_current_world(now,e))return false;
        return (base==image&&owner_thread==thread&&sha=="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"&&current==context&&same_world(world,now)&&now.world_epoch==binding.native_epoch&&now.generation==binding.native_generation&&net::current_world_epoch()==bound_origin.epoch)||fail(e,"resolver/session/local lifetime differs");
    }
    bool sample(uintptr_t receiver,Sample& out,bool& owned,std::string& e){
        out={};owned=false;if(!pin(e))return false;
        uintptr_t singleton=0,ap=0,data=0;int32_t count=0,capacity=0;
        if(!message::at(memory,uintptr_t(world.state),0x1d0,out.marketing,e)||!message::at(memory,image,0x13df2c0,singleton,e)||
           !message::at(memory,out.marketing,0,ap,e))return false;
        if(out.marketing!=singleton||ap!=image+0x16dfa60)return fail(e,"current Marketing owner differs");
        if(!message::at(memory,out.marketing,0x70,data,e)||!message::at(memory,out.marketing,0x78,count,e)||!message::at(memory,out.marketing,0x7c,capacity,e))return false;
        if(count<0||count>4||capacity<count||(count&&!data))return fail(e,"current Campaign array bound differs");
        for(int32_t i=0;i<count;++i){uintptr_t item=0;if(!message::at(memory,data,size_t(i)*sizeof item,item,e))return false;out.members.push_back(item);}
        const auto matches=std::count(out.members.begin(),out.members.end(),receiver);
        if(!matches){e.clear();return true;}if(matches!=1)return fail(e,"Campaign is not unique current owner");owned=true;
        if(!message::at(memory,receiver,0,ap,e)||ap!=image+0x16e3d50)return fail(e,e.empty()?"Campaign class differs":e);
        for(size_t i=0;i<out.fields.size();++i)if(!message::at(memory,receiver,0x50+i*4,out.fields[i],e))return false;
        if(!message::at(memory,receiver,0x68,out.active,e))return false;
        native::ClockState clock;if(!native::read_clock(clock,e))return false;out.day=clock.flat_day;
        if(out.active>1||out.fields[0]<0||out.fields[0]>8)return fail(e,"Campaign enum/active byte differs");
        return pin(e);
    }
    bool read_graph(graph::Graph& out,std::string& e){graph::Reader reader(memory,image,image_size);return reader.read_ui(uintptr_t(world.mode),uintptr_t(world.state),out,e)&&out.present;}
    bool observe(uintptr_t receiver,notice_hook::Original original)override{
        if(updating){latch("Campaign IsActive observation is reentrant");return original(receiver);}
        struct Busy{bool& flag;explicit Busy(bool& f):flag(f){flag=true;}~Busy(){flag=false;}} busy(updating);
        Sample before;graph::Graph old;bool due=false;std::string e;
        try{
            // Only an active due row needs a full UI graph; ordinary checks stay read-only.
            uint8_t active=0;int32_t start=0,duration=0;native::ClockState clock;
            if(failure.empty()&&message::at(memory,receiver,0x68,active,e)&&active&&
               message::at(memory,receiver,0x54,start,e)&&message::at(memory,receiver,0x58,duration,e)&&native::read_clock(clock,e)&&
               int64_t(start)+duration<=clock.flat_day){
                bool owned=false;Sample again;bool again_owned=false;
                if(!notice_hook::healthy(e)||!notice_hook::verify(image,e)||!sample(receiver,before,owned,e))latch(e);
                else if(owned){
                    const int64_t end=int64_t(start)+duration;
                    if(active!=1||duration<=0||end<INT32_MIN||end>INT32_MAX||before.fields[1]!=start||before.fields[2]!=duration||before.day!=clock.flat_day||before.active!=1)latch("Campaign pre-expiry scalar/arithmetic differs");
                    else if(!read_graph(old,e)||!sample(receiver,again,again_owned,e)||!again_owned||again!=before)latch(e.empty()?"Campaign before original double-read/UI differs":e);
                    else due=true;
                }
            }else if(!e.empty())latch(e);
        }catch(const std::exception& x){latch(std::string("Campaign before observation exception: ")+x.what());}
        catch(...){latch("Campaign before observation nonstandard exception");}
        bool result=false;
        try{result=original(receiver);}catch(...){latch("Campaign original IsActive threw; message outcome unconfirmed");throw;}
        if(due&&failure.empty())try{
            Sample after;bool owned=false;graph::Graph now;
            if(!sample(receiver,after,owned,e)||!owned||!read_graph(now,e))latch(e.empty()?"Campaign after original current owner/UI absent":e);
            else{
                auto expected=before;expected.active=0;
                if(result||after!=expected||!message::same_owner(old,now))latch("Campaign original expiry return/state/UI owner differs");
                else{
                    unsigned matches=0;std::string payload;uintptr_t emitted=0;
                    for(auto created_alert:now.owned.items){if(message::count(old.owned,created_alert))continue;int32_t type=-1;
                        if(!message::at(memory,created_alert,0x18,type,e)){latch(e);break;}if(type!=0)continue;
                        uintptr_t selector=0;if(!message::inspect(memory,image,now,created_alert,1,nullptr,selector,payload,e,0)){latch(e);break;}emitted=created_alert;++matches;
                    }
                    if(failure.empty()){
                        if(matches!=1)latch("Campaign expiry requires exactly one new original owned type0 alert");
                        else if(!pin(e)||!host.append(bound_origin,payload,e))latch(e);
                        else last_witness={{"sequence",host.pending().rows.back().sequence},{"receiver",receiver},{"alert",emitted},{"type",before.fields[0]},
                            {"startedOn",before.fields[1]},{"duration",before.fields[2]},{"day",before.day},{"message",payload},{"original_return",result}};
                    }
                }
            }
        }catch(const std::exception& x){latch(std::string("Campaign after observation exception: ")+x.what());}
        catch(...){latch("Campaign after observation nonstandard exception");}
        return result;
    }
    bool frame(const live::Frame& f,std::string& e){
        if(!pin(e)||unknown)return fail(e,e.empty()?"Client locked after uncertain handoff":e);
        const auto o=origin(f);
        if(o.session!=context.session||o.snapshot!=context.snapshot_uuid||o.epoch!=bound_origin.epoch||!live::vip::valid_origin(o)||!f.marketing||!f.marketing->notices)return fail(e,"complete bound Frame16 notice component required");
        if(context.role==action_session::Role::Host)return o==bound_origin||fail(e,"Host Frame source generation differs");
        if(source)return *source==o||fail(e,"Client Frame source changed without rebind");
        if(!client.bind(o,e))return false;source=o;return true;
    }
};
Runtime::Runtime():impl_(std::make_unique<Impl>()){}
Runtime::~Runtime(){notice_hook::disarm(impl_.get());}
bool Runtime::bind(const action_session::Context& c,const action_session::Binding& b,uint64_t epoch,std::string& e){
    if(impl_->bound&&(!impl_->suspended||impl_->unknown||impl_->updating))return fail(e,"bind requires fresh or suspended confirmed world");
    notice_hook::disarm(impl_.get());auto next=std::make_unique<Impl>();next->context=c;next->binding=b;next->bound_origin={c.session,epoch,b.native_generation,c.snapshot_uuid};
    if(!live::vip::valid_origin(next->bound_origin)||c.local_peer<1||c.local_peer>4||(c.peer_mask&~15u)||!(c.peer_mask&(1u<<(c.local_peer-1))))return fail(e,"bind origin/peer mask invalid");
    std::string sha;if(!native::read_resolver_receipt(next->image,next->thread,sha,e)||!native_open_runtime::read_current_world(next->world,e))return false;
    next->bound=true;if(!next->pin(e))return false;
    int32_t pe=0;if(!message::at(memory,next->image,0x3c,pe,e)||pe<0||pe>4096||!message::at(memory,next->image,size_t(pe)+80,next->image_size,e))return fail(e,e.empty()?"PE extent invalid":e);
    if(c.role==action_session::Role::Host){
        if(c.local_peer!=c.host_peer)return fail(e,"Host binding local peer differs");
        if(!next->host.bind(next->bound_origin,uint32_t(c.peer_mask&~(1u<<(c.local_peer-1))),e)||!notice_hook::initialize(next->image,e)||!notice_hook::arm(next.get(),next->thread,e))return false;
    }
    impl_=std::move(next);e.clear();return true;
}
bool Runtime::suspend(std::string& e){
    if(!impl_->bound||impl_->suspended||impl_->updating||GetCurrentThreadId()!=impl_->thread)return fail(e,"suspend requires idle bound Mainthread");
    notice_hook::disarm(impl_.get());impl_->suspended=true;
    if(impl_->unknown||!impl_->failure.empty())return fail(e,impl_->failure.empty()?"unconfirmed message handoff":impl_->failure);e.clear();return true;
}
bool Runtime::capture(const live::Frame& f,Batch& out,std::string& e){
    out={};if(impl_->context.role!=action_session::Role::Host||origin(f)!=impl_->bound_origin||!impl_->pin(e)||!notice_hook::healthy(e))return fail(e,e.empty()?"capture Host origin differs":e);
    out=impl_->host.pending();return valid(out,e);
}
bool Runtime::sent(const live::Frame& f,std::string& e){if(impl_->context.role!=action_session::Role::Host)return fail(e,"sent requires Host");return impl_->frame(f,e)&&impl_->host.sent(impl_->bound_origin,f.revision,*f.marketing->notices,e);}
bool Runtime::acknowledge(uint64_t peer,uint64_t revision,std::string& e){
    if(impl_->context.role!=action_session::Role::Host||peer>4||!impl_->pin(e))return fail(e,e.empty()?"ACK Host/peer differs":e);
    return impl_->host.acknowledge(impl_->bound_origin,uint32_t(peer),revision,e);
}
bool Runtime::admit(const live::Frame& f,const live::Frame* prior,std::string& e){
    if(impl_->context.role!=action_session::Role::Client||!impl_->frame(f,e))return fail(e,e.empty()?"admit requires Client":e);
    if(prior&&(!prior->marketing||!prior->marketing->notices||origin(*prior)!=origin(f)))return fail(e,"queued origin/component differs");
    return impl_->client.supersede(origin(f),*f.marketing->notices,prior?&*prior->marketing->notices:nullptr,e);
}
bool Runtime::apply(const live::Frame& f,live::MarketingReceipt& result,std::string& e){
    result.confirmed=false;
    struct Outcome {
        live::MarketingReceipt& receipt;bool& unknown;
        ~Outcome(){if(receipt.attempted&&!receipt.confirmed){receipt.unknown=true;unknown=true;}}
    } outcome{result,impl_->unknown};
    if(impl_->context.role!=action_session::Role::Client||!impl_->frame(f,e)){result.cause=e;return false;}
    std::vector<Row> work;if(!impl_->client.prepare(origin(f),*f.marketing->notices,work,e)){result.cause=e;return false;}
    native_alert::NativePort port(origin(f),impl_->context,impl_->binding,authorize,0);
    std::vector<Row> applied;
    for(const auto& row:work){Receipt receipt;const bool okay=present(port,row,receipt,e);result.attempted|=receipt.calls.attempted();
        impl_->last_receipt=encode(receipt);impl_->receipts.push_back(impl_->last_receipt);
        if(!okay){result.unknown=result.attempted||receipt.unknown;result.cause=e;impl_->unknown=result.unknown;return false;}applied.push_back(row);
    }
    if(!impl_->client.confirm(origin(f),*f.marketing->notices,applied,e)){result.unknown=result.attempted;result.cause=e;impl_->unknown=result.unknown;return false;}
    // Keep receipt history only while the Host can still send that sequence.
    const auto floor=f.marketing->notices->after;
    auto& rows=impl_->receipts.get_ref<Json::array_t&>();std::erase_if(rows,[&](const Json& row){return row.at("sequence").get<uint64_t>()<=floor;});
    result.confirmed=true;e.clear();return true;
}
Json Runtime::status()const{return {{"bound",impl_->bound},{"suspended",impl_->suspended},{"unknown",impl_->unknown},{"cause",impl_->failure},
    {"pending",encode(impl_->host.pending())},{"committed",impl_->client.committed()},{"witness",impl_->last_witness},{"last_native",impl_->last_receipt},{"receipts",impl_->receipts}};}
}
