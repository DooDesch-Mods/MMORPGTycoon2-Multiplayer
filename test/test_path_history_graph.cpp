#include "path_history_graph.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::game::path_history_graph;
void need(bool value,const std::string& error){if(!value)throw std::runtime_error(error);}
Snapshot sample(){
    Snapshot s;s.roads={11,12};s.nodes={21,22,23};
    s.rows={{"paths",Json::array({Json::array({0,1,Json::array({{0,0,0},{1,0,0}}),Json::array({0,1}),Json::array(),Json::array()}),
                                Json::array({1,2,Json::array({{1,0,0},{2,0,0}}),Json::array({0,1}),Json::array(),Json::array()})})},
            {"nodes",Json::array({Json::array({{0,0,0},{0}}),Json::array({{1,0,0},{0,1}}),Json::array({{2,0,0},{1}})})}};
    return s;
}
int main(){try{
    unsigned cases=0;std::string error;Json expected,actual;const auto source=sample();
    need(rebase(source,source,expected,error),error);++cases;
    auto reordered=source;reordered.roads={12,11};reordered.nodes={23,21,22};
    reordered.rows["paths"]=Json::array({source.rows["paths"][1],source.rows["paths"][0]});
    reordered.rows["paths"][0][0]=2;reordered.rows["paths"][0][1]=0;
    reordered.rows["paths"][1][0]=1;reordered.rows["paths"][1][1]=2;
    reordered.rows["nodes"]=Json::array({Json::array({{2,0,0},{0}}),Json::array({{0,0,0},{1}}),Json::array({{1,0,0},{0,1}})});
    need(rebase(reordered,source,actual,error)&&actual==expected,error);++cases;
    reordered.rows["nodes"][2][1]=Json::array({1,0});
    need(rebase(reordered,source,actual,error)&&actual==expected,error);++cases;
    auto reject=[&](auto change){auto broken=source;change(broken);actual={{"unchanged",true}};
        need(!rebase(broken,source,actual,error)&&actual==Json({{"unchanged",true}})&&!error.empty(),"invalid topology accepted or output changed");++cases;};
    reject([](auto& s){s.roads[0]=0;});reject([](auto& s){s.roads[0]=99;});
    reject([](auto& s){s.roads[1]=s.roads[0];});reject([](auto& s){s.nodes[0]=99;});
    reject([](auto& s){s.nodes[1]=s.nodes[0];});reject([](auto& s){s.rows["paths"][0][0]=-1;});
    reject([](auto& s){s.rows["paths"][0][0]=3;});reject([](auto& s){s.rows["paths"][0][1]=0;});
    reject([](auto& s){s.rows["nodes"][0][1]=Json::array({0,0});});
    reject([](auto& s){s.rows["nodes"][0][1]=Json::array({1});});
    reject([](auto& s){s.rows["nodes"][1][1]=Json::array({0});});
    reject([](auto& s){s.rows["nodes"][0][1]=Json::array({2});});
    reject([](auto& s){s.rows["paths"][0].erase(5);});reject([](auto& s){s.rows["nodes"][0][0]=Json::array({0,0});});
    reject([](auto& s){s.rows["extra"]=1;});reject([](auto& s){s.roads.pop_back();});
    auto changed=source;changed.rows["paths"][0][2][0][0]=4;
    need(rebase(changed,source,actual,error)&&actual!=expected,error);++cases;
    changed=source;changed.rows["nodes"][1][0][2]=4;
    need(rebase(changed,source,actual,error)&&actual!=expected,error);++cases;
    const Snapshot empty{{},{},{{"paths",Json::array()},{"nodes",Json::array()}}};
    need(rebase(empty,empty,actual,error)&&actual==empty.rows,error);++cases;
    std::cout<<cases<<" Path history UID/topology comparisons passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
