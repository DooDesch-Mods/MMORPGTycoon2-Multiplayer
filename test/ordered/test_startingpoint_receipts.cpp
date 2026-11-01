#include "game/typed_action/action_session.h"
#include <iostream>
using json=nlohmann::json;
namespace action=mt2mp::action_session;
namespace tutorial_replication=mt2mp::game::tutorial_replication;
#include "startingpoint_receipt_ledger.inc"
struct Engine {
    #include "startingpoint_receipt_members.inc"
    #include "startingpoint_receipt_method.inc"
};
int count=0;void require(bool value){++count;if(!value)throw std::runtime_error("receipt check "+std::to_string(count));}
int main(){try{
    auto engine=std::make_unique<Engine>();require(engine->startingpoint_operations().empty());
    for(auto kind:{action::Action::StartStartingpoint,action::Action::SetStartingpointWorkers,action::Action::CommitStartingpoint}){
        auto op=std::make_unique<Engine::CommitOperation>();auto& q=op->request;
        q.action=kind;q.action_uuid="operation"+std::to_string(int(kind));q.snapshot_uuid="snapshot";q.peer=2;q.sequence=7;q.host_revision=9;
        q.canonical_record_key="tech:startingpoint";q.desired_workers=kind==action::Action::SetStartingpointWorkers?1:0;
        op->binding={3,4};op->ordered=true;op->evidence.before.cash=500;op->evidence.after.cash=100;
        op->evidence.startingpoint_before=tutorial_replication::startingpoint::Row{};
        op->evidence.startingpoint_after=tutorial_replication::startingpoint::Row{};op->evidence.startingpoint_after->purchased=true;
        engine->commits_.emplace(q.action_uuid,*op);auto row=engine->startingpoint_operations()[q.action_uuid];
        require(row["action_uuid"]==q.action_uuid&&row["action"]==int(kind));require(row["snapshot_uuid"]=="snapshot");
        require(row["peer"]==2&&row["sequence"]==7&&row["host_revision"]==9);require(row["native_epoch"]==3&&row["native_generation"]==4);
        require(row["desired_workers"]==q.desired_workers&&row["canonical_record_key"]==q.canonical_record_key);
        require(row["ordered"]==true&&row["confirmed"]==false&&row["retired"]==false);
        require(row["ledger_before"]["cash"]==500&&row["ledger_after"]["cash"]==100);
        require(row["before"]["purchased"]==false&&row["after"]["purchased"]==true);
        auto& retained=engine->commits_.at(q.action_uuid);retained.confirmed=true;retained.evidence.retirement_confirmed=true;
        row=engine->startingpoint_operations()[q.action_uuid];require(row["confirmed"]==true&&row["retired"]==true);
        require(row==engine->startingpoint_operations()[q.action_uuid]);
    }
    require(engine->startingpoint_operations().size()==3);
    auto unrelated=std::make_unique<Engine::CommitOperation>();unrelated->request.action=action::Action::StartResearch;
    engine->commits_.emplace("unrelated",*unrelated);require(engine->startingpoint_operations().size()==3);
    auto& pending=engine->commits_.at("operation19");pending.evidence.startingpoint_before.reset();pending.evidence.startingpoint_after.reset();
    auto row=engine->startingpoint_operations()["operation19"];require(row["before"].is_null()&&row["after"].is_null());
    require(!row.contains("original_calls")&&!row.contains("native_entered"));
    std::cout<<count<<" actual specialized receipt checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
