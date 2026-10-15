#pragma once
#include "state.h"
#include <algorithm>
namespace mt2mp::game::tutorial_replication::lifecycle {
inline const char* name(size_t slot){return slot==0?"initial_tutorial":"leveltwo_tutorial";}
inline size_t slot(const std::string& id){return id=="initial_tutorial"?0:1;}
inline const Cursor* find(const std::vector<Cursor>& rows,size_t index){for(const auto& c:rows)if(c.scenario==name(index))return &c;return nullptr;}
inline bool completed(const State& s,size_t index){return index==0?s.initial_complete:s.leveltwo_complete;}
inline const Cursor* desired(const State& s,size_t index){const auto* c=find(s.active,index);return c?c:find(s.terminal,index);}
enum class Kind {Forward,Create,Retire};
struct Operation {Kind kind;size_t index;int32_t next;bool operator==(const Operation&)const=default;};
struct Plan {std::vector<Operation> operations;bool version_set=false;};
inline bool version_plan(const std::optional<std::array<int32_t,3>>& local,const std::optional<std::array<int32_t,3>>& target,
                         const Cursor* from,const Cursor* to,bool& set,std::string& e){
    set=false;
    if(from&&to&&from->next<=100&&to->next>100&&(!target||((*target)[0]==0&&*target!=std::array<int32_t,3>{0,0,100}))){e="tutorial step100 Host version is not the original Set result";return false;}
    if(local==target)return true;
    if(!local||!target||!from||!to){e="tutorial version transition has no exact owned before/after cursor";return false;}
    if(from->next>100||to->next<=100||(*local)[0]>0||*target!=std::array<int32_t,3>{0,0,100}){e="tutorial version transition is not fixed Initial step100 Set(0,0,100)";return false;}
    set=true;return true;
}
inline bool plan(const State& local,const State& target,Plan& out,std::string& e){
    out={};if(!valid(local,e)||!valid(target,e))return false;
    struct Event {uint32_t serial;Kind kind;size_t index;};std::vector<Event> events;
    for(size_t i=0;i<2;++i){
        const auto* a=find(local.active,i);const auto* b=find(target.active,i);const auto* end=find(target.terminal,i);
        const auto* retained=find(local.terminal,i);
        if(completed(local,i)&&!completed(target,i)){e="tutorial completed scenario cannot become incomplete";return false;}
        if(retained&&(!end||*retained!=*end||local.lifecycle_order[i]!=target.lifecycle_order[i])){e="tutorial retained terminal receipt changed";return false;}
        if(local.lifecycle_order[i][0]&&local.lifecycle_order[i][0]!=target.lifecycle_order[i][0]){e="tutorial retained birth serial changed";return false;}
        if(a&&!b&&!completed(target,i)){e="tutorial active scenario disappeared without native completion";return false;}
        if(a){const auto* wanted=b?b:end;if(!wanted){e="tutorial completion requires retained terminal cursor";return false;}
            if(a->count!=wanted->count||a->next>wanted->next||(a->complete&&!wanted->complete)){e="tutorial lifecycle backward/count transition";return false;}}
        if(!a&&!completed(local,i)&&(b||completed(target,i))){
            if(!target.lifecycle_order[i][0]){e="tutorial creation requires original Host StartScenario birth receipt";return false;}
            if(completed(target,i)&&!end){e="tutorial coalesced creation/completion lacks terminal cursor";return false;}
            events.push_back({target.lifecycle_order[i][0],Kind::Create,i});
        }
        if(!completed(local,i)&&completed(target,i)){
            if(!end){e="tutorial native completion lacks terminal proof";return false;}
            events.push_back({target.lifecycle_order[i][1],Kind::Retire,i});
        }
    }
    const auto* from=find(local.active,0);const auto* to=desired(target,0);
    if(from&&to){const auto version=target.version?target.version:target.terminal_version;
        if(!version_plan(local.version,version,from,to,out.version_set,e))return false;}
    if(!completed(local,0)&&completed(target,0)&&(!target.terminal_startingpoint||!target.terminal_version)){
        e="tutorial Initial completion requires terminal Startingpoint and version proof";return false;}
    if(local.terminal_startingpoint&&local.terminal_startingpoint!=target.terminal_startingpoint){e="tutorial retained terminal Startingpoint proof changed";return false;}
    if(local.terminal_version&&local.terminal_version!=target.terminal_version){e="tutorial retained terminal version proof changed";return false;}
    std::sort(events.begin(),events.end(),[](const Event& a,const Event& b){return a.serial<b.serial;});
    bool initial_present=from!=nullptr;int32_t initial_next=from?from->next:0;
    for(const auto& event:events){
        if(event.kind==Kind::Create&&event.index==1&&initial_present&&target.leveltwo_initial_next<0){e="tutorial LevelTwo birth requires the original active Initial checkpoint";return false;}
        if(event.kind==Kind::Create&&event.index==1&&initial_present&&target.leveltwo_initial_next>=0){
            if(!to||target.leveltwo_initial_next<initial_next||target.leveltwo_initial_next>to->next){e="tutorial LevelTwo birth Initial cursor checkpoint differs";return false;}
            out.operations.push_back({Kind::Forward,0,target.leveltwo_initial_next});
            initial_next=target.leveltwo_initial_next;
        }
        out.operations.push_back({event.kind,event.index,event.kind==Kind::Retire?desired(target,event.index)->next:1});
        if(event.index==0){initial_present=event.kind==Kind::Create;if(initial_present)initial_next=1;}
    }
    for(const auto& wanted:target.active){const auto* current=find(local.active,slot(wanted.scenario));
        if(!current||*current!=wanted)out.operations.push_back({Kind::Forward,slot(wanted.scenario),wanted.next});}
    e.clear();return true;
}
inline std::optional<startingpoint::Row> technology(const State& s){return s.startingpoint?s.startingpoint:s.terminal_startingpoint;}
}
