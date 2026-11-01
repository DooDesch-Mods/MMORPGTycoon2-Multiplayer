#pragma once
#include <string>

namespace mt2mp::game::terrain_history_contours {
struct State {
    std::string current,undo_target,redo_target;
    void initialize(const std::string& before,const std::string& after){current=after;undo_target=before;redo_target.clear();}
    const std::string& target(bool undoing)const{return undoing?undo_target:redo_target;}
    void confirmed(bool undoing,const std::string& before,const std::string& after){
        (undoing?redo_target:undo_target)=before;current=after;
    }
};
}
