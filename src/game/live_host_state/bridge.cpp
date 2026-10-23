#include "bridge.h"
#include "mt2mp/core/hash.h"
#include "apply_timing.h"
#include "economic_fence.h"
#include "network/node_native.h"
#include "tutorial_replication/native.h"
#include "developer_staff/action_native.h"
#include "bug_authority/runtime.h"
namespace mt2mp::game::live_host_state {
namespace {bool fail(std::string& e,const std::string& cause){e=cause;return false;}
// Only ordinary publication ignores the live Research scan stamp. Exact
// Frame identity, retained receipts and action-before publication stay separate.
bool same_tutorial_publication(const std::optional<tutorial_replication::State>& a,
                               const std::optional<tutorial_replication::State>& b){
    if(!a||!b)return a==b;
    auto compared=*b;
    if(a->research&&compared.research)
        compared.research->value.release_counter=a->research->value.release_counter;
    return *a==compared;
}
struct DeveloperFailureLatch {
    bool& attempted;bool& completed;bool& locked;ApplyReceipt& receipt;std::string& cause;
    ~DeveloperFailureLatch(){if(attempted&&!completed){locked=true;receipt.attempted=true;receipt.unknown=true;receipt.confirmed=false;
        if(!cause.empty())receipt.cause=cause;else if(receipt.cause.empty())receipt.cause="live frame failed after Developer property attempt; no replay";}}
};
struct BugFailureLatch {
    bool& attempted;bool& completed;bool& locked;ApplyReceipt& receipt;std::string& cause;
    ~BugFailureLatch(){if(attempted&&!completed){locked=true;receipt.attempted=true;receipt.unknown=true;receipt.confirmed=false;
        if(!cause.empty())receipt.cause=cause;else if(receipt.cause.empty())receipt.cause="live frame failed after Bug property attempt; no replay";}}
};
struct PartyFailureLatch {
    bool& attempted;bool& completed;bool& locked;ApplyReceipt& receipt;std::string& cause;
    ~PartyFailureLatch(){if(attempted&&!completed){locked=true;receipt.attempted=true;receipt.unknown=true;receipt.confirmed=false;
        if(!cause.empty())receipt.cause=cause;else if(receipt.cause.empty())receipt.cause="live frame failed after Party/Instance native effect; no replay";}}
};
struct VipFailureLatch {
    bool& attempted;bool& completed;bool& locked;ApplyReceipt& receipt;std::string& cause;
    ~VipFailureLatch(){if(attempted&&!completed){locked=true;receipt.attempted=true;receipt.unknown=true;receipt.confirmed=false;
        if(!cause.empty())receipt.cause=cause;else if(receipt.cause.empty())receipt.cause="live frame failed after VIP UI attempt; no replay";}}
};
}
bool Bridge::route_receive(uint64_t sender,const uint8_t* bytes,size_t n,bool& handled,std::string& e){
    handled=false;
    if(!bytes||!n||n>max_frame_payload)return fail(e,"live Channel9 packet bound invalid");
    try{
        const auto j=fragments::packet_json(bytes,n);
        if(j.is_object()&&j.contains("kind")&&j.at("kind").is_string()&&j.at("kind")=="host_state_fragment"){
            handled=true;
            if(suspended_){e="info: Host frame fragment skipped for suspended snapshot";return true;}
            std::vector<uint8_t> complete;
            if(!fragments_.take(sender,context_,common_epoch_,source_generation_,j,mt2mp::sha256,complete,e)){locked_=true;return false;}
            if(complete.empty())return true;
            received_frame_transport_=fragments_.completed();received_frame_transport_["admitted"]=false;
            const bool admitted=receive(sender,complete.data(),complete.size(),e);
            received_frame_transport_["admitted"]=admitted;return admitted;
        }
        if(j.is_object()&&j.contains("kind")&&j.at("kind").is_string()&&j.at("kind")=="host_state_subscriber_ack"){
            handled=true;
            if(suspended_){e="info: Subscriber ACK skipped for suspended snapshot";return true;}
            if(!economy_||context_.role!=action_session::Role::Host||sender<1||sender>4||sender==context_.local_peer||!(context_.peer_mask&(1u<<(sender-1)))||
               j.size()!=8||j.at("schema")!=1||!j.at("session").is_number_unsigned()||!j.at("epoch").is_number_unsigned()||!j.at("source_generation").is_number_unsigned()||!j.at("frame_revision").is_number_unsigned()||
               j.at("session").get<uint64_t>()!=context_.session||j.at("epoch").get<uint64_t>()!=common_epoch_||j.at("snapshot_uuid")!=context_.snapshot_uuid||
               j.at("source_generation").get<uint64_t>()!=binding_.native_generation||!j.at("frame_revision").get<uint64_t>()||j.at("frame_revision").get<uint64_t>()>host_revision_)
                return fail(e,"Subscriber Frame ACK authenticated sender/session/epoch/revision differs");
            const auto revision=j.at("frame_revision").get<uint64_t>();
            const auto& rows=j.at("rows");const auto encoded=rows.dump();
            if(!rows.is_array())return fail(e,"live applied Frame ACK rows invalid");
            if(!apply_acks_.validate(sender,revision,encoded,e))return false;
            if(!economy_->acknowledge_subscribers(uint8_t(sender),uint8_t(context_.peer_mask&~(1u<<(context_.local_peer-1))),revision,rows,e))return false;
            if(!acknowledge_vips(sender,revision,e)||(marketing_port_&&!marketing_port_->acknowledge(sender,revision,e))||(management_port_&&!management_port_->acknowledge(sender,revision,e))||(party_port_&&!party_port_->acknowledge(sender,revision,e))){locked_=true;return false;}
            apply_acks_.acknowledge(sender,revision,encoded,std::chrono::steady_clock::now());e.clear();return true;
        }
        // Action and state codecs version independently on the same channel.
        // Their kind identifies the owner; a schema number is not a message type.
        if(!j.is_object()||!j.contains("kind")||!j.at("kind").is_string()||
           j.at("kind")!=(economy_?"host_state_economy":(day_?"host_state_day":"host_state"))){e.clear();return true;}
        handled=true;
        if(fragments_.pending()){locked_=true;return fail(e,"Host frame arrived before fragment completion; no partial Apply");}
        return receive(sender,bytes,n,e);
    }catch(const std::exception& x){return fail(e,std::string("live Channel9 discriminator: ")+x.what());}
}
bool Bridge::capture(uint64_t action,Frame& out,std::string& e){apply_timing::Scope measured_inner_HostCapture(apply_timing::Part::HostCapture);out={};if(suspended_||locked_||context_.role!=action_session::Role::Host||host_revision_==UINT64_MAX)return fail(e,"live capture locked/wrong role/revision exhausted");Pin p;{apply_timing::Scope measured_inner_HostCapturePin(apply_timing::Part::HostCapturePin);if(!port_.read(p,e))return false;}if(p.context!=context_||p.binding!=binding_||p.common_epoch!=common_epoch_)return fail(e,"live capture actual Controller/world pin differs");uint64_t time=0;{apply_timing::Scope measured_inner_HostCaptureTime(apply_timing::Part::HostCaptureTime);if(!time_||!time_(time,e))return fail(e,e.empty()?"live actual Time authority provider missing":e);}out.session=context_.session;out.snapshot=context_.snapshot_uuid;out.epoch=common_epoch_;out.source_generation=binding_.native_generation;out.revision=host_revision_+1;out.base_action_revision=action;out.time_revision=time;out.selected=p.selected;out.pause=p.pause;out.values=p.values;if(party_port_){apply_timing::Scope measured_inner_HostCaptureParty(apply_timing::Part::HostCaptureParty);if(!party_port_->prepare_capture(context_,binding_,out.revision,e)||!port_.read(p,e))return false;if(p.context!=context_||p.binding!=binding_||p.common_epoch!=common_epoch_)return fail(e,"Party preparation changed native Frame owner");out.selected=p.selected;out.pause=p.pause;out.values=p.values;}network::State nodes;{apply_timing::Scope measured_inner_HostCaptureNetwork(apply_timing::Part::HostCaptureNetwork);if(!network::read_native(p,nodes,e))return false;out.network=std::move(nodes);}tutorial_replication::State tutorial;{apply_timing::Scope measured_inner_HostCaptureTutorial(apply_timing::Part::HostCaptureTutorial);if(!tutorial_replication::capture(context_,binding_,tutorial,e))return false;out.tutorial=std::move(tutorial);}bug_authority::State bugs;{apply_timing::Scope measured_inner_HostCaptureBugs(apply_timing::Part::HostCaptureBugs);if(!bug_authority::capture(context_,binding_,out.revision,bugs,e))return false;out.bugs=std::move(bugs);}developer_staff::PoolState developers;{apply_timing::Scope measured_inner_HostCaptureDevelopers(apply_timing::Part::HostCaptureDevelopers);if(!developer_staff::action_runtime::capture(context_,binding_,developers,e))return false;out.developers=std::move(developers);}{apply_timing::Scope measured_inner_HostCaptureVips(apply_timing::Part::HostCaptureVips);if(!capture_vips(out,e))return false;}if(marketing_port_){apply_timing::Scope measured_inner_HostCaptureMarketing(apply_timing::Part::HostCaptureMarketing);marketing_authority::State marketing;if(!marketing_port_->capture(context_,binding_,out,marketing,e))return false;out.marketing=std::move(marketing);}if(management_port_){apply_timing::Scope measured_inner_HostCaptureManagement(apply_timing::Part::HostCaptureManagement);management_simulation::LiveState management;if(!management_port_->capture(context_,binding_,management,e))return false;out.management=std::move(management);}if(release_port_){apply_timing::Scope measured_inner_HostCaptureRelease(apply_timing::Part::HostCaptureRelease);release_cycle::Publication release;if(!release_port_->capture(context_,binding_,release,e))return false;out.release=std::move(release);}{apply_timing::Scope measured_inner_HostCaptureValidate(apply_timing::Part::HostCaptureValidate);if(!valid(out,e))return false;}if(day_){apply_timing::Scope measured_inner_HostCaptureDay(apply_timing::Part::HostCaptureDay);day::Target target;if(!day_->capture(out,target,e))return false;captured_day_=std::move(target);if(economy_){apply_timing::Scope measured_inner_HostCaptureEconomy(apply_timing::Part::HostCaptureEconomy);day::economy::State state;if(!economy_->capture(*captured_day_,state,e))return false;if(party_port_){if(!state.day.frame.party)return fail(e,"Party native capture omitted its complete Frame component");out.party=state.day.frame.party;}captured_economy_=std::move(state);}}if(monster_port_){apply_timing::Scope measured_inner_HostCaptureMonsters(apply_timing::Part::HostCaptureMonsters);monsters::State monsters;if(!monster_port_->capture(context_,binding_,out,monsters,e))return false;out.monsters=std::move(monsters);}{apply_timing::Scope measured_inner_HostCaptureManagementFinish(apply_timing::Part::HostCaptureManagementFinish);if(management_port_&&!management_port_->finish_capture(context_,binding_,out,e))return false;}{apply_timing::Scope measured_inner_HostCaptureFinalize(apply_timing::Part::HostCaptureFinalize);if(captured_day_)captured_day_->frame=out;if(captured_economy_)captured_economy_->day.frame=out;return valid(out,e);}}
bool Bridge::send(const Frame& frame,std::string& e){apply_timing::Scope measured_inner_HostSend(apply_timing::Part::HostSend);
    std::vector<uint8_t> bytes;
    if(economy_){if(!captured_economy_||captured_economy_->day.frame!=frame)return fail(e,"live exact captured Economy frame missing");if(!day::economy::encode(*captured_economy_,bytes,e))return false;}
    else if(day_){if(!captured_day_||captured_day_->frame!=frame)return fail(e,"live actual captured Day frame missing");if(!day::encode(*captured_day_,bytes,e))return false;}
    else if(!encode(frame,bytes,e))return false;
    fragments::Packets packets;if(!fragments::encode(frame,bytes,mt2mp::sha256,packets,e))return false;
    try{
        sent_frame_transport_={{"bytes",bytes.size()},{"chunks",packets.size()},{"revision",frame.revision},
            {"sha256",packets.size()>1?nlohmann::json::parse(packets.front()).at("sha256"):nlohmann::json(nullptr)},
            {"packets_queued",0},{"queued",false}};
        size_t queued=0;
        for(uint64_t peer=1;peer<=4;++peer)if(peer!=context_.local_peer&&(context_.peer_mask&(1u<<(peer-1))))
            for(const auto& packet:packets){
                if(!transport_.send(peer,packet,e)){locked_=true;return fail(e,"live Host frame send Unknown peer="+std::to_string(peer)+" original="+e+"; no retry");}
                sent_frame_transport_["packets_queued"]=++queued;
            }
        sent_frame_transport_["queued"]=true;
        if(!sent_vips(frame,e)||(marketing_port_&&!marketing_port_->sent(frame,e))||(management_port_&&!management_port_->sent(frame,e))||(party_port_&&!party_port_->sent(frame,e))){locked_=true;return fail(e,"live queued presentation Frame retention failed original="+e+"; no retry");}
        host_revision_=frame.revision;last_=frame;
        if(economy_)apply_acks_.sent(frame.revision,uint8_t(context_.peer_mask&~(1u<<(context_.local_peer-1))),std::chrono::steady_clock::now());
        if(day_){last_day_sequence_=captured_day_->history.back().sequence;last_day_=captured_day_;}
        if(economy_)last_economy_=captured_economy_;return true;
    }catch(const std::exception& x){locked_=true;return fail(e,std::string("live Host frame send Unknown original exception: ")+x.what());}
    catch(...){locked_=true;return fail(e,"live Host frame send Unknown original nonstandard exception");}
}
bool Bridge::capture_before(const action_session::Request& r,const action_session::Ledger& before,uint64_t previous,std::string& e){paused_idle_equal_=false;Frame frame;if(r.snapshot_uuid!=context_.snapshot_uuid||r.host_revision!=previous||!capture(previous,frame,e))return fail(e,e.empty()?"live Host before request/snapshot/order differs":e);frame.for_action=r.action_uuid;if(!valid(frame,e))return false;if(ledger(frame.values)!=before)return fail(e,"live Host capture differs from actual action before ledger");host_before_=frame;if(day_){captured_day_->frame=frame;before_day_=captured_day_;if(economy_){captured_economy_->day.frame=frame;before_economy_=captured_economy_;}}e.clear();return true;}
bool Bridge::publish_before(const action_session::Message& result,std::string& e){if(!host_before_||result.kind!=action_session::Kind::HostResult||result.outcome!=action_session::Outcome::Confirmed||result.session!=context_.session||result.request.action_uuid!=host_before_->for_action||result.revision!=host_before_->base_action_revision+1||result.evidence.before!=ledger(host_before_->values))return fail(e,"live Host before publication differs from actual confirmed result");if(day_)captured_day_=before_day_;if(economy_)captured_economy_=before_economy_;if(!send(*host_before_,e)){locked_=true;return false;}host_before_.reset();before_day_.reset();before_economy_.reset();return true;}
bool Bridge::receive(uint64_t sender,const uint8_t* bytes,size_t n,std::string& e){if(locked_)return fail(e,"live Client frame locked after Unknown; no replay");if(suspended_){e="info: live frame waits during actual Snapshot load";return true;}Frame frame;std::optional<day::Target> target;std::optional<day::economy::State> economic;if(economy_){day::economy::State decoded;if(!day::economy::decode(bytes,n,decoded,e))return false;frame=decoded.day.frame;target=decoded.day;economic=std::move(decoded);}else if(day_){day::Target decoded;if(!day::decode(bytes,n,decoded,e))return false;frame=decoded.frame;target=std::move(decoded);}else if(!decode(bytes,n,frame,e))return false;if(bool(frame.party)!=bool(party_port_))return fail(e,"Party complete component/provider presence differs before Frame admission");if(bool(frame.monsters)!=bool(monster_port_))return fail(e,"Monster complete component/provider presence differs before Frame admission");if(bool(frame.release)!=bool(release_port_))return fail(e,"Release complete component/provider presence differs before full Frame ACK");if(bool(frame.management)!=bool(management_port_))return fail(e,"Management component/provider presence differs before full Frame ACK");if(bool(frame.marketing)!=bool(marketing_port_))return fail(e,"Marketing component/provider presence differs before full Frame ACK");if(bool(frame.vips)!=bool(vip_port_))return fail(e,vip_port_?"armed VIP Frame component missing":"VIP Frame requires native UI consumer before full-frame ACK");uint64_t time=0;if(!time_||!time_(time,e))return fail(e,e.empty()?"live actual Time authority provider missing":e);const auto gate=admit(sender,context_,common_epoch_,source_generation_?source_generation_:frame.source_generation,last_?&*last_:nullptr,frame,frame.base_action_revision,time,true,e);if(gate==Admission::Rejected)return false;if(gate==Admission::Duplicate||gate==Admission::Stale){if(gate==Admission::Duplicate&&day_&&(!last_day_||target!=last_day_))return fail(e,"live duplicate Frame conflicts with retained Day history");if(gate==Admission::Duplicate&&economy_&&economic!=last_economy_)return fail(e,"live duplicate Frame conflicts with retained Economy state");e.clear();return true;}if(!admit_vips(frame,e)||(marketing_port_&&!marketing_port_->admit(frame,queued_?&*queued_:nullptr,e))||(management_port_&&!management_port_->admit(frame,queued_?&*queued_:nullptr,e))||(party_port_&&!party_port_->admit(frame,queued_?&*queued_:nullptr,e)))return false;if(queued_&&frame.revision<=queued_->revision){if(frame==*queued_&&(!day_||target==queued_day_)&&(!economy_||economic==queued_economy_)){e.clear();return true;}return fail(e,"live queued revision stale/conflicting; no overwrite");}if(queued_&&!queued_->for_action.empty())return fail(e,"live action before frame retained until matching HostResult; no overwrite");source_generation_=frame.source_generation;queued_=frame;queued_day_=std::move(target);queued_economy_=std::move(economic);e.clear();return true;}
bool Bridge::take(const Frame& frame,std::string& e){apply_timing::Scope measured_take(apply_timing::Part::Take);bool party_attempted=false,party_completed=false;PartyFailureLatch party_latch{party_attempted,party_completed,locked_,receipt_,e};bool vip_attempted=false,vip_completed=false;VipFailureLatch vip_latch{vip_attempted,vip_completed,locked_,receipt_,e};std::vector<vip::Transition> vip_work;if(!prepare_vips(frame,vip_work,e))return false;bool developer_attempted=false,developer_completed=false;DeveloperFailureLatch developer_latch{developer_attempted,developer_completed,locked_,receipt_,e};bool bug_attempted=false,bug_completed=false;BugFailureLatch bug_latch{bug_attempted,bug_completed,locked_,receipt_,e};if(locked_)return fail(e,"live Client Apply locked after Unknown; no replay");if(bool(frame.party)!=bool(party_port_))return fail(e,"Party complete component/provider presence differs before mutation");if(party_port_){PartyReceipt prepared;const bool okay=([&]{apply_timing::Scope measured(apply_timing::Part::ClientPartyPrepare);return party_port_->prepare_apply(context_,binding_,frame,prepared,e);}());party_attempted=prepared.attempted||prepared.unknown;if(!okay||!prepared.confirmed)return fail(e,e.empty()?"Party/Instance structural receipt unconfirmed":e);}Pin before;if(!port_.read(before,e))return false;if(before.context!=context_||before.binding!=binding_||before.common_epoch!=common_epoch_)return fail(e,"live Client actual pin differs from Controller");if(!frame.network||!([&]{apply_timing::Scope measured_group(apply_timing::Part::NetworkPreflight);return network::preflight_native(port_,*frame.network,e);}()))return fail(e,e.empty()?"live exact Node state/preflight absent":e);if(!frame.tutorial||!tutorial_replication::valid(*frame.tutorial,e))return fail(e,e.empty()?"live tutorial state absent":e);
if(bool(frame.monsters)!=bool(monster_port_))return fail(e,"Monster component/provider presence differs before mutation");
if(monster_port_&&!monster_port_->preflight(context_,binding_,frame,e))return false;
if(applying_economy_)return fail(e,"live nested Economy adoption rejected");
if(economy_&&(!queued_economy_||queued_economy_->day.frame!=frame))return fail(e,"live exact queued Economy frame missing");
struct WeaponEconomyScope {
    const day::economy::State*& slot;
    WeaponEconomyScope(const day::economy::State*& target,const day::economy::State* value):slot(target){slot=value;}
    ~WeaponEconomyScope(){slot=nullptr;}
} weapon_scope(applying_economy_,economy_?&*queued_economy_:nullptr);
if(frame.bugs&&!([&]{apply_timing::Scope measured_group(apply_timing::Part::BugPreflight);return bug_authority::preflight_frame(context_,binding_,frame.revision,*frame.bugs,e,
    frame.developers&&frame.developers->construction&&frame.developers->construction->work?&*frame.developers->construction->work:nullptr);}()))return false;
if(!frame.developers||!([&]{apply_timing::Scope measured_group(apply_timing::Part::DeveloperPreflight);return developer_staff::action_runtime::preflight_frame(context_,binding_,*frame.developers,e);}()))return fail(e,e.empty()?"live actual Developer pool absent":e);
bool bug_unknown=false;if(frame.bugs&&!([&]{apply_timing::Scope measured_group(apply_timing::Part::BugAdopt);return bug_authority::adopt_frame(context_,binding_,frame.revision,*frame.bugs,bug_attempted,bug_unknown,e);}())){
    if(bug_unknown){locked_=true;receipt_.unknown=true;receipt_.confirmed=false;receipt_.cause=e;}return false;}
bool developer_unknown=false;if(!([&]{apply_timing::Scope measured_group(apply_timing::Part::DeveloperAdopt);return developer_staff::action_runtime::adopt_frame(context_,binding_,*frame.developers,developer_attempted,developer_unknown,e);}())){
    if(developer_unknown){locked_=true;receipt_.unknown=true;receipt_.confirmed=false;receipt_.cause=e;}return false;}
if(frame.bugs){
    bool finish_attempted=false,finish_unknown=false,finished=false;
    try{finished=([&]{apply_timing::Scope measured_group(apply_timing::Part::BugFinish);return bug_authority::finish_frame(context_,binding_,frame.revision,*frame.bugs,finish_attempted,finish_unknown,e);}());}
    catch(...){bug_attempted=bug_attempted||finish_attempted;throw;}
    bug_attempted=bug_attempted||finish_attempted;
    if(!finished){if(finish_unknown){locked_=true;receipt_.unknown=true;receipt_.confirmed=false;receipt_.cause=e;}return false;}
}
if(!frame.tutorial||!([&]{apply_timing::Scope measured_group(apply_timing::Part::TutorialPreflight);return tutorial_replication::preflight(context_,binding_,*frame.tutorial,e);}()))return fail(e,e.empty()?"live tutorial state absent":e);day::economy::Owners economy_before;if(economy_){if(!queued_economy_||queued_economy_->day.frame!=frame)return fail(e,"live exact queued Economy frame missing");bool prepared=false;{apply_timing::Scope measured(apply_timing::Part::BeforeDay);prepared=economy_->before_day(*queued_economy_,economy_before,e);}if(party_port_){const auto state=economy_->addition_receipt();const auto& actual=state.at("last_creation");if(actual.is_object()&&actual.value("frame_revision",uint64_t{})==frame.revision)party_attempted=party_attempted||actual.value("attempted",false)||actual.value("unknown",false);}if(!prepared){if(economy_->locked()){locked_=true;receipt_.unknown=true;receipt_.cause=e;}return false;}}if(day_){if(!queued_day_||queued_day_->frame!=frame)return fail(e,"live exact queued Day frame missing");bool day_applied=false;{apply_timing::Scope measured(apply_timing::Part::DayTake);day_applied=day_->take(*queued_day_,receipt_,e);}if(!day_applied){if(day_->locked()||receipt_.unknown)locked_=true;return false;}if(!economic_fence().adopt_day_commit(e)){locked_=true;receipt_.unknown=true;receipt_.confirmed=false;receipt_.cause="live Day fence adoption Unknown original="+e;return false;}if(economy_){bool applied=false;{apply_timing::Scope measured(apply_timing::Part::AfterDay);applied=economy_->after_day(*queued_economy_,economy_before,e);}receipt_.attempted=receipt_.attempted||economy_->receipt().attempted;receipt_.attempted_stores+=economy_->receipt().attempted_stores;receipt_.completed_stores+=economy_->receipt().stores;if(!applied){locked_=true;receipt_.unknown=true;receipt_.confirmed=false;receipt_.cause="live actual Economy Apply Unknown original="+e;return false;}}}else receipt_=apply(port_,before,frame);receipt_.attempted=receipt_.attempted||developer_attempted||bug_attempted;if(receipt_.unknown)locked_=true;if(!receipt_.confirmed)return fail(e,receipt_.cause);network_receipt_=([&]{apply_timing::Scope measured_group(apply_timing::Part::NetworkApply);return network::apply_native(port_,*frame.network);}());if(!network_activation_receipt_.record(frame,binding_,network_receipt_,e)){locked_=true;receipt_.unknown=true;receipt_.confirmed=false;receipt_.cause=e;return false;}receipt_.attempted=receipt_.attempted||network_receipt_.attempted;if(!network_receipt_.confirmed){locked_=receipt_.attempted||network_receipt_.unknown;receipt_.unknown=locked_;receipt_.confirmed=false;receipt_.cause="live actual Node Apply failed original="+network_receipt_.cause;return fail(e,receipt_.cause);}bool tutorial_attempted=false,tutorial_unknown=false;if(!([&]{apply_timing::Scope measured_group(apply_timing::Part::TutorialApply);return tutorial_replication::apply(context_,binding_,*frame.tutorial,tutorial_attempted,tutorial_unknown,e);}())){locked_=receipt_.attempted||tutorial_attempted||tutorial_unknown;receipt_.unknown=locked_;receipt_.confirmed=false;receipt_.cause=e;return false;}receipt_.attempted=receipt_.attempted||tutorial_attempted;if(!apply_marketing(frame,e))return false;if(!apply_management(frame,e))return false;if(!apply_release(frame,e))return false;if(!apply_vips(frame,vip_work,vip_attempted,e))return false;if(!apply_monsters(frame,e))return false;if(!acknowledge_frame(frame,e))return false;party_completed=true;vip_completed=true;developer_completed=true;bug_completed=true;last_=frame;if(day_){last_day_sequence_=day_->sequence();last_day_=queued_day_;}if(economy_)last_economy_=queued_economy_;e.clear();return true;}
bool Bridge::acknowledge_frame(const Frame& frame,std::string& e){
    if(!economy_){e.clear();return true;}
    auto rows=nlohmann::json::array();
    if(economy_&&queued_economy_&&queued_economy_->additions.available)
        rows=mt2mp::subscriber_lifecycle::additions::Cache::acknowledgement(queued_economy_->additions);
    const auto text=nlohmann::json{{"kind","host_state_subscriber_ack"},{"schema",1},{"session",context_.session},{"epoch",common_epoch_},{"snapshot_uuid",context_.snapshot_uuid},{"source_generation",frame.source_generation},{"frame_revision",frame.revision},{"rows",rows}}.dump();
    std::vector<uint8_t> bytes(text.begin(),text.end());
    if(bytes.size()>max_frame_payload||!transport_.send(context_.host_peer,bytes,e)){locked_=true;receipt_.unknown=true;receipt_.confirmed=false;receipt_.cause="Client confirmed Frame ACK send failed original="+e+"; no native replay";return fail(e,receipt_.cause);}
    if(party_port_&&!party_port_->committed(frame,e)){locked_=true;receipt_.unknown=true;receipt_.confirmed=false;receipt_.cause="Party Frame ACK cleanup failed original="+e;return false;}
    if(economy_&&queued_economy_&&queued_economy_->additions.available)economy_->subscriber_frame_committed(frame.revision);
    e.clear();return true;
}
bool Bridge::application_ready(bool& waiting,std::string& e){
    if(!port_.application_ready(common_epoch_,waiting,e)){application_waiting_=false;return false;}
    if(waiting){
        application_last_wait_=e;if(application_first_wait_.empty())application_first_wait_=e;
        if(application_wait_count_!=UINT64_MAX)++application_wait_count_;
    }else if(application_waiting_&&application_recovery_count_!=UINT64_MAX)++application_recovery_count_;
    application_waiting_=waiting;return true;
}
bool Bridge::tick(uint64_t action,bool pending,std::string& e,std::chrono::steady_clock::time_point now){
    publication_wait_.clear();
    if(suspended_){e="info: live state suspended for actual Snapshot load";return true;}
    if(locked_)return fail(e,"live frame locked after Unknown; no replay");
    if(!fragments_.timely(now,e)){locked_=true;return false;}
    if(context_.role==action_session::Role::Host){
        if(economy_&&!apply_acks_.timely(now,e)){locked_=true;return false;}
        if(pending){e="info: Host frame waits for existing action completion";return true;}
        uint64_t time=0;
        if(!time_||!time_(time,e))return fail(e,e.empty()?"live actual Time authority provider missing":e);
        // Authority changes and exact action-before frames must not wait for a
        // superseded ordinary frame. Unchanged authority waits for full Apply.
        if(economy_&&last_&&last_->base_action_revision==action&&last_->time_revision==time){
            bool waiting=false;if(!apply_acks_.ready(now,waiting,e)){locked_=true;return false;}
            if(waiting){publication_wait_=e;return true;}
        }
        // Leave 250ms after a completed ordinary capture for native updates and rendering.
        // Authority changes bypass the wait.
        // Action before-frames use capture_before/publish_before independently.
        if(last_&&last_capture_&&last_->base_action_revision==action&&last_->time_revision==time&&
           now-*last_capture_<std::chrono::milliseconds(250)){e.clear();return true;}
        bool waiting=false;
        {apply_timing::Scope measured_inner_HostPublicationReady(apply_timing::Part::HostPublicationReady);if(!port_.publication_ready(common_epoch_,waiting,e))return false;}
        if(waiting){
            paused_idle_equal_=false;
            publication_wait_=publication_last_wait_=e;
            if(publication_first_wait_.empty())publication_first_wait_=e;
            if(publication_wait_count_!=UINT64_MAX)++publication_wait_count_;
            publication_was_waiting_=true;return true;
        }
        if(publication_was_waiting_){
            if(publication_recovery_count_!=UINT64_MAX)++publication_recovery_count_;
            publication_was_waiting_=false;
        }
        // An equal paused publication still needs bounded polls for native UI changes.
        // Read current owners, clock and scalars before extending only the idle interval.
        if(paused_idle_equal_&&last_&&last_capture_&&last_->for_action.empty()&&
           last_->base_action_revision==action&&last_->time_revision==time&&
           (!day_||last_day_sequence_==day_->sequence())&&
           now-*last_capture_<std::chrono::milliseconds(1000)){
            Pin current;if(!port_.read(current,e))return false;
            if(current.context!=context_||current.binding!=binding_||current.common_epoch!=common_epoch_)
                return fail(e,"live paused idle actual Controller/world pin differs");
            if(current.selected==last_->selected&&current.pause==last_->pause&&
               (current.selected==0||current.pause==0)&&current.values==last_->values){e.clear();return true;}
        }
        paused_idle_equal_=false;
        Frame frame;if(!capture(action,frame,e))return false;
        last_capture_=capture_clock_?capture_clock_():std::chrono::steady_clock::now();
        {apply_timing::Scope measured_inner_HostCompare(apply_timing::Part::HostCompare);
        if(last_&&(!day_||last_day_sequence_==day_->sequence())&&
           (!economy_||(captured_economy_&&last_economy_&&
               captured_economy_->subscribers==last_economy_->subscribers&&
               captured_economy_->loans==last_economy_->loans&&
               captured_economy_->offers==last_economy_->offers&&
               captured_economy_->npcs==last_economy_->npcs&&
               captured_economy_->additions==last_economy_->additions))&&
           last_->management==frame.management&&last_->monsters==frame.monsters&&last_->marketing==frame.marketing&&last_->vips==frame.vips&&last_->bugs==frame.bugs&&last_->developers==frame.developers&&same_tutorial_publication(last_->tutorial,frame.tutorial)&&last_->network==frame.network&&last_->values==frame.values&&last_->selected==frame.selected&&
           last_->pause==frame.pause&&last_->base_action_revision==action&&last_->time_revision==frame.time_revision){
            paused_idle_equal_=last_->for_action.empty()&&(frame.selected==0||frame.pause==0);
            e.clear();return true;}
}

        return send(frame,e);
    }
    if(!queued_){e.clear();return true;}
    uint64_t time=0;if(!time_||!time_(time,e))return fail(e,e.empty()?"live actual Time authority provider missing":e);
    const auto gate=admit(context_.host_peer,context_,common_epoch_,source_generation_,last_?&*last_:nullptr,*queued_,action,time,pending,e);
    if(gate==Admission::Wait)return true;
    if(gate==Admission::Rejected)return false;
    if(gate==Admission::New){
        bool waiting=false;if(!application_ready(waiting,e))return false;
        // Keep all queued components until native saving ends, before any Frame work.
        if(waiting){publication_wait_=e;return true;}
        if(!take(*queued_,e))return false;
    }
    queued_.reset();queued_day_.reset();queued_economy_.reset();e.clear();return true;
}
bool Bridge::before_result(uint64_t sender,const action_session::Message& result,uint64_t previous,std::string& e){
    if(locked_||!queued_)return fail(e,locked_?"live before-result locked; no replay":"live exact HostBeforeFrame missing before native replay");
    uint64_t time=0;if(!time_||!time_(time,e))return fail(e,e.empty()?"live actual Time authority provider missing":e);
    const auto gate=admit_before_result(sender,context_,common_epoch_,source_generation_,last_?&*last_:nullptr,*queued_,result,previous,time,e);
    if(gate!=Admission::New&&gate!=Admission::Duplicate)return fail(e,"live authority before-result refused: "+e);
    if(gate==Admission::New){
        bool waiting=false;if(!application_ready(waiting,e))return false;
        // The Controller has committed this HostResult by now; only before_ready may defer it.
        if(waiting)return fail(e,"live authority before-result native save began after readiness; no replay: "+e);
        if(!take(*queued_,e))return false;
    }
    queued_.reset();queued_day_.reset();queued_economy_.reset();e.clear();return true;
}
action_session::AuthorityReadiness Bridge::before_ready(uint64_t sender,const action_session::Message& result,uint64_t previous,std::string& e){
    using Ready=action_session::AuthorityReadiness;
    if(locked_||!queued_){e=locked_?"live before-result locked":"live exact HostBeforeFrame missing before native replay";return Ready::Rejected;}
    uint64_t time=0;if(!time_||!time_(time,e)){if(e.empty())e="live actual Time authority provider missing";return Ready::Rejected;}
    const auto gate=admit_before_result(sender,context_,common_epoch_,source_generation_,last_?&*last_:nullptr,*queued_,result,previous,time,e);
    if(gate==Admission::Wait){e="info: live HostResult waits for actual Time revision measured="+std::to_string(time)+" required="+std::to_string(queued_->time_revision);return Ready::Wait;}
    if(gate!=Admission::New&&gate!=Admission::Duplicate)return Ready::Rejected;
    if(gate==Admission::New){
        bool waiting=false;if(!application_ready(waiting,e))return Ready::Rejected;
        if(waiting)return Ready::Wait;
    }
    e.clear();return Ready::Ready;
}
}

