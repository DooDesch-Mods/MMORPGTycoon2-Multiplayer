#include "tutorial_replication/lifecycle.h"
#include <iostream>
#include <stdexcept>
namespace tutorial=mt2mp::game::tutorial_replication;
namespace life=tutorial::lifecycle;
using json=nlohmann::json;
namespace {
unsigned checks=0;
void need(bool value,const std::string& why){if(!value)throw std::runtime_error(why);}
void pass(const char* label){++checks;std::cout<<"PASS "<<label<<'\n';}
tutorial::Cursor cursor(const char* id,int next,bool terminal=false){const int count=std::string(id)=="initial_tutorial"?107:17;return {id,"last",count,next,terminal,{"welcome"},{}};}
tutorial::startingpoint::Row deployed(){tutorial::startingpoint::Row r;r.purchased=r.deployed=true;r.inventory_count=r.release_feature_count=r.release_change_count=1;r.release_counter=1;r.commit_bugs=tutorial::startingpoint::CommitBugs{};return r;}
tutorial::State active(int next=92){tutorial::State s;s.active.push_back(cursor("initial_tutorial",next));s.version=std::array<int32_t,3>{0,0,0};tutorial::startingpoint::Row p;p.project_present=p.purchased=true;p.work=182;p.lines=182;s.startingpoint=p;return s;}
tutorial::State terminal(){tutorial::State s;s.initial_complete=true;s.terminal.push_back(cursor("initial_tutorial",107,true));s.terminal_startingpoint=deployed();s.terminal_version=std::array<int32_t,3>{0,0,100};s.lifecycle_order[0][1]=1;return s;}
void codecs(){
    std::string e;tutorial::State out;const auto s=terminal();const auto raw=tutorial::encode(s);
    need(tutorial::valid(s,e)&&tutorial::decode(json::parse(raw.dump()),out,e)&&out==s,"terminal cursor/Commit receipt did not roundtrip: "+e);pass("actual terminal cursor, Startingpoint Commit receipt and version roundtrip");
    auto bad=raw;bad["terminal"][0]["count"]=108;bad["terminal"][0]["next"]=108;need(!tutorial::decode(bad,out,e)&&out==tutorial::State{},"foreign terminal asset count accepted");pass("terminal fixed native count is exact and failure clears partial output");
    bad=raw;bad["terminal"][0]["complete"]=false;need(!tutorial::decode(bad,out,e),"uncompleted terminal accepted");pass("terminal cursor must be complete");
    bad=raw;bad["terminal"][0]["message_progress"]={{"shown",true},{"value",1.0},{"minimum",0.0},{"maximum",1.0}};need(!tutorial::decode(bad,out,e),"retired native message meter accepted");pass("terminal receipt contains no live message meter");
    bad=raw;bad["terminal"].push_back(bad["terminal"][0]);need(!tutorial::decode(bad,out,e),"duplicate terminal ID accepted");pass("duplicate terminal scenario rejects");
    bad=raw;bad["active"].push_back(raw["terminal"][0]);need(!tutorial::decode(bad,out,e),"scenario simultaneously active and terminal accepted");pass("active/terminal duplicate cannot enter state");
    for(const auto& value:{json(-1),json(5),json(true),json(1.0)}){bad=raw;bad["lifecycle_order"][0][1]=value;need(!tutorial::decode(bad,out,e),"invalid lifecycle serial accepted");}pass("serials are exact integers within the four-event lifetime");
    bad=raw;bad["lifecycle_order"][0]=json::array({1,1});need(!tutorial::decode(bad,out,e),"duplicate birth/end serial accepted");pass("birth and terminal serials cannot collide");
    bad=raw;bad["lifecycle_order"][0]=json::array({2,1});need(!tutorial::decode(bad,out,e),"birth after completion accepted");pass("scenario cannot end before its own birth");
    for(const auto& value:{json::array({0,100}),json::array({0,0,-1}),json::array({0,0,100.0}),json::array({0,0,true})}){bad=raw;bad["terminal_version"]=value;need(!tutorial::decode(bad,out,e),"invalid native version shape/type accepted");}pass("version shape and component types stay exact");
    bad=raw;bad["terminal_startingpoint"]["commit_bugs"]["total"]=-1;need(!tutorial::decode(bad,out,e),"invalid terminal Commit bug receipt accepted");pass("terminal Startingpoint retains strict Commit scalar validation");
    auto old=tutorial::encode(active());for(const auto* key:{"startingpoint","staff_display","terminal","terminal_startingpoint","version","terminal_version","lifecycle_order","leveltwo_initial_next"})old.erase(key);
    need(tutorial::decode(old,out,e)&&!out.startingpoint&&!out.version&&out.terminal.empty()&&out.lifecycle_order==tutorial::State{}.lifecycle_order,"legacy payload invented lifecycle effects");pass("authentic old schema omits every new optional field");
}
void plans(){
    std::string e;life::Plan p;auto before=active(),after=terminal();
    need(life::plan(before,after,p,e)&&p.version_set&&p.operations==std::vector<life::Operation>{{life::Kind::Retire,0,107}},"coalesced Commit/end did not plan one retirement: "+e);pass("coalesced Commit/end uses retained technology proof and one terminal operation");
    auto missing=after;missing.terminal_startingpoint.reset();need(!life::plan(before,missing,p,e)&&e.find("Startingpoint")!=std::string::npos,"completion without terminal technology proof accepted");pass("completion requires terminal Startingpoint evidence");
    missing=after;missing.terminal_version.reset();need(!life::plan(before,missing,p,e),"completion without version proof accepted");pass("completion requires terminal native version");
    after.active.push_back(cursor("leveltwo_tutorial",4));after.lifecycle_order[1][0]=2;
    need(life::plan(before,after,p,e)&&p.operations==std::vector<life::Operation>{{life::Kind::Retire,0,107},{life::Kind::Create,1,1},{life::Kind::Forward,1,4}},"Initial retirement was followed by an invalid old checkpoint forward");pass("LevelTwo born after Initial end never forwards an already retired sequence");
    after.lifecycle_order[0][1]=2;after.lifecycle_order[1][0]=1;after.leveltwo_initial_next=97;
    need(life::plan(before,after,p,e)&&p.operations==std::vector<life::Operation>{{life::Kind::Forward,0,97},{life::Kind::Create,1,1},{life::Kind::Retire,0,107},{life::Kind::Forward,1,4}},"native birth/checkpoint event order lost");pass("LevelTwo birth before Initial end uses the measured Initial checkpoint first");
    auto invalid=after;invalid.leveltwo_initial_next=91;need(!life::plan(before,invalid,p,e)&&e.find("checkpoint")!=std::string::npos,"checkpoint behind local cursor accepted");pass("birth checkpoint cannot rewind a local cursor");
    invalid=after;invalid.lifecycle_order[1][0]=2;need(!life::plan(before,invalid,p,e),"duplicate cross-scenario serial accepted");pass("two-scenario event serials are globally unique");
    invalid=after;invalid.initial_complete=false;invalid.terminal.clear();invalid.terminal_version.reset();invalid.terminal_startingpoint.reset();invalid.lifecycle_order[0][1]=0;need(!life::plan(before,invalid,p,e),"active Initial vanished without completion");pass("active scenario disappearance requires actual native completion");
    auto twice=after;need(life::plan(after,twice,p,e)&&p.operations.empty(),"duplicate terminal frame repeated creation or retirement");pass("duplicate confirmed terminal frame plans no second birth/end");
    invalid=after;invalid.terminal[0].last="changed";need(!life::plan(after,invalid,p,e)&&e.find("retained terminal")!=std::string::npos,"terminal proof changed after confirmation");pass("retained terminal cursor is immutable");
    tutorial::State all;all.initial_complete=all.leveltwo_complete=true;all.terminal={cursor("initial_tutorial",107,true),cursor("leveltwo_tutorial",17,true)};all.terminal_startingpoint=deployed();all.terminal_version=std::array<int32_t,3>{0,0,100};all.lifecycle_order={{{1,2},{3,4}}};
    need(life::plan({},all,p,e)&&p.operations==std::vector<life::Operation>{{life::Kind::Create,0,1},{life::Kind::Retire,0,107},{life::Kind::Create,1,1},{life::Kind::Retire,1,17}},"coalesced two-scenario order lost: "+e);pass("both coalesced births and ends preserve original Host order");
    auto from=cursor("initial_tutorial",100),to=cursor("initial_tutorial",101);bool set=false;std::optional<std::array<int32_t,3>> zero=std::array<int32_t,3>{0,0,0},exact=std::array<int32_t,3>{0,0,100};
    need(life::version_plan(zero,exact,&from,&to,set,e)&&set,"exact original Version Set rejected");pass("Step100 permits exactly original Set(0,0,100)");
    need(!life::version_plan(zero,zero,&from,&to,set,e),"equal but unexecuted major0 version falsely passed Step100");pass("equal major0 version still requires the actual Step100 result");
    auto wrong=exact;(*wrong)[1]=1;need(!life::version_plan(zero,wrong,&from,&to,set,e),"arbitrary tutorial version setter admitted");pass("Step100 rejects an invented version target");
    std::optional<std::array<int32_t,3>> released=std::array<int32_t,3>{1,2,3};need(life::version_plan(released,released,&from,&to,set,e)&&!set,"released major version was rewritten");pass("already released native major version is preserved without a Set");
    from.next=101;to.next=102;need(!life::version_plan(zero,exact,&from,&to,set,e),"version setter outside Step100 admitted");pass("late version mutation cannot borrow Step100 authority");
}
void checkpoint(){auto before=active(),after=terminal();after.active.push_back(cursor("leveltwo_tutorial",4));after.lifecycle_order[0][1]=2;after.lifecycle_order[1][0]=1;after.leveltwo_initial_next=-1;life::Plan p;std::string e;need(!life::plan(before,after,p,e)&&e.find("checkpoint")!=std::string::npos,"missing Initial checkpoint accepted for LevelTwo birth before Initial retirement");pass("coalesced early LevelTwo birth requires its actual Initial checkpoint");}
void created_checkpoint(){auto after=terminal();after.active.push_back(cursor("leveltwo_tutorial",4));after.lifecycle_order={{{1,3},{2,0}}};after.leveltwo_initial_next=103;life::Plan p;std::string e;need(life::plan({},after,p,e)&&p.operations==std::vector<life::Operation>{{life::Kind::Create,0,1},{life::Kind::Forward,0,103},{life::Kind::Create,1,1},{life::Kind::Retire,0,107},{life::Kind::Forward,1,4}},"newly created Initial missed checkpoint before coalesced LevelTwo birth");pass("coalesced Initial creation reaches actual checkpoint before LevelTwo birth");}
}
int main(int argc,char** argv){try{const std::string group=argc>1?argv[1]:"all";if(group=="all"||group=="codec")codecs();if(group=="all"||group=="plan")plans();if(group=="all"||group=="checkpoint")checkpoint();if(group=="all"||group=="created_checkpoint")created_checkpoint();std::cout<<checks<<" Completion checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
