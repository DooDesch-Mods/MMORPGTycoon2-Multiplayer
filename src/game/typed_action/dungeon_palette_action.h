#pragma once
#include "dungeon_sound_action.h"
#include "dungeon_cells_action.h"
namespace mt2mp::action_session {
enum class DungeonPaletteEdit:uint8_t {Colors,Options,Cells};
struct DungeonPaletteIntent {
    std::string key,before_sha256;DungeonPaletteEdit edit=DungeonPaletteEdit::Colors;bool defaults=false;
    std::array<std::array<float,4>,4> colors{};std::array<uint32_t,4> packed{};
    DungeonCellStroke stroke;
    bool operator==(const DungeonPaletteIntent&)const=default;
};
struct DungeonPaletteState {
    DungeonSoundState protected_state;std::vector<DungeonPaletteCell> cells;std::vector<uint32_t> palette;bool defaults=false;
    DungeonCellWitness stroke;
    bool operator==(const DungeonPaletteState&)const=default;
};
inline bool dungeon_palette_transition(const DungeonPaletteIntent& intent,const DungeonPaletteState& before,const DungeonPaletteState& after){
    if(intent.edit==DungeonPaletteEdit::Cells)return dungeon_cells_transition(intent.stroke,before,after);
    if(before.stroke!=DungeonCellWitness{}||after.stroke!=DungeonCellWitness{}||intent.stroke!=DungeonCellStroke{})return false;
    if(before.palette.size()<4)return false;auto expected=before;expected.defaults=intent.defaults;
    for(size_t i=0;i<4;++i)expected.palette[i]=intent.packed[i];return expected==after;
}
}