namespace mt2mp::game::live_host_state {
bool Bridge::suspend_snapshot(std::string& e){if(locked_||suspended_||host_before_||queued_||!suspend_vips(e)||(management_port_&&!management_port_->suspend(e))||(release_port_&&!release_port_->suspend(e))||(marketing_port_&&!marketing_port_->suspend(e))||(day_&&!day_->suspend_snapshot(e))||(monster_port_&&!monster_port_->suspend(e))||(party_port_&&!party_port_->suspend(e)))return fail(e,e.empty()?"live snapshot suspend locked/pending/unbound":e);paused_idle_equal_=false;suspended_=true;fragments_.clear();apply_acks_.clear();network_activation_receipt_.clear();e.clear();return true;}
bool Bridge::suspend_lobby(std::string& e){
    if(suspended_||host_before_)return fail(e,"live lobby suspension requires unsuspended owner without active Host-before action");
    if((management_port_&&!management_port_->suspend(e))||(release_port_&&!release_port_->suspend(e)))return false;
    std::string party_cause;const bool party_clean=!party_port_||party_port_->suspend(party_cause);
    if(!party_clean){locked_=true;receipt_.unknown=true;receipt_.confirmed=false;if(receipt_.cause.empty())receipt_.cause=party_cause;}
    const bool vip_clean=suspend_vips(e);
    std::string monster_cause;const bool monster_clean=!monster_port_||monster_port_->suspend(monster_cause);
    if(!monster_clean){locked_=true;receipt_.unknown=true;receipt_.confirmed=false;if(receipt_.cause.empty())receipt_.cause=monster_cause;}
    std::string marketing_cause;const bool marketing_clean=!marketing_port_||marketing_port_->suspend(marketing_cause);
    if(!marketing_clean){locked_=true;receipt_.unknown=true;receipt_.confirmed=false;if(receipt_.cause.empty())receipt_.cause=marketing_cause;}
    const std::string vip_cause=e;
    if(!vip_clean){locked_=true;receipt_.unknown=true;receipt_.confirmed=false;if(receipt_.cause.empty())receipt_.cause=vip_cause;}
    // Retain the failed frame, lock and receipt until the old native World ends.
    paused_idle_equal_=false;suspended_=lobby_retiring_=true;fragments_.clear();apply_acks_.clear();
    if(!vip_clean)e="warn: live lobby retiring after VIP cleanup failure original="+vip_cause;else if(!marketing_clean)e="warn: live lobby retiring after Marketing suspension failure original="+marketing_cause;else if(!monster_clean)e="warn: live lobby retiring after Monster suspension failure original="+monster_cause;else if(!party_clean)e="warn: live lobby retiring after Party suspension failure original="+party_cause;else e.clear();return true;
}
bool Bridge::rebind_snapshot(const Context& c,const Binding& b,uint64_t epoch,std::string& e){if(lobby_retiring_||!suspended_||locked_||!epoch||c.session!=context_.session||c.role!=context_.role||c.local_peer!=context_.local_peer||c.host_peer!=context_.host_peer||c.peer_mask!=context_.peer_mask)return fail(e,"live snapshot rebind requires suspended same Controller");Pin actual;if(!port_.read(actual,e)||actual.context!=c||actual.binding!=b||actual.common_epoch!=epoch)return fail(e,e.empty()?"live snapshot fresh actual Pin differs":e);if(day_&&!day_->rebind_snapshot(c,b,e))return false;if(economy_&&!economy_->rebind_snapshot(e))return false;if((management_port_&&!management_port_->bind(c,b,epoch,e))||(release_port_&&!release_port_->bind(c,b,epoch,e))||!reset_vips(c,b,epoch,e)||(marketing_port_&&!marketing_port_->bind(c,b,epoch,e))||(monster_port_&&!monster_port_->bind(c,b,epoch,e))||(party_port_&&!party_port_->bind(c,b,epoch,e)))return false;context_=c;binding_=b;common_epoch_=epoch;time_=[c](uint64_t& revision,std::string& cause){return read_time_authority(c,revision,cause);};last_.reset();queued_.reset();host_before_.reset();captured_day_.reset();queued_day_.reset();before_day_.reset();last_day_.reset();captured_economy_.reset();queued_economy_.reset();before_economy_.reset();last_economy_.reset();host_revision_=source_generation_=last_day_sequence_=0;last_capture_.reset();paused_idle_equal_=false;fragments_.clear();apply_acks_.clear();sent_frame_transport_=received_frame_transport_=nullptr;network_activation_receipt_.clear();publication_wait_.clear();publication_first_wait_.clear();publication_last_wait_.clear();publication_wait_count_=publication_recovery_count_=0;publication_was_waiting_=false;application_first_wait_.clear();application_last_wait_.clear();application_wait_count_=application_recovery_count_=0;application_waiting_=false;suspended_=false;e.clear();return true;}
}

