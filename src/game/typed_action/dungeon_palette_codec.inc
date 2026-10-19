bool palette_bool(const json& j){if(!j.is_boolean())throw std::runtime_error("Dungeon palette flag requires bool");return j.get<bool>();}
json dungeon_cell_stroke_json(const DungeonCellStroke& v){return {{"mode",uint8_t(v.mode)},{"theme",v.theme},{"theme_packed",v.theme_packed},{"cells",v.cells}};}
DungeonCellStroke dungeon_cell_stroke_read(const json& j){
    exact(j,{"mode","theme","theme_packed","cells"});DungeonCellStroke v;const auto mode=u64(j.at("mode"));
    if(mode>1)throw std::runtime_error("Dungeon cell stroke mode is outside normal Tile edits");v.mode=DungeonCellMode(mode);v.theme=j.at("theme").get<std::string>();
    ability_array(j.at("theme_packed"),v.theme_packed,[](const json& item){const auto n=u64(item);if(n>UINT32_MAX)throw std::runtime_error("Dungeon Theme packed color exceeds uint32");return uint32_t(n);});
    if(!j.at("cells").is_array())throw std::runtime_error("Dungeon cell stroke requires ordered cell indices");
    for(const auto& item:j.at("cells")){const auto n=u64(item);if(n>INT32_MAX)throw std::runtime_error("Dungeon cell stroke index exceeds native int32");v.cells.push_back(int32_t(n));}return v;
}
bool valid_dungeon_palette_intent(const DungeonPaletteIntent& v){
    if(!key(v.key)||!digest(v.before_sha256)||uint8_t(v.edit)>2||(v.edit==DungeonPaletteEdit::Colors&&v.defaults))return false;
    if(v.edit==DungeonPaletteEdit::Cells)return !v.defaults&&v.colors==std::array<std::array<float,4>,4>{}&&v.packed==std::array<uint32_t,4>{}&&valid_cell_stroke(v.stroke);
    if(v.stroke!=DungeonCellStroke{})return false;
    for(const auto& color:v.colors)for(float x:color)if(!std::isfinite(x))return false;return true;
}
json dungeon_palette_intent_json(const DungeonPaletteIntent& v){json result={{"key",v.key},{"before_sha256",v.before_sha256},{"edit",uint8_t(v.edit)},{"defaults",v.defaults},{"colors",v.colors},{"packed",v.packed}};if(v.edit==DungeonPaletteEdit::Cells)result["stroke"]=dungeon_cell_stroke_json(v.stroke);return result;}
DungeonPaletteIntent dungeon_palette_intent_read(const json& j){
    const auto edit=u64(j.at("edit"));if(edit>2)throw std::runtime_error("Dungeon palette edit outside normal commands");
    if(edit==uint8_t(DungeonPaletteEdit::Cells))exact(j,{"key","before_sha256","edit","defaults","colors","packed","stroke"});else exact(j,{"key","before_sha256","edit","defaults","colors","packed"});
    DungeonPaletteIntent v;v.key=j.at("key").get<std::string>();v.before_sha256=j.at("before_sha256").get<std::string>();v.edit=DungeonPaletteEdit(edit);v.defaults=palette_bool(j.at("defaults"));
    if(v.edit==DungeonPaletteEdit::Cells)v.stroke=dungeon_cell_stroke_read(j.at("stroke"));
    if(!j.at("colors").is_array()||j.at("colors").size()!=4)throw std::runtime_error("Dungeon palette effective RGBA dimension differs");
    for(size_t i=0;i<4;++i)ability_array(j.at("colors")[i],v.colors[i],scalar);
    ability_array(j.at("packed"),v.packed,[](const json& x){const auto n=u64(x);if(n>UINT32_MAX)throw std::runtime_error("Dungeon packed RGBA exceeds uint32");return uint32_t(n);});return v;
}
bool valid_dungeon_palette_state(const DungeonPaletteState& v){if(v==DungeonPaletteState{})return true;
    if(!valid_dungeon_sound_state(v.protected_state)||v.palette.size()<4)return false;for(const auto& c:v.cells)if(c.theme.find('\0')!=std::string::npos)return false;
    if(v.stroke!=DungeonCellWitness{}){
        if(!v.cells.empty()||!digest(v.stroke.dungeon_sha)||!digest(v.stroke.scenes_sha)||!digest(v.stroke.quests_sha)||v.stroke.cells.empty())return false;
        std::set<int32_t> seen;for(const auto& cell:v.stroke.cells)if(cell.index<0||!seen.insert(cell.index).second||cell.value.theme.find('\0')!=std::string::npos)return false;
    }return true;
}
json dungeon_palette_layout_json(const DungeonPaletteState& v){json cells=json::array();for(const auto& c:v.cells)cells.push_back({{"theme",c.theme},{"colors",c.colors},{"open",c.open}});return {{"cells",std::move(cells)},{"palette",v.palette},{"defaults",v.defaults}};}
json dungeon_palette_state_json(const DungeonPaletteState& v){if(v==DungeonPaletteState{})return nullptr;auto out=dungeon_palette_layout_json(v);out["protected_state"]=dungeon_sound_state_json(v.protected_state);
    if(v.stroke!=DungeonCellWitness{}){json cells=json::array();for(const auto& cell:v.stroke.cells)cells.push_back({{"index",cell.index},{"theme",cell.value.theme},{"colors",cell.value.colors},{"open",cell.value.open}});
        out["stroke"]={{"dungeon_sha",v.stroke.dungeon_sha},{"scenes_sha",v.stroke.scenes_sha},{"quests_sha",v.stroke.quests_sha},{"cells",std::move(cells)}};}
    return out;}
