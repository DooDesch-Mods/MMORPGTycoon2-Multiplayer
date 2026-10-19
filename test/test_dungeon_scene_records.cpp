#include "dungeon_scene_records.h"
#include <iostream>
#include <stdexcept>
namespace records=mt2mp::game::dungeon_scene_records;
namespace native=mt2mp::subscriber_lifecycle;
const std::string owner="action:11111111-1111-4111-8111-111111111111";
native::Graph graph(uintptr_t root,uintptr_t actor,uintptr_t prototype){native::Graph out;
    out.records={{SIZE_MAX,0,{2,0,"region"},{}},{0,0,{2,0,"linkId"},{{4,1,""}}},{0,1,{2,0,"actor"},{}},
        {2,0,{2,0,"linkId"},{{4,2,""}}},{2,1,{2,0,"ordinary_integer"},{{4,2,""}}},{2,2,{2,0,"prototype"},{{4,3,""}}}};
    out.private_links={{1,root,true},{2,actor,true},{3,prototype,false}};return out;}
std::vector<records::Scene> scenes(uintptr_t base){return {{owner+"/template",owner,base,base+1,base+300,graph(base,base+10,base+110)},
    {owner+"/instance/0",owner,base+100,base+101,base+300,graph(base+100,base+110,base+10)}};}
int main()try{
    unsigned checks=0;const auto check=[&](bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);};
    std::string error;records::Witness first,second;const auto unknown=[](uintptr_t,std::string&,std::string& e){e="original unknown external scene reference";return false;};
    auto a=scenes(1000),b=scenes(5000);check(records::summarize(a,{},unknown,first,error),error);check(records::summarize(b,{},unknown,second,error)&&first.sha==second.sha,"Process pointers crossed complete original scene digest");
    check(first.scenes==2&&first.records==12&&first.links==6,"Complete template and Instance record census was reduced");
    b[0].graph.records[4].tokens[0].bits=3;check(records::summarize(b,{},unknown,second,error)&&first.sha!=second.sha,"Ordinary integer equal to native link ID was rewritten");
    b=scenes(5000);b[0].graph.private_links[2].object=99999;check(!records::summarize(b,{},unknown,second,error)&&error=="original unknown external scene reference","Unknown external record reference was guessed or cause replaced");
    b=scenes(5000);b[1].key=b[0].key;check(!records::summarize(b,{},unknown,second,error),"Duplicate native scene key accepted");
    b=scenes(5000);b[0].graph.records[3].tokens[0].bits=1;check(!records::summarize(b,{},unknown,second,error),"Duplicate original inline definition accepted");
    b=scenes(5000);b[0].graph.records[5].tokens[0].bits=7;check(records::summarize(b,{},unknown,second,error)&&first.sha!=second.sha,"Original authored prototype token was omitted");
    std::cout<<checks<<" complete original Dungeon scene records, cross-scene references and preserved tokens comparisons passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