namespace mt2mp::game::live_host_state {
bool Bridge::after_action(const action_session::Message& m,std::string& e){if(monster_port_&&!monster_port_->after_action(context_,binding_,e)){locked_=true;return false;}if(!day_)return true;if(!day_->after_action(m,e)){locked_=true;return false;}return true;}
}

namespace mt2mp::game::live_host_state {
bool Bridge::bind_vip(VipPort& port,std::string& e){
    if(vip_port_||!economy_||suspended_||locked_||last_||queued_||host_before_||host_revision_||source_generation_)
        return fail(e,"VIP binding requires pristine unsuspended Economy Bridge");
    std::optional<vip::Origin> next;
    if(context_.role==action_session::Role::Host){
        vip::Origin origin{context_.session,common_epoch_,binding_.native_generation,context_.snapshot_uuid};
        if(!vip::valid_origin(origin))return fail(e,"VIP Host binding origin invalid");
        next=std::move(origin);
    }
    if(!port.bind(context_,binding_,common_epoch_,e))return false;
    vip_origin_=std::move(next);vip_port_=&port;e.clear();return true;
}
bool Bridge::capture_vips(Frame& frame,std::string& e){
    if(!vip_port_){e.clear();return true;}
    const vip::Origin origin{frame.session,frame.epoch,frame.source_generation,frame.snapshot};
    if(!vip_origin_||origin!=*vip_origin_)return fail(e,"VIP capture bound origin differs");
    vip::Batch batch;if(!vip_port_->host_capture(origin,batch,e)||!vip::valid(batch,e))return false;
    frame.vips=std::move(batch);e.clear();return true;
}
bool Bridge::sent_vips(const Frame& frame,std::string& e){
    if(!vip_port_){e.clear();return true;}
    const vip::Origin origin{frame.session,frame.epoch,frame.source_generation,frame.snapshot};
    if(!vip_origin_||origin!=*vip_origin_||!frame.vips)return fail(e,"VIP sent Frame origin/component differs");
    return vip_port_->host_sent(origin,frame.revision,*frame.vips,e);
}
bool Bridge::acknowledge_vips(uint64_t peer,uint64_t revision,std::string& e){
    if(!vip_port_){e.clear();return true;}
    if(!vip_origin_||peer>UINT32_MAX)return fail(e,"VIP ACK bound origin/peer absent");
    return vip_port_->host_acknowledge(*vip_origin_,uint32_t(peer),revision,e);
}
bool Bridge::admit_vips(const Frame& frame,std::string& e){
    if(!vip_port_){e.clear();return true;}
    if(!frame.vips)return fail(e,"armed VIP Frame component missing");
    const vip::Origin origin{frame.session,frame.epoch,frame.source_generation,frame.snapshot};
    if(!vip_origin_){
        if(!vip_client_.bind(origin,e))return false;
        vip_origin_=origin;
    }
    std::vector<vip::Transition> work;
    if(!prepare_vips(frame,work,e))return false;
    if(queued_&&queued_->vips){
        const auto& prior=*queued_->vips;const auto& next=*frame.vips;
        // A superseding Frame must retain every not-yet-applied transition.
        // A transport receipt cannot advance the Host's acknowledged floor.
        for(const auto& row:prior.rows){
            if(row.sequence<=vip_client_.committed())continue;
            if(row.sequence<=next.after||row.sequence-next.after>next.rows.size()||
               next.rows[size_t(row.sequence-next.after-1)]!=row)
                return fail(e,"VIP queued transition lost/conflicting during Frame supersession");
        }
    }
    e.clear();return true;
}
bool Bridge::prepare_vips(const Frame& frame,std::vector<vip::Transition>& work,std::string& e){
    work.clear();
    if(!vip_port_){if(frame.vips)return fail(e,"VIP Frame requires native UI consumer before full-frame ACK");e.clear();return true;}
    const vip::Origin origin{frame.session,frame.epoch,frame.source_generation,frame.snapshot};
    if(!vip_origin_||origin!=*vip_origin_||!frame.vips)return fail(e,"VIP Apply exact origin/component differs");
    return vip_client_.prepare(origin,*frame.vips,work,e);
}
bool Bridge::apply_vips(const Frame& frame,const std::vector<vip::Transition>& work,bool& attempted,std::string& e){
    attempted=false;if(!vip_port_){e.clear();return true;}
    if(!vip_origin_||!frame.vips)return fail(e,"VIP native Apply bound origin/component absent");
    VipApplyReceipt native;bool applied=false;
    auto retain=[&]{
        attempted=attempted||native.attempted;
        receipt_.attempted=receipt_.attempted||attempted;
        vip_receipt_={{"session",frame.session},{"snapshot_uuid",frame.snapshot},{"epoch",frame.epoch},
            {"source_generation",frame.source_generation},{"frame_revision",frame.revision},
            {"planned_count",work.size()},{"applied_count",native.applied.size()},
            {"planned_rows",vip::encode(vip::Batch{0,work}).at("rows")},
            {"applied_rows",vip::encode(vip::Batch{0,native.applied}).at("rows")},{"attempted",native.attempted},
            {"native_confirmed",native.confirmed},{"confirmed",false},{"unknown",native.unknown},{"cause",native.cause}};
    };
    if(!vip_port_->client_preflight(*vip_origin_,work,e)){
        if(e.empty())e="VIP native preflight failed without confirmation";
        native.cause=e;retain();
        locked_=receipt_.attempted;receipt_.unknown=locked_;receipt_.confirmed=false;receipt_.cause=e;
        vip_receipt_["unknown"]=locked_;return false;
    }
    try{applied=vip_port_->client_apply(*vip_origin_,work,native,e);}
    catch(...){native.unknown=native.unknown||native.attempted;if(native.cause.empty())native.cause=e;retain();throw;}
    retain();
    if(!applied||!native.confirmed||native.unknown||native.applied!=work){
        if(e.empty())e=native.cause.empty()?"VIP native Apply receipt incomplete/conflicting":native.cause;
        locked_=receipt_.attempted||native.unknown;receipt_.unknown=locked_;receipt_.confirmed=false;receipt_.cause=e;
        vip_receipt_["unknown"]=locked_;vip_receipt_["cause"]=e;return false;
    }
    if(!vip_client_.confirm(*vip_origin_,*frame.vips,native.applied,e)){
        locked_=true;receipt_.unknown=true;receipt_.confirmed=false;receipt_.cause=e;
        vip_receipt_["unknown"]=true;vip_receipt_["cause"]=e;return false;
    }
    vip_receipt_["confirmed"]=true;e.clear();return true;
}
bool Bridge::suspend_vips(std::string& e){
    if(!vip_port_){e.clear();return true;}
    const bool ok=vip_port_->suspend(context_,binding_,common_epoch_,vip_origin_,e);
    if(!ok){
        if(e.empty())e="VIP native cleanup failed without confirmation";
        locked_=true;receipt_.unknown=true;receipt_.confirmed=false;if(receipt_.cause.empty())receipt_.cause=e;
    }
    vip_cleanup_={{"confirmed",ok},{"unknown",!ok},{"cause",e}};return ok;
}
bool Bridge::reset_vips(const Context& c,const Binding& b,uint64_t epoch,std::string& e){
    if(!vip_port_){e.clear();return true;}
    if(vip_origin_&&!vip_port_->reset(*vip_origin_,e))return false;
    if(!vip_port_->bind(c,b,epoch,e))return false;
    vip_client_.reset();vip_origin_.reset();vip_receipt_=vip_cleanup_=nullptr;
    if(c.role==action_session::Role::Host)vip_origin_=vip::Origin{c.session,epoch,b.native_generation,c.snapshot_uuid};
    e.clear();return true;
}

}

