#pragma once
#include <optional>
#include <utility>

namespace mt2mp::game::scene_building_registry {
// A replacement needs an exact baseline to prevent overwriting concurrent edits.
// Empty changes publish no aliases and do not depend on unrelated registry rows.
template<class Registry>struct Preview {
    Registry before;
    std::optional<Registry> after;
    void prepare(const Registry& current,bool changes){
        if(changes){before=current;after=current;}
        else {before.clear();after.reset();}
    }
    bool current(const Registry& actual)const{return !after||before==actual;}
    void publish_after_check(Registry& actual){
        if(after)actual=std::move(*after);
    }
};
}
