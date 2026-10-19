#define main duplicate_protocol_fixture_main
#include "test_duplicate_protocol.cpp"
#undef main
#include "duplicate_history.h"
namespace dh=mt2mp::game::duplicate_history;
Message history(bool undo,bool group){
    const auto birth=message(group);Message m;m.kind=Kind::HostResult;m.session=4;m.revision=2;m.required_mask=3;m.outcome=Outcome::Confirmed;
    m.request.action=undo?Action::UndoBuilding:Action::RedoBuilding;m.request.peer=2;m.request.sequence=1;
    m.request.action_uuid="33333333-3333-4333-8333-333333333333";m.request.snapshot_uuid=fixture_snapshot_uuid;
    m.request.target_action_uuid=fixture_action_uuid;m.request.canonical_record_key="duplicate:"+fixture_action_uuid;
    auto live=*birth.evidence.duplicate_after;auto absent=live;absent.created.clear();
    m.evidence.duplicate_before=undo?live:absent;m.evidence.duplicate_after=undo?absent:live;
    m.evidence.before=m.evidence.after=birth.evidence.after;m.evidence.duplicate_cash=dh::cash(birth.request.duplicate,undo);
    const int32_t amount=undo?10:-10;auto& after=m.evidence.after;
    after.cash+=amount;after.record_cash+=amount;after.record_income+=amount;after.line_income[6]+=amount;after.line_counts[6]++;
    m.evidence.undo_confirmed=m.evidence.retirement_confirmed=true;return m;
}
int main()try{
    unsigned count=0;std::string e;std::vector<uint8_t> bytes;Message decoded;
    for(bool undo:{false,true})for(bool group:{false,true}){
        const auto m=history(undo,group);need(encode(m,bytes,e),e);need(decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,e);
        need(duplicate_history_wire_fits(m.request,*m.evidence.duplicate_before,*m.evidence.duplicate_after,m.evidence.duplicate_cash,m.evidence.before,e),e);count+=3;
        auto reject=[&](auto change){auto bad=m;change(bad);need(!encode(bad,bytes,e),"invalid Duplicate history evidence accepted");++count;};
        reject([](auto& v){v.evidence.after.record_expense++;});reject([](auto& v){v.evidence.after.record_income++;});
        reject([](auto& v){v.evidence.after.line_counts[6]--;});reject([](auto& v){v.evidence.after.line_income[6]++;});
        reject([](auto& v){v.evidence.duplicate_cash[0].amount=-v.evidence.duplicate_cash[0].amount;});
        reject([](auto& v){v.evidence.duplicate_cash.clear();});reject([](auto& v){v.evidence.duplicate_cash[0].position[0]=0x7fc00000;});
        reject([](auto& v){v.evidence.duplicate_after->untouched_sha=std::string(64,'f');});
        reject([](auto& v){v.evidence.duplicate_after->sources[0].record_sha=std::string(64,'f');});
        reject([](auto& v){v.evidence.duplicate_after->release[0]++;});
        reject([](auto& v){v.evidence.undo_confirmed=false;});reject([](auto& v){v.evidence.selection_confirmed=true;});
        reject([](auto& v){v.request.canonical_record_key="duplicate:"+fixture_snapshot_uuid;});
        reject([](auto& v){v.evidence.duplicate_after=v.evidence.duplicate_before;});
        need(encode(m,bytes,e),e);auto wrong_schema=nlohmann::json::parse(bytes);wrong_schema["schema"]=10;const auto text=wrong_schema.dump();
        need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,e),"Duplicate history schema10 accepted");++count;
    }
    const auto birth=message();auto many=birth.request.duplicate;many.rows.resize(10,many.rows[0]);
    need(dh::cash(many,true).size()==1&&dh::cash(many,true)[0].amount==100&&dh::cash(many,false)[0].amount==-100,"large selection original aggregate cash differs");++count;
    auto zero=history(true,false);zero.evidence.duplicate_cash[0].amount=0;zero.evidence.after=zero.evidence.before;zero.evidence.after.line_counts[6]++;
    need(encode(zero,bytes,e),e);++count;
    zero.evidence.after.line_counts[6]--;need(!encode(zero,bytes,e),"zero DoCash original AddCash line omitted");++count;
    auto negative=history(false,false);need(negative.evidence.after.record_income<negative.evidence.before.record_income&&
        negative.evidence.after.record_expense==negative.evidence.before.record_expense,"negative DoCash became expense");++count;
    auto overflow=history(true,false);overflow.evidence.before.cash=INT32_MAX;need(!encode(overflow,bytes,e),"signed AddCash overflow accepted");++count;
    for(bool undo:{false,true}){
        auto building=history(undo,false);DuplicateHistoryEffects witness{std::string(64,'1'),std::string(64,'2'),std::string(64,'3'),{key(1)}};
        building.evidence.duplicate_before->history=witness;building.evidence.duplicate_after->history=witness;
        building.evidence.duplicate_after->history->services_sha=std::string(64,'4');
        building.evidence.duplicate_after->history->quests_sha=std::string(64,'5');
        building.evidence.duplicate_after->history->related_sha=std::string(64,'6');
        building.evidence.duplicate_after->sources[0].record_sha=std::string(64,'7');
        need(encode(building,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==building,e);++count;
        need(duplicate_history_wire_fits(building.request,*building.evidence.duplicate_before,*building.evidence.duplicate_after,
            building.evidence.duplicate_cash,building.evidence.before,e),e);++count;
        auto reject=[&](auto change){auto invalid=building;change(invalid);need(!encode(invalid,bytes,e),"invalid bounded Building history witness admitted");++count;};
        reject([](auto& value){value.evidence.duplicate_after->history.reset();});
        reject([](auto& value){value.evidence.duplicate_after->history->related_keys.clear();});
        reject([](auto& value){value.evidence.duplicate_after->history->services_sha="missing";});
        reject([](auto& value){value.evidence.duplicate_before->history->related_keys.push_back(key(1));});
        reject([](auto& value){value.evidence.duplicate_after->sources[0].pose[4]=0x40000000;});
        auto unrelated=building;unrelated.evidence.duplicate_before->history->related_keys.clear();unrelated.evidence.duplicate_after->history->related_keys.clear();
        need(!encode(unrelated,bytes,e),"unrelated source record drift laundered through Building witness");++count;
    }
    auto placement=message();placement.evidence.duplicate_before->history=DuplicateHistoryEffects{std::string(64,'1'),std::string(64,'2'),std::string(64,'3'),{}};
    placement.evidence.duplicate_after->history=placement.evidence.duplicate_before->history;
    need(!encode(placement,bytes,e),"ordinary Duplicate placement accepted history-only effect witness");++count;
    std::cout<<count<<" Duplicate history actual codec/signed AddCash/order/full record cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