namespace mt2mp::game::live_host_state {
bool Bridge::apply_marketing(const Frame& frame,std::string& e){
    apply_timing::Scope measured(apply_timing::Part::ClientMarketingApply);
    if(!marketing_port_&&!frame.marketing)return true;
    MarketingReceipt result;
    if(!marketing_port_||!frame.marketing){e="Marketing complete component/provider absent";}
    else {
        try{if(marketing_port_->apply(context_,binding_,frame,result,e)&&result.confirmed&&!result.unknown){receipt_.attempted=receipt_.attempted||result.attempted;return true;}}
        catch(const std::exception& x){e=std::string("Marketing native apply exception: ")+x.what();result.unknown=true;}
        catch(...){e="Marketing native apply nonstandard exception";result.unknown=true;}
    }
    receipt_.attempted=receipt_.attempted||result.attempted;locked_=receipt_.attempted||result.unknown;
    receipt_.unknown=locked_;receipt_.confirmed=false;receipt_.cause=e.empty()?"Marketing full component not confirmed":e;
    return false;
}
}

namespace mt2mp::game::live_host_state {
bool Bridge::apply_monsters(const Frame& frame,std::string& e){
    apply_timing::Scope measured(apply_timing::Part::ClientMonstersApply);
    if(!monster_port_&&!frame.monsters)return true;
    monsters::Receipt result;bool returned=false;
    if(!monster_port_||!frame.monsters)e="Monster complete component/provider absent at final Apply";
    else {
        try{returned=monster_port_->apply(context_,binding_,frame,result,e);}
        catch(const std::exception& x){e=std::string("Monster final Apply exception: ")+x.what();result.unknown=result.attempted;}
        catch(...){e="Monster final Apply nonstandard exception";result.unknown=result.attempted;}
    }
    receipt_.attempted=receipt_.attempted||result.attempted;
    if(returned&&result.confirmed&&!result.unknown)return true;
    locked_=receipt_.attempted||result.unknown;receipt_.unknown=locked_;receipt_.confirmed=false;
    receipt_.cause=e.empty()?(result.cause.empty()?"Monster full motion component not confirmed":result.cause):e;e=receipt_.cause;return false;
}
}

