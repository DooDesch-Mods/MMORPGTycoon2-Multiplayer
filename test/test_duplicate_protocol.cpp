#include "typed_action/action_session.h"
#include "duplicate_transition.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
void need(bool okay,const std::string& e){if(!okay)throw std::runtime_error(e);}
const std::string fixture_action_uuid="11111111-1111-4111-8111-111111111111",fixture_snapshot_uuid="22222222-2222-4222-8222-222222222222";
std::string key(unsigned n){return std::string(64,'a')+":world:building:"+std::to_string(n);}
std::string fresh(unsigned n){auto s=fixture_action_uuid;s[0]=char('2'+n);return "action:"+s;}
std::array<uint32_t,10> pose(){return {0,0,0,0x3f800000,0,0,0,0x3f800000,0x3f800000,0x3f800000};}
Message message(bool group=false){
    Message m;m.kind=Kind::HostResult;m.session=4;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    auto& r=m.request;r.action=Action::DuplicateSelection;r.peer=1;r.sequence=1;r.action_uuid=fixture_action_uuid;r.snapshot_uuid=fixture_snapshot_uuid;r.canonical_record_key="duplicate:"+fixture_action_uuid;
    DuplicateTarget target;target.key=group?"group:"+key(1):key(1);target.record_sha=std::string(64,'b');target.family=group?DuplicateFamily::Group:DuplicateFamily::Building;
    target.region=67;target.cost=10;target.before=target.after=pose();target.after[4]=0x3f800000;target.created_keys={fresh(0)};
    if(group){target.members={key(1),key(2)};target.created_keys.push_back(fresh(1));}r.duplicate.rows={target};
    DuplicateState before;before.untouched_sha=std::string(64,'c');before.sources={{target.key,"",target.record_sha,target.family,67,target.before,target.members}};
    auto after=before;after.random[0]=123;
    after.created.push_back({group?"group:"+fresh(0):fresh(0),"",std::string(64,'d'),target.family,68,target.after,group?target.created_keys:std::vector<std::string>{}});
    if(group)for(const auto& k:target.created_keys)after.created.push_back({k,after.created.front().key,std::string(64,'e'),DuplicateFamily::Scenery,68,pose(),{}});
    else after.release[1]=1;
    m.evidence.duplicate_before=before;m.evidence.duplicate_after=after;
    m.evidence.before.record_present=1;m.evidence.before.cash=m.evidence.before.record_cash=100;m.evidence.after=m.evidence.before;
    m.evidence.after.cash-=10;m.evidence.after.record_cash-=10;m.evidence.after.record_expense+=10;m.evidence.after.line_counts[6]=1;m.evidence.after.line_income[6]=-10;
    m.evidence.undo_confirmed=m.evidence.selection_confirmed=m.evidence.retirement_confirmed=true;return m;
}
int main()try{
    unsigned count=0;std::string e;std::vector<uint8_t> bytes;Message decoded;
    for(bool group:{false,true}){const auto m=message(group);need(encode(m,bytes,e),e);need(decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,e);
        need(duplicate_wire_fits(m.request,*m.evidence.duplicate_before,m.evidence.before,e),e);count+=3;}
    const auto m=message();auto bad=[&](auto change){auto v=m;change(v);need(!encode(v,bytes,e),"invalid Duplicate result accepted");++count;};
    bad([](auto& v){v.request.duplicate.rows[0].created_keys[0]=key(2);});
    bad([](auto& v){v.request.duplicate.rows.push_back(v.request.duplicate.rows[0]);});
    bad([](auto& v){v.request.duplicate.rows[0].cost=-1;});
    bad([](auto& v){v.evidence.duplicate_after->sources[0].record_sha=std::string(64,'e');});
    bad([](auto& v){v.evidence.duplicate_after->untouched_sha=std::string(64,'f');});
    bad([](auto& v){v.evidence.duplicate_after->created[0].pose[4]=0;});
    bad([](auto& v){v.evidence.duplicate_after->created[0].record_sha.clear();});
    bad([](auto& v){v.evidence.duplicate_after->created[0].parent="group:"+key(4);});
    bad([](auto& v){v.evidence.duplicate_after->release[1]=0;});
    bad([](auto& v){v.evidence.duplicate_after->release[2]=1;});
    bad([](auto& v){v.evidence.after.line_income[6]=0;});
    bad([](auto& v){v.evidence.after.line_counts[0]=1;});
    bad([](auto& v){v.evidence.after.cash=100;});
    bad([](auto& v){v.evidence.undo_confirmed=false;});
    bad([](auto& v){v.evidence.selection_confirmed=false;});
    bad([](auto& v){v.request.group.selected={key(1)};});
    bad([](auto& v){v.request.canonical_record_key="duplicate:"+fixture_snapshot_uuid;});
    auto group=message(true);std::swap(group.evidence.duplicate_after->created[1],group.evidence.duplicate_after->created[2]);need(!encode(group,bytes,e),"Group original child order lost");++count;
    group=message(true);group.evidence.duplicate_after->created[1].region=67;need(!encode(group,bytes,e),"Group child reciprocal Region lost");++count;
    group=message(true);const auto retained="group:action:"+fixture_snapshot_uuid;
    group.request.duplicate.rows[0].key=retained;group.evidence.duplicate_before->sources[0].key=retained;
    group.evidence.duplicate_after->sources[0].key=retained;
    need(encode(group,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==group,
        "Duplicate rejected a Group whose retained identity differs from its current first member");count+=2;
    need(duplicate_wire_fits(group.request,*group.evidence.duplicate_before,group.evidence.before,e),e);++count;
    need(encode(m,bytes,e),e);const auto original=nlohmann::json::parse(bytes);
    auto malformed=[&](auto change){auto j=original;change(j);const auto text=j.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,e),"malformed Duplicate packet accepted");++count;};
    malformed([](auto& j){j["schema"]=34;});
    malformed([](auto& j){j["request"]["duplicate"][0][0][3]=67.0;});
    malformed([](auto& j){j["evidence"]["duplicate_after"][1][0][5][0]=-1;});
    malformed([](auto& j){j["evidence"]["duplicate_after"][1][0].push_back(0);});
    std::cout<<count<<" Duplicate full record/order/payment/codec cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
