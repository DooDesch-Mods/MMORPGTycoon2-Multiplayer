#include "typed_action/action_session.h"
#include "group_transition.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
namespace gt=mt2mp::game::group_transition;
void need(bool b,const std::string& e){if(!b)throw std::runtime_error(e);}
const std::string fixture_action_uuid="11111111-1111-4111-8111-111111111111",fixture_snapshot_uuid="22222222-2222-4222-8222-222222222222";
std::string key(unsigned n){return std::string(64,'a')+":world:building:"+std::to_string(n);}
std::array<uint32_t,10> pose(){return {0,0,0,0x3f800000,0,0,0,0x3f800000,0x3f800000,0x3f800000};}
GroupState before(){GroupState v;for(unsigned i=1;i<=4;++i)v.props.push_back({key(i),"",67,pose()});return v;}
GroupState apply(const GroupState& prior,const GroupIntent& intent){
    gt::Plan p;std::string e;need(gt::plan(prior,intent,p,e),e);auto out=prior;
    std::erase_if(out.groups,[&](const auto& g){return std::find(p.removed.begin(),p.removed.end(),g.key)!=p.removed.end();});
    for(auto& prop:out.props)if(std::find(p.added.begin(),p.added.end(),prop.key)!=p.added.end())prop.parent=intent.kind==GroupKind::Group?p.target:"";
    if(intent.kind==GroupKind::Group){if(p.create)out.groups.push_back({p.target,67,pose(),{}});
        auto& g=*std::find_if(out.groups.begin(),out.groups.end(),[&](const auto& v){return v.key==p.target;});for(const auto& k:p.added)g.members.push_back({k,pose()});}
    return out;
}
Message message(const GroupState& prior,const GroupIntent& intent){Message m;m.kind=Kind::HostResult;m.session=4;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    m.request.action=Action::EditGroup;m.request.peer=1;m.request.sequence=1;m.request.action_uuid=fixture_action_uuid;m.request.snapshot_uuid=fixture_snapshot_uuid;m.request.canonical_record_key="groups:"+fixture_action_uuid;m.request.group=intent;
    m.evidence.group_before=prior;m.evidence.group_after=apply(prior,intent);
    gt::Plan plan;std::string error;need(gt::plan(prior,intent,plan,error),error);
    if(plan.create){const auto target="group:action:"+m.request.action_uuid;
        for(auto& group:m.evidence.group_after->groups)if(group.key==plan.target)group.key=target;
        for(auto& prop:m.evidence.group_after->props)if(prop.parent==plan.target)prop.parent=target;}
    m.evidence.undo_confirmed=m.evidence.selection_confirmed=m.evidence.retirement_confirmed=true;return m;}
int main()try{
    unsigned n=0;std::string e;std::vector<uint8_t> bytes;Message decoded;
    auto check=[&](const Message& m){need(encode(m,bytes,e),e);need(decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,e);need(group_wire_fits(m.request,*m.evidence.group_before,m.evidence.before,e),e);n+=3;};
    auto m=message(before(),{GroupKind::Group,{key(1),key(2)}});check(m);
    const auto first=*m.evidence.group_after;const auto stable=first.groups[0].key;
    auto append=message(first,{GroupKind::Group,{stable,key(3)}});check(append);
    auto pair=apply(first,{GroupKind::Group,{key(3),key(4)}});auto merge=message(pair,{GroupKind::Group,{"group:"+key(3),stable}});check(merge);
    auto split=message(pair,{GroupKind::Ungroup,{stable,"group:"+key(3)}});check(split);
    check(message(pair,{GroupKind::Ungroup,{"group:"+key(3)}}));
    auto rejected=[&](auto edit){auto bad=m;edit(bad);need(!encode(bad,bytes,e),"invalid Group result accepted");++n;};
    rejected([](auto& v){v.request.group.selected.push_back(key(1));});
    rejected([](auto& v){v.evidence.group_after->props[0].pose[4]=1;});
    rejected([](auto& v){v.evidence.group_after->props[0].parent.clear();});
    rejected([](auto& v){v.evidence.group_after->props[0].region=68;});
    rejected([](auto& v){v.evidence.group_after->groups[0].members[1].key=key(3);});
    rejected([](auto& v){std::swap(v.evidence.group_after->groups[0].members[0],v.evidence.group_after->groups[0].members[1]);});
    rejected([](auto& v){v.evidence.group_after->random[0]=1;});
    rejected([](auto& v){v.evidence.group_after->groups[0].pose[0]=0x7fc00000;});
    rejected([](auto& v){v.evidence.group_after->groups[0].members[0].relative[0]=0x7f800000;});
    rejected([](auto& v){v.evidence.group_after->groups[0].key="group:"+key(3);});
    rejected([](auto& v){v.evidence.after.cash=1;});
    rejected([](auto& v){v.evidence.undo_confirmed=false;});
    rejected([](auto& v){v.evidence.selection_confirmed=false;});
    rejected([](auto& v){v.request.district_title.position[0]=1;});
    rejected([](auto& v){v.request.canonical_record_key="groups:"+fixture_snapshot_uuid;});
    auto foreign=merge;foreign.evidence.group_after->props.pop_back();need(!encode(foreign,bytes,e),"missing surviving Scenery accepted");++n;
    auto survivor=message(pair,{GroupKind::Ungroup,{stable}});survivor.evidence.group_after->groups[0].pose[4]=1;need(!encode(survivor,bytes,e),"unrelated group changed");++n;
    need(encode(m,bytes,e),e);const auto j=nlohmann::json::parse(bytes);
    auto malformed=[&](auto edit){auto v=j;edit(v);const auto text=v.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,e),"malformed Group packet accepted");++n;};
    malformed([](auto& v){v["request"]["group"]["kind"]=1.0;});
    malformed([](auto& v){v["evidence"]["group_after"][0][0][3][0][1]=-1;});
    malformed([](auto& v){v["evidence"]["group_after"][0][0][3][0].push_back(0);});
    malformed([](auto& v){v["evidence"]["group_after"][2][0]=-1;});
    malformed([](auto& v){v["schema"]=34;});
    gt::Plan plan;need(!gt::plan(first,{GroupKind::Group,{key(1),key(3)}},plan,e),"grouped Scenery accepted as free target");++n;
    need(!gt::plan(first,{GroupKind::Ungroup,{key(3)}},plan,e),"ungroup without a Group accepted");++n;
    auto large=before();for(unsigned i=5;i<500;++i)large.props.push_back({key(i),"",67,pose()});std::sort(large.props.begin(),large.props.end(),[](const auto& a,const auto& b){return a.key<b.key;});
    need(!group_wire_fits(m.request,large,m.evidence.before,e),"oversized Group result admitted");++n;
    auto shrunk=first;shrunk.groups[0].members.erase(shrunk.groups[0].members.begin());shrunk.props.erase(shrunk.props.begin());
    need(gt::valid(shrunk,[](const auto& value){return value.starts_with("action:")||value.starts_with(std::string(64,'a'));})&&shrunk.groups[0].key==stable,
        "retained Group identity was tied to its current first member");++n;
    check(message(shrunk,{GroupKind::Ungroup,{stable}}));
    auto empty=shrunk;empty.groups[0].members.clear();std::erase_if(empty.props,[&](const auto& prop){return prop.parent==stable;});
    check(message(empty,{GroupKind::Group,{stable,key(3)}}));
    std::cout<<n<<" Group full ownership/order/codec/ledger/RNG/budget cases passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}
