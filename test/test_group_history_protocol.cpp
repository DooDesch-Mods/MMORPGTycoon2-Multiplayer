#define main group_protocol_fixture_main
#include "test_group_protocol.cpp"
#undef main
#include "group_history.h"
namespace gh=mt2mp::game::group_history;
Message history(const GroupState& prior,const GroupState& after,Action direction=Action::UndoBuilding){
    Message m;m.kind=Kind::HostResult;m.session=4;m.revision=2;m.required_mask=3;m.outcome=Outcome::Confirmed;
    m.request.action=direction;m.request.peer=2;m.request.sequence=1;m.request.action_uuid="33333333-3333-4333-8333-333333333333";
    m.request.snapshot_uuid=fixture_snapshot_uuid;m.request.target_action_uuid=fixture_action_uuid;m.request.canonical_record_key="groups:"+fixture_action_uuid;
    m.evidence.group_before=gh::canonical(prior);m.evidence.group_after=gh::canonical(after);m.evidence.undo_confirmed=m.evidence.retirement_confirmed=true;return m;
}
int main()try{
    unsigned count=0;std::string e;std::vector<uint8_t> bytes;Message decoded;
    const auto free=before(),one=apply(free,{GroupKind::Group,{key(1),key(2)}}),two=apply(one,{GroupKind::Group,{key(3),key(4)}});
    auto remove=[&](const GroupRow& g){std::vector<std::string> members;for(const auto& m:g.members)members.push_back(m.key);return gh::Node{gh::Kind::Remove,g.key,g.region,g.pose,members};};
    std::vector<gh::Node> nodes{remove(two.groups[0]),remove(two.groups[1])};GroupState ungrouped;
    need(gh::project(two,nodes,ungrouped,e)&&ungrouped==free,e);++count;
    need(gh::effect(two,ungrouped,nodes,e),e);++count;
    auto restored_nodes=nodes;for(auto& n:restored_nodes)n.kind=gh::Kind::Add;
    auto restored=two;std::reverse(restored.groups.begin(),restored.groups.end());restored.groups[0].pose[4]=0x3f800000;
    restored.groups[0].members[0].relative[4]=0xbf800000;
    need(gh::effect(free,restored,restored_nodes,e)&&gh::same_nodes(nodes,restored_nodes,true),e);++count;
    auto wire=[&](const Message& m){need(encode(m,bytes,e),e);need(decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,e);
        need(group_history_wire_fits(m.request,*m.evidence.group_before,*m.evidence.group_after,m.evidence.before,e),e);count+=3;};
    wire(history(two,free));wire(history(free,restored,Action::RedoBuilding));
    // Original merge Undo releases only appended members. It does not restore
    // secondary Groups flattened by the preceding original GroupSelection.
    const auto merged=apply(two,{GroupKind::Group,{two.groups[0].key,two.groups[1].key}});
    const std::vector<gh::Node> merge_inverse{{gh::Kind::Remove,two.groups[0].key,67,pose(),{key(3),key(4)}}};GroupState merge_undo;
    need(gh::project(merged,merge_inverse,merge_undo,e)&&merge_undo.groups.size()==1,e);
    need(gh::effect(merged,merge_undo,merge_inverse,e)&&!gh::effect(merged,two,merge_inverse,e),"merge inverse fabricated secondary Group");count+=2;
    wire(history(merged,merge_undo));
    auto reject_effect=[&](auto change){auto changed=ungrouped;change(changed);need(!gh::effect(two,changed,nodes,e),"invalid original history effect accepted");++count;};
    reject_effect([](auto& s){s.random[0]=1;});reject_effect([](auto& s){s.props[0].pose[4]=1;});
    reject_effect([](auto& s){s.props[0].region=68;});reject_effect([](auto& s){s.props[0].parent="group:"+key(1);});
    reject_effect([&](auto& s){s.groups.push_back(two.groups[0]);});
    auto changed_nodes=nodes;changed_nodes[0].members[0]=key(3);need(!gh::effect(two,ungrouped,changed_nodes,e),"foreign/duplicate member admitted");++count;
    auto m=history(two,free);auto invalid=[&](auto change){auto bad=m;change(bad);need(!encode(bad,bytes,e),"invalid Group history protocol accepted");++count;};
    invalid([](auto& v){v.evidence.after.cash=1;});invalid([](auto& v){v.evidence.undo_confirmed=false;});
    invalid([](auto& v){v.evidence.selection_confirmed=true;});invalid([](auto& v){v.evidence.target_retired=true;});
    invalid([](auto& v){v.evidence.group_after->props[0].pose[4]=1;});invalid([](auto& v){v.evidence.group_after=v.evidence.group_before;});
    invalid([](auto& v){v.request.canonical_record_key="groups:"+fixture_snapshot_uuid;});
    invalid([](auto& v){v.request.group.selected={key(1),key(2)};});invalid([](auto& v){v.evidence.group_after.reset();});
    need(encode(m,bytes,e),e);const auto encoded=nlohmann::json::parse(bytes);
    for(const auto schema:{10,34}){auto bad=encoded;bad["schema"]=schema;const auto data=bad.dump();need(!decode(reinterpret_cast<const uint8_t*>(data.data()),data.size(),decoded,e),"wrong history schema accepted");++count;}
    auto order=restored;order.groups[0].members[0].key=key(4);need(!gh::effect(free,order,restored_nodes,e),"original ordered member changed");++count;
    const std::vector<gh::Node> remove_first{{gh::Kind::Remove,one.groups[0].key,67,pose(),{key(1)}}};GroupState retained;
    need(gh::project(one,remove_first,retained,e)&&retained.groups[0].key==one.groups[0].key&&
        retained.groups[0].members[0].key==key(2)&&gh::effect(one,retained,remove_first,e),
        "original first-member removal changed retained Group identity");++count;
    std::cout<<count<<" Group history original node/merge/recenter/canonical order/codec cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
