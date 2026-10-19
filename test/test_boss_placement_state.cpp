#include "typed_action/action_session.h"
#include "boss_placement_state.h"
#include <iostream>
namespace {
namespace a=mt2mp::action_session;namespace state=mt2mp::game::boss_placement_state;namespace actors=mt2mp::game::interventions::actors;
const std::string uuid="01234567-89ab-4cde-8fab-0123456789ab";
int checks=0,failures=0;void need(bool value,const char* why){++checks;if(!value){++failures;std::cerr<<why<<'\n';}}
a::BossPlacementIntent intent(){a::BossPlacementIntent out;out.type=4;out.region=17;out.character_sha=std::string(64,'a');out.pose={0,0,0,0x3f800000,0,0,0,0x3f800000,0x3f800000,0x3f800000};return out;}
actors::Row row(const std::string& key){actors::Row out;out.target={mt2mp::game::interventions::Family::Boss,key};out.slot=3;out.type=4;out.pose.region=17;out.pose.orientation={0,0,0,1};out.pose.scale={1,1,1};out.lifecycle=actors::Boss{};return out;}
}
int main(){
    const auto placement=intent();std::string error;
    a::BossPlacementState before,after;const auto created=row(state::key(uuid));
    need(state::split({},state::key(uuid),before,error),"empty complete current Actor census captured");
    need(state::split({created},state::key(uuid),after,error),"one actual canonical created Actor split");after.record_sha=std::string(64,'b');
    need(state::effect(placement,uuid,before,after),"native one-birth effect valid");
    auto wrong=after;wrong.created->type++;need(!state::effect(placement,uuid,before,wrong),"different original Character ID rejected");
    wrong=after;wrong.created->pose.region++;need(!state::effect(placement,uuid,before,wrong),"different actual Region rejected");
    wrong=after;wrong.created->pose.scale[0]=2;need(!state::effect(placement,uuid,before,wrong),"different full native pose rejected");
    wrong=after;wrong.created->target.key=state::key("11234567-89ab-4cde-8fab-0123456789ab");need(!state::effect(placement,uuid,before,wrong),"foreign birth action alias rejected");
    wrong=after;wrong.record_sha.clear();need(!state::valid(wrong),"present actor requires complete original Record");
    auto survivor=row(state::key("21234567-89ab-4cde-8fab-0123456789ab"));actors::State full{created,survivor};
    need(state::split(full,state::key(uuid),after,error),"complete survivors retained");after.record_sha=std::string(64,'b');
    auto host=after;host.created->values[0]=std::bit_cast<uint32_t>(50.f);host.created->clips[0]={std::bit_cast<uint32_t>(2.f),std::bit_cast<uint32_t>(1.f),false,std::bit_cast<uint32_t>(1.f)};
    actors::State target;need(state::adoption(full,host,target,error)&&target[0]==*host.created&&target[1]==survivor,"existing shared writer target replaces only actual new Boss row");
    full[1].values[1]=std::bit_cast<uint32_t>(1.f);need(!state::adoption(full,host,target,error)&&!error.empty(),"unrelated current actor changes cannot be hidden by adoption");
    a::Ledger money;money.cash=money.record_cash=2000;money.record_present=1;auto paid=money;
    paid.cash-=1000;paid.record_cash-=1000;paid.record_expense+=1000;++paid.line_counts[6];paid.line_income[6]-=1000;
    need(state::payment(money,paid),"original SubtractCash category-six ledger exact");
    auto corrupt=paid;corrupt.record_expense=0;corrupt.record_income=-1000;need(!state::payment(money,corrupt),"signed Undo AddCash is not original Purchase");
    corrupt=paid;++corrupt.line_counts[7];need(!state::payment(money,corrupt),"foreign ledger category rejected");
    corrupt=paid;++corrupt.record_cash;need(!state::payment(money,corrupt),"partial native debit rejected");
    money.record_present=0;need(!state::payment(money,paid),"missing actual business Day record rejected");
    std::cout<<checks<<" Boss complete-state checks, "<<failures<<" failures\n";return failures?1:0;
}
