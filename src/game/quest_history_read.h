#pragma once
#include "duplicate_quest_history.h"
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::game::quest_history_read {
inline bool references(const nlohmann::json& before,const nlohmann::json& after,const std::string& owner,std::string& e){
    auto expected=before;
    if(owner.empty()||!expected.is_array()||!after.is_array()||expected.size()!=after.size())
        return monster_quest_retirement::fail(e,"Quest history Subscriber cohort or owner differs");
    const auto prefix=owner+"/quest/";
    for(size_t i=0;i<expected.size();++i){auto& progress=expected[i]["progress"];
        if(!progress.is_array()||!after[i].contains("progress")||after[i]["progress"].size()!=progress.size())
            return monster_quest_retirement::fail(e,"Quest history active progress count differs");
        for(size_t j=0;j<progress.size();++j){auto& row=progress[j];auto& link=row["quest"];
            if(link["state"]!=2||!link["key"].is_string()||!link["key"].get_ref<const std::string&>().starts_with(prefix))continue;
            const auto& actual=after[i]["progress"][j]["quest"];
            if((actual["state"]!=0&&actual["state"]!=1)||actual["key"]!="")
                return monster_quest_retirement::fail(e,"Quest history retained a Live replaced Quest reference");
            link=actual;row["complete"]=false;
        }
    }
    return expected==after||monster_quest_retirement::fail(e,"Quest history changed unrelated progress, rewards or NPC references");
}
}
