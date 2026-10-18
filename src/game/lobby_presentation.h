#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace mt2mp::game::lobby_presentation {
enum class Phase : uint8_t { Choosing, Selected, Loading, Preparing, Sending, ClientLoading, Starting, Playing, Leaving, Failed, Creating };
inline bool parse_phase(std::string_view value,Phase& phase){
    if(value.size()==1&&value[0]>='0'&&value[0]<='9'){phase=Phase(value[0]-'0');return true;}
    if(value=="10"){phase=Phase::Creating;return true;}
    return false;
}
inline Phase host_phase(int preparation, bool transitioning, bool selected, const std::string& snapshot,bool creating=false) {
    if(preparation==10)return Phase::Failed;
    if(preparation==12)return Phase::Leaving;
    if(preparation==9)return Phase::Playing;
    if(snapshot=="awaiting_load")return Phase::ClientLoading;
    if(snapshot=="transferring"||snapshot=="awaiting_receivers"||snapshot=="awaiting_verification")return Phase::Sending;
    if(preparation==8||preparation==7)return Phase::Starting;
    if(preparation==1||preparation==4||transitioning)return Phase::Loading;
    if(preparation!=0)return Phase::Preparing;
    if(creating)return Phase::Creating;
    return selected?Phase::Selected:Phase::Choosing;
}
struct Message {std::string heading,detail;bool busy=false;};
inline Message describe(bool host, bool member, bool pending, int preparation, Phase remote,
                        const std::string& snapshot, uint64_t done, uint64_t total, bool failed) {
    if(failed||preparation==10)return {"Connection lost","Leave the session and reconnect."};
    if(!member)return pending?Message{"Connecting","" ,true}:Message{"Build together",""};
    if(preparation==12)return {"Leaving session","",true};
    if(snapshot=="transferring"&&total){
        const auto percent=done>=total?100:unsigned((done*100)/total);
        return {host?"Sending world":"Receiving world",std::to_string(percent)+" %",true};
    }
    if(snapshot=="awaiting_verification")return {"Verifying world","",true};
    if(snapshot=="awaiting_load")return {host?"Player is loading the world":"Loading your world",host?"You will start together afterwards.":"You will join automatically.",true};
    if(preparation==9)return {"Playing together",""};
    const auto phase=remote;
    switch(phase){
    case Phase::Creating:return {host?"Create your world":"Host is creating a world",host?"Complete the setup wizard.":"You will join automatically afterwards.",true};
    case Phase::Choosing:return {host?"Choose your world":"Host is choosing a world",host?"Load a save or create a new world.":"You will join automatically afterwards."};
    case Phase::Selected:return {host?"Ready to start":"Host has chosen a world",host?"Start the world for all players.":"The host will start your world."};
    case Phase::Loading:return {host?"Loading your world":"Host is loading the world",host?"Other players will join automatically.":"You will join automatically.",true};
    case Phase::Preparing:return {"Preparing shared world","",true};
    case Phase::Sending:return {host?"Sending world":"Transferring world","",true};
    case Phase::ClientLoading:return {host?"Player is loading the world":"Loading your world","",true};
    case Phase::Starting:return {"Starting game","",true};
    case Phase::Playing:return {"Finishing connection","",true};
    case Phase::Leaving:return {"Host left the session","Leave the lobby to start a new game."};
    case Phase::Failed:return {"Session interrupted","Leave the lobby and reconnect."};
    }
    return {"Connecting","",true};
}
}
