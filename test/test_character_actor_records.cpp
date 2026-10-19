#include "character_actor_records.h"
#include <iostream>
namespace actors=mt2mp::game::npc_probe::character_actor_records;
namespace record=mt2mp::subscriber_lifecycle;
namespace complete=mt2mp::game::duplicate_records;
record::Graph graph(uintptr_t object,uintptr_t definition,uintptr_t target){return {
    {{SIZE_MAX,0,{2,0,"actor"},{}},{0,0,{2,0,"linkId"},{{4,1,""}}},{0,1,{2,0,"health"},{{3,0x41200000,""}}},
     {0,2,{2,0,"type"},{{4,2,""}}},{0,3,{2,0,"target"},{{4,3,""}}},{0,4,{2,0,"ordinary_integer"},{{4,2,""}}}},
    {{1,object,true},{2,definition,false},{3,target,false}}};}
int main()try{
    unsigned checks=0;std::string error;const auto check=[&](bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);};
    for(size_t family=0;family<4;++family){check(actors::methods[family][1]-actors::methods[family][0]==0x60,"Actual COFF Actor PrepareSave/Save wrapper pair changed");
        const auto first=graph(100,200,300),other=graph(1000,2000,3000);
        const auto owners=[](uintptr_t pointer,std::string& key,std::string& cause){if(pointer==200||pointer==2000){key="character:1:0";return true;}
            if(pointer==300||pointer==3000){key="subscriber:9";return true;}cause="original unresolved Actor external owner";return false;};
        complete::Witness a,b;check(complete::summarize(first,100,owners,a,error),error);
        check(complete::summarize(other,1000,owners,b,error)&&a==b,"Process pointer leaked through complete Actor Record reference manifest");
        auto changed=other;changed.records[2].tokens[0].bits++;check(complete::summarize(changed,1000,owners,b,error)&&a.sha!=b.sha,"Original Actor health bit was omitted");
        changed=other;changed.records[5].tokens[0].bits=3;check(complete::summarize(changed,1000,owners,b,error)&&a.sha!=b.sha,"Ordinary value equal to a link ID was rewritten");
        changed=other;changed.private_links[2].object=4000;check(!complete::summarize(changed,1000,owners,b,error)&&error=="original unresolved Actor external owner","Unknown original Actor reference was guessed or its cause lost");
        changed=other;changed.records[2].tokens.push_back({4,0xabcdef01,""});check(complete::summarize(changed,1000,owners,b,error)&&a.sha!=b.sha,"Additional native Actor field was reduced to a selected property subset");
    }
    check(actors::methods[0][0]==0x10eda70&&actors::methods[1][0]==0xe4ad10&&actors::methods[2][0]==0x10ecae0&&actors::methods[3][0]==0x10f9c20,"Concrete original Boss/Monster/NPC/Toon serializer family is wrong");
    std::cout<<checks<<" complete original four-family Actor Record/reference and exact wrapper comparisons passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
