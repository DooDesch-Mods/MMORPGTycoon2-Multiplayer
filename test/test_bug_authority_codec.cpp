#include "bug_authority/codec.h"
#include <iostream>
#include <limits>
namespace bug=mt2mp::game::bug_authority;
using Json=nlohmann::json;
namespace {
void need(bool ok,const std::string& e){if(!ok)throw std::runtime_error(e);}
const std::string id="11111111-1111-4111-8111-111111111111",sha(64,'a');
bug::Row row(){bug::Row r;r.parent_key=sha+":world:building:31";r.birth.source=sha;r.value.variant=0;r.value.reproducibility=.5f;return r;}
}
int main(){try{
    std::string e;bug::State state{{row()}},actual;
    need(bug::decode(bug::encode(state),actual,e)&&actual==state,e);
    need(bug::decode(Json::array(),actual,e)&&actual.rows.empty(),"empty state is not authoritative");
    size_t checks=2;
    auto invalid=[&](Json j){actual=state;need(!bug::decode(j,actual,e)&&actual.rows.empty()&&!e.empty(),"malformed Bug row accepted or partial output retained");++checks;};
    const auto good=bug::encode(state);
    for(const size_t field:{0u,2u,4u,5u,6u,7u,8u,9u,10u}){auto j=good;j[0][field]=1.5;invalid(j);}
    for(const size_t field:{12u,13u,14u}){auto j=good;j[0][field]=1;invalid(j);}
    for(const auto& [field,value]:std::vector<std::pair<size_t,Json>>{{0,2},{2,3},{4,-1},{5,-1},{6,3},{7,-2},{8,-2},{9,-1},{10,-1},{11,-.1},{11,1.0000000001},{11,"0.5"},{1,"action:invalid"},{3,id}}){auto j=good;j[0][field]=value;invalid(j);}
    auto j=good;j[0].push_back(1);invalid(j);invalid(Json::object());j=good;j.push_back(j[0]);invalid(j);
    auto second=row();second.parent_key=sha+":world:building:32";state.rows.push_back(second);
    need(bug::valid(state,e),"snapshot occurrence must be parent-scoped: "+e);++checks;
    for(const auto kind:{bug::BirthKind::Action,bug::BirthKind::Publication}){
        auto s=state;for(auto& r:s.rows){r.birth.kind=kind;r.birth.source=id;r.birth.sequence=kind==bug::BirthKind::Publication?7:0;}
        need(!bug::valid(s,e)&&e.find("global")!=std::string::npos,"global birth duplicate accepted");++checks;
        s.rows[1].birth.occurrence=1;need(bug::decode(bug::encode(s),actual,e)&&actual==s,e);++checks;
        s.rows[0].birth.sequence=kind==bug::BirthKind::Publication?0:1;need(!bug::valid(s,e),"invalid birth sequence");++checks;
    }
    state={{{row()}}};state.rows[0].parent_kind=bug::ParentKind::Npc;state.rows[0].parent_key=sha+":world:npc:34";
    need(bug::valid(state,e),e);++checks;state.rows[0].parent_key=sha+":world:building:34";need(!bug::valid(state,e),"wrong parent domain");++checks;
    state.rows[0].parent_key="action:"+id;state.rows[0].value.manual=state.rows[0].value.debugged=true;
    need(bug::decode(bug::encode(state),actual,e)&&actual==state,e);++checks;
    state.rows[0].value.reproducibility=std::numeric_limits<float>::quiet_NaN();need(!bug::valid(state,e),"NaN accepted");++checks;
    std::cout<<"PASS "<<checks<<" actual Bug codec cases\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
