#pragma once
#include "typed_action/dungeon_cells_action.h"
#include <cstdint>
#include <optional>
#include <string>
namespace mt2mp::game::dungeon_cells_capture {
struct Stroke {
    uintptr_t receiver=0,dungeon=0;std::string key,snapshot;
    action_session::DungeonCellStroke intent;
};
class Collector {
    std::optional<Stroke> value_;
public:
    bool append(const Stroke& next,int32_t cell,std::string& error){
        if(!next.receiver||!next.dungeon||next.key.empty()||next.snapshot.empty()||cell<0||
           uint8_t(next.intent.mode)>1||(next.intent.mode==action_session::DungeonCellMode::Paint?next.intent.theme.empty():!next.intent.theme.empty())){
            error="Dungeon cell stroke requires an owned receiver, scene, mode and cell";return false;}
        if(value_&&(value_->receiver!=next.receiver||value_->dungeon!=next.dungeon||value_->key!=next.key||
           value_->snapshot!=next.snapshot||value_->intent.mode!=next.intent.mode||value_->intent.theme!=next.intent.theme)){
            error="Dungeon cell stroke receiver, scene, snapshot, mode or Theme changed before release";return false;}
        if(!value_){value_=next;value_->intent.cells.clear();}
        for(const auto old:value_->intent.cells)if(old==cell){error.clear();return true;}
        value_->intent.cells.push_back(cell);error.clear();return true;
    }
    bool take(uintptr_t receiver,std::optional<Stroke>& out,std::string& error){
        out.reset();if(!value_){error.clear();return true;}
        if(value_->receiver!=receiver){error="Dungeon cell release does not own the captured receiver";return false;}
        out=std::move(value_);value_.reset();error.clear();return true;
    }
    void clear(){value_.reset();}
    bool active()const{return value_.has_value();}
};
}
