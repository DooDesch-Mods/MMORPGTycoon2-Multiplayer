#include <memory>
namespace modular_protocol_fixture {
nlohmann::json modular_record(bool plan){using j=nlohmann::json;return j::array({j::array({int64_t(-1),size_t(0),j::array({1,uint32_t(0),plan?"mmoBuildingPlan":"mmoBuildingLayout"}),j::array()}),j::array({int64_t(0),size_t(0),j::array({1,uint32_t(0),"fixture-value"}),j::array({j::array({3,uint32_t(0x3f800000),""}),j::array({4,uint32_t(7),""})})})});}
std::unique_ptr<Message> modular_message(uint8_t mode){auto owned=std::make_unique<Message>();auto& m=*owned;m.kind=Kind::HostResult;m.session=4;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;auto& r=m.request;r.action=Action::EditModularBuilding;r.peer=1;r.sequence=1;r.action_uuid=fixture_action;r.snapshot_uuid=fixture_snapshot;r.canonical_record_key="modular:"+fixture_action;r.modular.mode=mode;r.modular.region=67;
    ModularState before;before.props_sha=std::string(64,'c');before.bugs_sha=std::string(64,'d');auto after=before;after.props_sha=std::string(64,'e');after.release[1]=1;
    ModularEffect result;result.untouched_sha=std::string(64,'f');result.dependency_sha={std::string(64,'6'),std::string(64,'7'),std::string(64,'6'),std::string(64,'7')};auto prior=object(old(1),DuplicateFamily::Building);auto next=prior;next.record_sha=std::string(64,'e');
    if(mode){r.modular.target=prior.key;r.modular.before_record_sha=prior.record_sha;result.changed.push_back({prior,next});}
    else{r.modular.theme="Medieval";r.modular.pose=pose();r.modular.layout=modular_record(false);r.modular.plan=modular_record(true);r.modular.clearance={0,0,0,0x3f800000,0x40000000,0x3f800000};result.created={object("action:"+fixture_action,DuplicateFamily::Building)};result.removed={object(old(2))};result.cash={{10,{}}};}
    auto& e=m.evidence;e.modular_before=before;e.modular_after=after;e.modular_result=result;e.before.record_present=1;e.before.cash=e.before.record_cash=100;e.after=e.before;if(!mode){e.after.cash=e.after.record_cash=90;e.after.record_expense=10;e.after.line_counts[6]=1;e.after.line_income[6]=-10;}e.undo_confirmed=e.selection_confirmed=e.retirement_confirmed=true;return owned;
}
unsigned modular_protocol_cases(){unsigned count=0;std::string error;std::vector<uint8_t> bytes;auto decoded=std::make_unique<Message>();const auto empty=std::make_unique<Evidence>();
    const auto roundtrip=[&](const Message& value){need(encode(value,bytes,error)&&decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==value,error);const auto j=nlohmann::json::parse(bytes);need(j.at("schema")==36,"Modular new action did not use schema36");count+=2;};
    for(uint8_t mode=0;mode<5;++mode){const auto original=modular_message(mode);roundtrip(*original);
        auto request=std::make_unique<Message>(*original);request->kind=Kind::Request;request->revision=0;request->required_mask=0;request->outcome=Outcome::Pending;request->evidence=*empty;roundtrip(*request);
        auto ack=std::make_unique<Message>(*original);ack->kind=Kind::ApplyAck;ack->ack_peer=2;roundtrip(*ack);auto complete=std::make_unique<Message>(*original);complete->kind=Kind::Completion;roundtrip(*complete);
        auto bad=std::make_unique<Message>(*original);bad->request.modular.mode=5;need(!encode(*bad,bytes,error),"Modular mode5 accepted");++count;
        *bad=*original;bad->request.scenery_stroke.region=67;need(!encode(*bad,bytes,error),"Modular accepted another action payload");++count;
        *bad=*original;bad->evidence.modular_after->release[1]=0;need(!encode(*bad,bytes,error),"Modular missing original Release edit accepted");++count;
        need(encode(*original,bytes,error),error);auto j=nlohmann::json::parse(bytes);j["schema"]=35;auto raw=j.dump();need(!decode(reinterpret_cast<const uint8_t*>(raw.data()),raw.size(),*decoded,error),"Modular59 accepted legacy35");++count;
        j=nlohmann::json::parse(bytes);j["request"]["action"]=57;raw=j.dump();need(!decode(reinterpret_cast<const uint8_t*>(raw.data()),raw.size(),*decoded,error),"Modular payload accepted as District57");++count;
        j=nlohmann::json::parse(bytes);j["request"]["modular"][1]=67.0;raw=j.dump();need(!decode(reinterpret_cast<const uint8_t*>(raw.data()),raw.size(),*decoded,error),"Modular Region float accepted");++count;
    }
    auto bad=modular_message(0);bad->request.modular.plan=bad->request.modular.layout;need(!encode(*bad,bytes,error),"Layout root accepted as Plan");++count;
    bad=modular_message(0);bad->request.modular.layout[1][0]=int64_t(1);need(!encode(*bad,bytes,error),"Record self-parent accepted");++count;
    bad=modular_message(0);bad->request.modular.layout[1][1]=size_t(1);need(!encode(*bad,bytes,error),"Record missing ordered child accepted");++count;
    bad=modular_message(0);bad->request.modular.clearance[0]=0x40000000;need(!encode(*bad,bytes,error),"Reversed clearance accepted");++count;
    bad=modular_message(1);bad->evidence.modular_result->changed[0][1].key=old(9);need(!encode(*bad,bytes,error),"Modular changed canonical owner accepted");++count;
    bad=modular_message(1);bad->evidence.modular_result->changed[0][0].record_sha=std::string(64,'9');need(!encode(*bad,bytes,error),"Modular stale before Record accepted");++count;
    for(bool prefab:{false,true})for(bool undo:{false,true}){auto history=modular_message(prefab?0:1);history->request.action=undo?Action::UndoBuilding:Action::RedoBuilding;history->request.target_action_uuid=fixture_action;history->request.action_uuid=fixture_snapshot;history->request.modular={};history->evidence.modular_after->release=history->evidence.modular_before->release;
        if(prefab){auto& result=*history->evidence.modular_result;if(undo)std::swap(result.created,result.removed);result.cash={{undo?10:-10,{}}};history->evidence.before={};history->evidence.before.record_present=1;history->evidence.before.cash=history->evidence.before.record_cash=100;history->evidence.after=history->evidence.before;history->evidence.after.cash+=undo?10:-10;history->evidence.after.record_cash+=undo?10:-10;history->evidence.after.record_income=undo?10:-10;history->evidence.after.line_counts[6]=1;history->evidence.after.line_income[6]=undo?10:-10;}
        roundtrip(*history);*bad=*history;bad->request.canonical_record_key="district-edit:"+fixture_action;need(!encode(*bad,bytes,error),"Modular history escaped canonical family");++count;
        *bad=*history;bad->evidence.modular_after->release[1]=7;need(!encode(*bad,bytes,error),"History replayed Release side effect");++count;
        if(prefab){*bad=*history;bad->evidence.modular_result->cash[0].amount*=-1;need(!encode(*bad,bytes,error),"Modular history wrong signed AddCash accepted");++count;}
        history->kind=Kind::Request;history->revision=0;history->required_mask=0;history->outcome=Outcome::Pending;history->evidence=*empty;roundtrip(*history);
    }
    auto unknown=modular_message(1);unknown->kind=Kind::ApplyAck;unknown->ack_peer=2;unknown->outcome=Outcome::Unknown;unknown->cause="original edit did not confirm";roundtrip(*unknown);*bad=*unknown;bad->evidence.modular_result->untouched_sha="invalid";need(!encode(*bad,bytes,error),"Unknown accepted malformed retained evidence");++count;
    return count;
}
}

unsigned modular_protocol_cases(){return modular_protocol_fixture::modular_protocol_cases();}
