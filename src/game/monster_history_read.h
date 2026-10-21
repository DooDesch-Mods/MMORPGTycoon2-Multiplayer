#pragma once
#include "duplicate_quest_history.h"
#include "vendor/nlohmann/json.hpp"

namespace mt2mp::game::monster_history_read {
inline bool references(const nlohmann::json& before,const nlohmann::json& after,const std::set<std::string>& replaced,std::string& error){
    auto expected=before;
    if(!expected.is_array()||!after.is_array()||expected.size()!=after.size())
        return monster_quest_retirement::fail(error,"Monster history Subscriber reference cohort differs");
    for(size_t i=0;i<expected.size();++i){auto& progress=expected[i]["progress"];
        if(!progress.is_array()||!after[i].contains("progress")||after[i]["progress"].size()!=progress.size())
            return monster_quest_retirement::fail(error,"Monster history active Quest progress extent differs");
        for(size_t j=0;j<progress.size();++j){auto& row=progress[j];auto& link=row["quest"];
            if(link["state"]!=2||!link["key"].is_string())continue;
            const auto key=link["key"].get<std::string>();
            const bool changed=std::any_of(replaced.begin(),replaced.end(),[&](const auto& owner){return key.starts_with(owner+"/quest/");});
            if(!changed)continue;
            const auto& actual=after[i]["progress"][j]["quest"];
            if((actual["state"]!=0&&actual["state"]!=1)||actual["key"]!="")
                return monster_quest_retirement::fail(error,"Monster history replaced Quest remains a live Subscriber reference");
            link=actual;row["complete"]=false;
        }
    }
    return expected==after||monster_quest_retirement::fail(error,"Monster history changed unrelated Subscriber progress/rewards/references");
}
}
