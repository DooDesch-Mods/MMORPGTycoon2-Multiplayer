#pragma once
#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
namespace mt2mp::action_session {
struct DungeonPaletteCell {std::string theme;std::array<uint32_t,4> colors{};bool open=false;bool operator==(const DungeonPaletteCell&)const=default;};
enum class DungeonCellMode:uint8_t {Paint,Close};
struct DungeonCellStroke {
    DungeonCellMode mode=DungeonCellMode::Paint;std::string theme;std::array<uint32_t,4> theme_packed{};std::vector<int32_t> cells;
    bool operator==(const DungeonCellStroke&)const=default;
};
struct DungeonCellIndexed {int32_t index=-1;DungeonPaletteCell value;bool operator==(const DungeonCellIndexed&)const=default;};
struct DungeonCellWitness {
    std::string dungeon_sha,scenes_sha,quests_sha;std::vector<DungeonCellIndexed> cells;
    bool operator==(const DungeonCellWitness&)const=default;
};
inline bool valid_cell_stroke(const DungeonCellStroke& value){
    if(uint8_t(value.mode)>1||value.cells.empty()||value.theme.find('\0')!=std::string::npos)return false;
    if(value.mode==DungeonCellMode::Paint?value.theme.empty():(!value.theme.empty()||value.theme_packed!=std::array<uint32_t,4>{}))return false;
    std::set<int32_t> seen;for(const auto cell:value.cells)if(cell<0||!seen.insert(cell).second)return false;return true;
}
template<class State>
bool dungeon_cells_history_transition(bool undo,const State& before,const State& after){
    if(!before.cells.empty()||!after.cells.empty()||before.palette.size()<4||before.palette!=after.palette||before.defaults!=after.defaults||
       before.stroke.cells.empty()||before.stroke.cells.size()!=after.stroke.cells.size()||before.stroke.dungeon_sha.empty()||
       before.stroke.dungeon_sha==after.stroke.dungeon_sha)return false;
    auto protected_state=before.protected_state;auto& count=protected_state.dungeon.undo_count;
    if(undo){if(!count)return false;--count;}else {if(count<20)++count;}
    protected_state.dungeon.global_random=after.protected_state.dungeon.global_random;
    if(protected_state!=after.protected_state)return false;
    for(size_t i=0;i<before.stroke.cells.size();++i)if(before.stroke.cells[i].index!=after.stroke.cells[i].index)return false;
    return true;
}
template<class State>
bool dungeon_cells_transition(const DungeonCellStroke& intent,const State& before,const State& after){
    if(!valid_cell_stroke(intent)||!before.cells.empty()||!after.cells.empty()||before.defaults!=after.defaults||
       before.palette.size()<4||before.palette.size()!=after.palette.size()||before.stroke.cells.size()!=intent.cells.size()||
       after.stroke.cells.size()!=intent.cells.size()||before.stroke.dungeon_sha==after.stroke.dungeon_sha)return false;
    auto protected_state=before.protected_state;
    if(protected_state.dungeon.undo_count>20)return false;
    if(protected_state.dungeon.undo_count<20)++protected_state.dungeon.undo_count;
    protected_state.dungeon.global_random=after.protected_state.dungeon.global_random;
    if(protected_state!=after.protected_state)return false;
    auto palette=before.palette;bool changed=false;
    for(size_t i=0;i<intent.cells.size();++i){
        const auto& old=before.stroke.cells[i];const auto& actual=after.stroke.cells[i];
        if(old.index!=intent.cells[i]||actual.index!=old.index)return false;
        if(actual.value==old.value)continue;
        auto desired=old.value;
        if(intent.mode==DungeonCellMode::Close){if(!desired.open)return false;desired.open=false;}
        else{
            if(!desired.open&&before.defaults)for(size_t color_index=0;color_index<4;++color_index)palette[color_index]=intent.theme_packed[color_index];
            desired.open=true;desired.theme=intent.theme;
            for(size_t color_index=0;color_index<4;++color_index)desired.colors[color_index]=palette[color_index];
        }
        if(desired!=actual.value)return false;changed=true;
    }
    return changed&&palette==after.palette;
}
}
