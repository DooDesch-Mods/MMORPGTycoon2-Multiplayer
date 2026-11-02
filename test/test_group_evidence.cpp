#define main group_protocol_fixture_main
#include "test_group_protocol.cpp"
#undef main
#include "group_history.h"
#include "typed_action/tick_evidence.h"
namespace tick=mt2mp::action_session::tick_commands;
Message history(const GroupState& prior,const GroupState& after){
    Message m;m.kind=Kind::HostResult;m.session=4;m.revision=2;m.required_mask=3;m.outcome=Outcome::Confirmed;
    m.request.action=Action::UndoBuilding;m.request.peer=2;m.request.sequence=1;m.request.action_uuid="33333333-3333-4333-8333-333333333333";
    m.request.snapshot_uuid=fixture_snapshot_uuid;m.request.target_action_uuid=fixture_action_uuid;m.request.canonical_record_key="groups:"+fixture_action_uuid;
    m.evidence.group_before=mt2mp::game::group_history::canonical(prior);m.evidence.group_after=mt2mp::game::group_history::canonical(after);
    m.evidence.undo_confirmed=m.evidence.retirement_confirmed=true;return m;
}
std::string digest(const Message& m){
    Context context;context.session=m.session;context.snapshot_uuid=m.request.snapshot_uuid;context.peer_mask=m.required_mask;
    tick::Assigned assigned{m.revision,{m.session,m.request.snapshot_uuid,m.request}};
    tick::ExecutionReceipt receipt;receipt.command=assigned;receipt.outcome=Outcome::Confirmed;receipt.evidence=std::make_shared<Evidence>(m.evidence);
    std::string error,value;need(tick::canonical_evidence(context,assigned,receipt,value,error),error);return value;
}
int main()try{
    auto group=message(before(),{GroupKind::Group,{key(1),key(2)}});
    const auto baseline=digest(group);
    auto advanced=group;advanced.evidence.group_before->random[0]++;advanced.evidence.group_after->random[0]++;
    const auto advanced_digest=digest(advanced);need(advanced_digest!=baseline,"different fresh execution RNG must change canonical digest");
    auto config=[](uint64_t peer){tick::Config c;c.context={77,peer,1,3,peer==1?Role::Host:Role::Client,"snapshot-A"};return c;};
    Request request;request.peer=2;request.sequence=1;request.action=Action::EditGroup;request.action_uuid="action";request.snapshot_uuid="snapshot-A";request.canonical_record_key="groups:action";
    tick::Input input{77,"snapshot-A",request};auto valid=[](const tick::Input&,std::string&){return true;};
    tick::Queue host(config(1),valid),client(config(2),valid);tick::Assigned assigned;tick::Batch batch;tick::Receipt h,c;std::string queue_error;
    need(host.offer(2,input,assigned,queue_error)==tick::Admission::Accepted&&host.seal(batch,queue_error)&&client.admit(1,batch,queue_error)==tick::Admission::Accepted,queue_error);
    auto current=[](std::string&){return true;};auto advance=[](uint64_t,std::string&){return Outcome::Confirmed;};
    auto consume=[&](tick::Queue& queue,tick::Receipt& receipt,const std::string& value){return queue.consume(1,current,[&](const auto& command,std::string& error){return queue.bind_evidence(command,value,error)?Outcome::Confirmed:Outcome::Unknown;},advance,receipt,queue_error);};
    need(consume(host,h,baseline)&&consume(client,c,advanced_digest),queue_error);
    need(!host.acknowledge(2,c,queue_error)&&host.state().locked,"different valid execution RNG digests must lock the ordered ACK path");
    auto different=[&](auto change){auto value=group;change(value);need(digest(value)!=baseline,"changed complete evidence produced same digest");};
    for(unsigned i=0;i<4;++i)different([&](auto& m){m.evidence.group_before->random[i]++;m.evidence.group_after->random[i]++;});
    different([](auto& m){m.evidence.before.cash++;m.evidence.after.cash++;});
    different([](auto& m){m.evidence.group_before->props[0].pose[4]=0x3f800000;m.evidence.group_after->props[0].pose[4]=0x3f800000;});
    different([](auto& m){m.evidence.group_after->groups[0].members[0].relative[4]=0x3f800000;});
    auto swapped=message(before(),{GroupKind::Group,{key(2),key(1)}});need(digest(swapped)!=baseline,"member order digest equal");
    auto other=message(before(),{GroupKind::Group,{key(1),key(3)}});need(digest(other)!=baseline,"member key and parent digest equal");
    auto changed=before();for(auto& p:changed.props)p.region=68;
    auto region=message(changed,{GroupKind::Group,{key(1),key(2)}});region.evidence.group_after->groups[0].region=68;
    need(digest(region)!=baseline,"Region digest equal");
    auto first=apply(before(),{GroupKind::Group,{key(1),key(2)}});
    auto undo=history(first,before());auto undo_digest=digest(undo);
    undo.evidence.group_before->random[2]++;undo.evidence.group_after->random[2]++;
    need(digest(undo)!=undo_digest,"history RNG digest equal");
    std::cout<<"11 Group and history complete canonical digest cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
