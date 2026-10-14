#pragma once
#include "npc_quest_state.h"
#include "snapshot_npc_key.h"
#include "subscriber_motion.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace mt2mp::subscriber_quest_state {
enum class LinkState { Empty,Expired,Live };
struct Link {
    LinkState state=LinkState::Empty;
    std::string npc_key;
    int32_t sequence_id=-1;
    bool operator==(const Link&)const=default;
};
struct Progress {
    Link npc,quest;
    int32_t completed=0,monsters_defeated=0;
    bool visited=false,action_performed=false,complete=false;
    bool operator==(const Progress&)const=default;
};
struct Toon {
    int32_t gold=0;
    uint32_t xp_bits=0;
    int32_t level=0,quests_completed=0;
    std::array<uint32_t,3> attributes{};
    uint32_t seconds_to_level_bits=0;
    std::vector<Progress> progress;
    bool operator==(const Toon&)const=default;
};
struct Counters {
    int32_t sequence_id=-1;
    npc_quest_records::DailyCount accepted,completed;
    std::array<int32_t,2> highest_cache{};
    bool operator==(const Counters&)const=default;
};
struct Npc {
    std::string key;
    int32_t type=0,level=0;
    game::subscriber_motion::Pose pose;
    npc_quest_records::DailyCount accepted,completed;
    std::vector<Counters> quests;
    std::array<int32_t,2> highest_cache{};
    bool operator==(const Npc&)const=default;
};
inline bool valid_key(std::string_view key){
    if(game::snapshot_npc_key::valid(key))return true;
    if(key.size()!=43||!key.starts_with("action:"))return false;
    const auto id=key.substr(7);
    for(size_t i=0;i<id.size();++i){
        if(i==8||i==13||i==18||i==23){if(id[i]!='-')return false;}
        else if(!((id[i]>='0'&&id[i]<='9')||(id[i]>='a'&&id[i]<='f')))return false;
    }
    return true;
}
inline bool finite_bits(uint32_t value){return std::isfinite(std::bit_cast<float>(value));}
inline bool valid(const Link& link,bool quest,std::string& error){
    if(link.state==LinkState::Empty||link.state==LinkState::Expired){
        if(link.npc_key.empty()&&link.sequence_id==-1){error.clear();return true;}
        error="Quest empty/expired link must have no key or sequence";return false;
    }
    if(link.state!=LinkState::Live||!valid_key(link.npc_key)||(quest?link.sequence_id<0:link.sequence_id!=-1)){
        error="Quest live link state/key/sequence invalid";return false;
    }
    error.clear();return true;
}
inline bool valid(const Toon& toon,std::string& error){
    if(toon.level<0||toon.quests_completed<0){error="Quest Toon level/completed count negative";return false;}
    if(!finite_bits(toon.xp_bits)||!finite_bits(toon.seconds_to_level_bits)){
        error="Quest Toon XP/time bits not finite";return false;
    }
    for(auto bits:toon.attributes)if(!finite_bits(bits)){error="Quest Toon attribute bits not finite";return false;}
    std::set<std::string> live_npc_keys;
    for(const auto& progress:toon.progress){
        if(!valid(progress.npc,false,error)||!valid(progress.quest,true,error))return false;
        if(progress.completed< -1||progress.monsters_defeated<0){error="Quest progress completed below discovered sentinel or monsters negative";return false;}
        if(progress.npc.state==LinkState::Live&&!live_npc_keys.insert(progress.npc.npc_key).second){error="Quest progress duplicate live NPC key";return false;}
        if(progress.npc.state==LinkState::Live&&progress.quest.state==LinkState::Live&&progress.npc.npc_key!=progress.quest.npc_key){
            error="Quest live NPC/Quest owners differ";return false;
        }
    }
    error.clear();return true;
}
inline bool valid(const std::vector<Npc>& npcs,std::string& error){
    std::set<std::string> keys;
    for(const auto& npc:npcs){
        if(!valid_key(npc.key)||!keys.insert(npc.key).second){error="Quest NPC key invalid/duplicate";return false;}
        if(npc.type<0||npc.type>2||npc.level<0||(npc.type!=0&&!npc.quests.empty())){error="Quest NPC type/level/Quest domain invalid";return false;}
        if(!game::subscriber_motion::valid(npc.pose,error))return false;
        std::set<int32_t> sequences;
        for(const auto& quest:npc.quests)if(quest.sequence_id<0||!sequences.insert(quest.sequence_id).second){error="Quest sequence invalid/duplicate";return false;}
    }
    error.clear();return true;
}
}
