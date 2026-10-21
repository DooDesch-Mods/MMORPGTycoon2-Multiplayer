#include "typed_action/dungeon_input_action.h"
#include <iostream>
#include <stdexcept>
namespace dungeon_action_fixture {
namespace a=mt2mp::action_session;
const std::string action="b7320a31-5464-4227-8284-f9ace596e29c";
const std::string other="01234567-89ab-cdef-0123-456789abcdef";
const std::string building="action:"+other;
unsigned checks=0;
void expect(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
struct Ledger {
    int32_t cash=100,flat_day=3,record_present=1,record_day=3,record_cash=100,record_income=0,record_expense=0;
    std::array<int32_t,20> line_counts{},line_income{};
    bool operator==(const Ledger&)const=default;
};
a::DungeonInputIntent intent(a::DungeonInputTool tool){
    a::DungeonInputIntent out;out.key=building;out.scene=building+"/template";out.before_sha256=std::string(64,'a');out.tool=tool;
    if(tool==a::DungeonInputTool::Monster){out.character_type=2;out.rank=0;out.level=1;}
    else {out.type="owned-type";out.variant="owned-variant";out.cost=10;}
    return out;
}
a::DungeonInputState state(){
    a::DungeonInputState out;out.dungeon_sha256=std::string(64,'a');out.scenes_sha256=std::string(64,'b');
    out.quests_sha256=std::string(64,'c');out.layout_sha256=std::string(64,'d');return out;
}
a::DungeonInputState changed(const a::DungeonInputState& before){
    auto out=before;out.dungeon_sha256=std::string(64,'e');out.scenes_sha256=std::string(64,'f');
    out.protected_state.dungeon.undo_count=1;return out;
}
a::DungeonInputBirth gizmo(const a::DungeonInputIntent& request){
    a::DungeonInputBirth row;row.key=mt2mp::game::dungeon_gizmo_identity::birth(action,0);row.scene=request.scene;row.tool=request.tool;
    row.gizmo.type=request.type;row.gizmo.variant=request.variant;row.gizmo.position=request.position;row.gizmo.rotation=request.rotation;row.gizmo.scale=request.scale;return row;
}
void run(){
    auto request=intent(a::DungeonInputTool::Door);expect(a::valid_dungeon_input(request),"actual Door intent accepted");
    auto bad=request;bad.scene="world";expect(!a::valid_dungeon_input(bad),"root scene cannot authorize Dungeon input");
    bad=request;bad.scene="action:"+action+"/template";expect(!a::valid_dungeon_input(bad),"foreign exterior scene rejected");
    bad=request;bad.cost=-1;expect(!a::valid_dungeon_input(bad),"negative original resource price rejected");
    bad=request;bad.shapes={{{{0,0},{1,0},{0,1}}}};expect(!a::valid_dungeon_input(bad),"Door cannot carry hidden Canvas edits");
    const auto before=state();auto after=changed(before);after.birth=gizmo(request);
    expect(a::dungeon_input_transition(action,request,before,after),"Door appends one canonical original Gizmo to exact scene");
    auto wrong=after;wrong.birth->gizmo.variant="foreign";expect(!a::dungeon_input_transition(action,request,before,wrong),"actual original variant must match captured selection");
    wrong=after;wrong.quests_sha256=std::string(64,'f');expect(!a::dungeon_input_transition(action,request,before,wrong),"ordinary Gizmo birth cannot alter authored Quest references");
    wrong=after;wrong.birth->key=mt2mp::game::dungeon_gizmo_identity::birth(other,0);expect(!a::dungeon_input_transition(action,request,before,wrong),"birth must retain exact ordered action identity");
    wrong=after;wrong.protected_state.dungeon.price=1;expect(!a::dungeon_input_transition(action,request,before,wrong),"new Gizmo cannot change Dungeon admission price");
    wrong=after;wrong.birth->scene=building+"/instance/0";expect(!a::dungeon_input_transition(action,request,before,wrong),"original return cannot move to another current scene");
    wrong=after;wrong.dungeon_sha256=before.dungeon_sha256;wrong.scenes_sha256=before.scenes_sha256;
    expect(!a::dungeon_input_transition(action,request,before,wrong),"unchanged complete native records cannot confirm");
    wrong=after;wrong.layout_sha256=std::string(64,'f');expect(!a::dungeon_input_transition(action,request,before,wrong),"Gizmo birth preserves complete original cell layout");
    request=intent(a::DungeonInputTool::Monster);expect(a::valid_dungeon_input(request),"normal rank zero Dungeon Monster is supported");
    bad=request;bad.rank=3;expect(!a::valid_dungeon_input(bad),"rank outside exact native three-value enum rejected");
    after=changed(before);a::MonsterActor monster;monster.tag=0;monster.status=3;monster.rank=0;monster.character_slot=1;monster.character_type=2;monster.level=1;
    monster.name="native owned name";monster.scale={1,1,1};monster.rotation={0,0,0,1};after.birth=a::DungeonInputBirth{"action:"+action,request.scene,request.tool,{},monster};
    expect(a::dungeon_input_transition(action,request,before,after),"original normal Dungeon Monster birth accepted");
    wrong=after;wrong.birth->monster.tag=7;expect(!a::dungeon_input_transition(action,request,before,wrong),"process-local spawn serial excluded from peer state");
    wrong=after;wrong.birth->monster.status=0;expect(!a::dungeon_input_transition(action,request,before,wrong),"unconfirmed original spawn status rejected");
    wrong=after;wrong.birth->monster.scale={2,2,2};expect(!a::dungeon_input_transition(action,request,before,wrong),"preview rank scale cannot replace native new Prop unit scale");
    auto retired=before;retired.dungeon_sha256=std::string(64,'f');retired.scenes_sha256=std::string(64,'e');
    expect(a::dungeon_input_history_transition(action,true,request,after,retired),"original Undo retires same canonical Monster with exact protected layout");
    expect(a::dungeon_input_history_transition(action,false,request,retired,after),"original Redo restores same canonical Monster key and current values");
    wrong=retired;wrong.birth=after.birth;expect(!a::dungeon_input_history_transition(action,true,request,after,wrong),"Undo cannot retain its original live canonical member");
    wrong=retired;wrong.protected_state.dungeon.undo_count=1;expect(!a::dungeon_input_history_transition(action,true,request,after,wrong),"history cannot bypass exact original Undo stack count");
    wrong=after;wrong.birth->key="action:"+other;expect(!a::dungeon_input_history_transition(action,false,request,retired,wrong),"restoration cannot invent another native action identity");
    auto paint=intent(a::DungeonInputTool::Paint);paint.type.clear();paint.variant.clear();paint.cost=0;paint.erase=true;
    paint.shapes={{{{0,0},{1,0},{0,1}}}};expect(a::valid_dungeon_input(paint),"actual clipped erase stroke admitted without synthetic Fill");
    auto painted=changed(before);expect(a::dungeon_input_transition(action,paint,before,painted),"complete original paint mutation retains no fake actor birth");
    wrong=painted;wrong.birth=gizmo(intent(a::DungeonInputTool::Door));expect(!a::dungeon_input_transition(action,paint,before,wrong),"paint stroke cannot conceal an unrelated actor birth");
    Ledger money,paid=money;request=intent(a::DungeonInputTool::Door);
    paid.cash=paid.record_cash=90;paid.record_expense=10;paid.line_counts[6]=1;paid.line_income[6]=-10;
    expect(a::dungeon_input_purchase(request,money,paid),"original Purchase updates exact expense/category6 once");
    auto forged=paid;forged.line_counts[5]=1;expect(!a::dungeon_input_purchase(request,money,forged),"unrelated ledger category cannot change");
    forged=paid;forged.record_income=-10;expect(!a::dungeon_input_purchase(request,money,forged),"purchase is expense, never signed history income");
    request.cost=0;paid=money;paid.line_counts[6]=1;expect(a::dungeon_input_purchase(request,money,paid),"free original Purchase still increments category6 count");
    expect(!a::dungeon_input_purchase(request,money,money),"free purchase cannot hide missing original accounting call");
    auto inverse=paid;inverse.line_counts[6]=2;expect(a::dungeon_input_history_cash(true,request,paid,inverse),"free Door original Undo still calls AddCash zero once");
    request.tool=a::DungeonInputTool::Key;expect(a::dungeon_input_history_cash(true,request,paid,paid),"free Key original graph contains no Cash inverse");
    expect(!a::dungeon_input_history_cash(true,request,paid,inverse),"free Key cannot synthesize Door cash node");
    request.cost=10;inverse=paid;inverse.cash+=10;inverse.record_cash+=10;inverse.record_income+=10;++inverse.line_counts[6];inverse.line_income[6]+=10;
    expect(a::dungeon_input_history_cash(true,request,paid,inverse),"paid original Undo refunds through signed income");
    auto redo=inverse;redo.cash-=10;redo.record_cash-=10;redo.record_income-=10;++redo.line_counts[6];redo.line_income[6]-=10;
    expect(a::dungeon_input_history_cash(false,request,inverse,redo),"original Redo uses negative AddCash signed income");
    forged=redo;forged.record_expense+=10;expect(!a::dungeon_input_history_cash(false,request,inverse,forged),"history cannot replay original purchase expense");
    money.line_counts[6]=INT32_MAX;paid=money;expect(!a::dungeon_input_purchase(request,money,paid),"native counter overflow rejected before original entry");
}
}
int main(){try{using namespace dungeon_action_fixture;run();std::cout<<checks<<" Dungeon input action checks passed\n";return 0;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