namespace mt2mp::game::live_host_state {
bool Bridge::apply_management(const Frame& frame,std::string& e){
    apply_timing::Scope measured(apply_timing::Part::ClientManagementApply);
    if(!management_port_&&!frame.management)return true;
    ManagementReceipt result;bool returned=false;
    if(!management_port_||!frame.management)e="Management complete component/provider absent";
    else try{returned=management_port_->apply(context_,binding_,frame,result,e);}
    catch(const std::exception& x){e=std::string("Management native Apply exception: ")+x.what();result.unknown=result.attempted;}
    catch(...){e="Management native Apply nonstandard exception";result.unknown=result.attempted;}
    receipt_.attempted=receipt_.attempted||result.attempted;
    receipt_.attempted_stores+=result.attempted_stores;receipt_.completed_stores+=result.stores;
    if(returned&&result.confirmed&&!result.unknown)return true;
    locked_=receipt_.attempted||result.unknown;receipt_.unknown=locked_;receipt_.confirmed=false;
    receipt_.cause=e.empty()?(result.cause.empty()?"Management full component not confirmed":result.cause):e;e=receipt_.cause;return false;
}
}

namespace mt2mp::game::live_host_state {
bool Bridge::apply_release(const Frame& frame,std::string& e){
    apply_timing::Scope measured(apply_timing::Part::ClientReleaseApply);
    if(!release_port_&&!frame.release)return true;ReleaseReceipt result;bool returned=false;
    if(!release_port_||!frame.release)e="Release complete component/provider absent";
    else try{returned=release_port_->apply(context_,binding_,frame,result,e);}
    catch(const std::exception& x){e=std::string("Release native Apply exception: ")+x.what();result.unknown=result.attempted;}
    catch(...){e="Release native Apply nonstandard exception";result.unknown=result.attempted;}
    receipt_.attempted=receipt_.attempted||result.attempted;if(returned&&result.confirmed&&!result.unknown)return true;
    locked_=receipt_.attempted||result.unknown;receipt_.unknown=locked_;receipt_.confirmed=false;receipt_.cause=e.empty()?(result.cause.empty()?"Release full component not confirmed":result.cause):e;e=receipt_.cause;return false;
}
}

namespace mt2mp::game::live_host_state {
bool Bridge::bind_parties(PartyPort& value,std::string& e){
    if(party_port_||!economy_||suspended_||locked_||host_revision_||last_||queued_||host_before_)
        return fail(e,"Party provider requires pristine unsuspended economic Frame owner");
    if(!value.bind(context_,binding_,common_epoch_,e)||!economy_->bind_parties(value,e))return false;
    party_port_=&value;e.clear();return true;
}
}
