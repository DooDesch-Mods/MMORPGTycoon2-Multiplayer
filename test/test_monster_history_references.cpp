#include "monster_history_read.h"
#include <iostream>
#include <stdexcept>
int main()try{using nlohmann::json;using mt2mp::game::monster_history_read::references;unsigned checks=0;std::string error;
    const auto need=[&](bool ok,const char* cause){++checks;if(!ok)throw std::runtime_error(cause);};
    const auto row=[](const char* key){return json{{"quest",{{"state",2},{"key",key}}},{"complete",true},{"reward",17}};};
    json before=json::array({{{"id",1},{"npc","owner-a"},{"progress",json::array({row("owner-a/quest/0"),row("owner-b/quest/1"),row("owner-c/quest/0")})}}});
    auto after=before;for(size_t i=0;i<2;++i){after[0]["progress"][i]["quest"]={{"state",i==0?0:1},{"key",""}};after[0]["progress"][i]["complete"]=false;}
    need(references(before,after,{"owner-a","owner-b"},error),"Both original replaced-owner links should expire");
    need(!references(before,after,{"owner-a"},error),"Unrelated owner's active link changed");
    need(!references(before,before,{"owner-a"},error),"Replaced Quest retained live pointer authority");
    need(references(before,before,{},error),"Move must retain every reference");
    for(unsigned mutation=0;mutation<7;++mutation){auto bad=after;
        if(mutation==0)bad[0]["id"]=2;if(mutation==1)bad[0]["npc"]="owner-b";if(mutation==2)bad[0]["progress"][0]["reward"]=18;
        if(mutation==3)bad[0]["progress"][2]["complete"]=false;if(mutation==4)bad[0]["progress"][0]["quest"]["key"]="local:0";
        if(mutation==5)bad[0]["progress"].erase(2);if(mutation==6)bad.push_back(bad[0]);
        need(!references(before,bad,{"owner-a","owner-b"},error),"Unrelated Subscriber/Quest state changed");
    }
    std::cout<<checks<<" actual Monster history active-reference cases passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