DungeonPaletteState dungeon_palette_state_read(const json& j){
    if(j.is_null())return {};if(j.contains("stroke"))exact(j,{"protected_state","cells","palette","defaults","stroke"});else exact(j,{"protected_state","cells","palette","defaults"});DungeonPaletteState v;v.protected_state=dungeon_sound_state_read(j.at("protected_state"));v.defaults=palette_bool(j.at("defaults"));
    if(j.contains("stroke")){const auto& stroke=j.at("stroke");exact(stroke,{"dungeon_sha","scenes_sha","quests_sha","cells"});v.stroke.dungeon_sha=stroke.at("dungeon_sha").get<std::string>();v.stroke.scenes_sha=stroke.at("scenes_sha").get<std::string>();v.stroke.quests_sha=stroke.at("quests_sha").get<std::string>();
        if(!stroke.at("cells").is_array())throw std::runtime_error("Dungeon cell readback requires indexed native cell records");
        for(const auto& cell:stroke.at("cells")){exact(cell,{"index","theme","colors","open"});DungeonCellIndexed row;const auto index=u64(cell.at("index"));if(index>INT32_MAX)throw std::runtime_error("Dungeon cell readback index exceeds native int32");row.index=int32_t(index);row.value.theme=cell.at("theme").get<std::string>();row.value.open=palette_bool(cell.at("open"));
            ability_array(cell.at("colors"),row.value.colors,[](const json& item){const auto n=u64(item);if(n>UINT32_MAX)throw std::runtime_error("Dungeon cell readback color exceeds uint32");return uint32_t(n);});v.stroke.cells.push_back(std::move(row));}}
    if(!j.at("cells").is_array()||!j.at("palette").is_array())throw std::runtime_error("Dungeon palette native cell/palette record is not an array");
    for(const auto& item:j.at("cells")){exact(item,{"theme","colors","open"});DungeonPaletteCell c;c.theme=item.at("theme").get<std::string>();c.open=palette_bool(item.at("open"));
        ability_array(item.at("colors"),c.colors,[](const json& x){const auto n=u64(x);if(n>UINT32_MAX)throw std::runtime_error("Dungeon cell RGBA exceeds uint32");return uint32_t(n);});v.cells.push_back(std::move(c));}
    for(const auto& item:j.at("palette")){const auto n=u64(item);if(n>UINT32_MAX)throw std::runtime_error("Dungeon palette RGBA exceeds uint32");v.palette.push_back(uint32_t(n));}return v;
}
