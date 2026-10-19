bool valid_dungeon_sound_intent(const DungeonSoundIntent& v){return key(v.key)&&v.before>=0&&v.before<6&&v.atmosphere>=0&&v.atmosphere<6;}
json dungeon_sound_intent_json(const DungeonSoundIntent& v){return {{"key",v.key},{"before",v.before},{"atmosphere",v.atmosphere}};}
DungeonSoundIntent dungeon_sound_intent_read(const json& j){exact(j,{"key","before","atmosphere"});DungeonSoundIntent v;v.key=j.at("key").get<std::string>();v.before=i32(j.at("before"));v.atmosphere=i32(j.at("atmosphere"));return v;}
bool valid_dungeon_sound_state(const DungeonSoundState& v){if(v==DungeonSoundState{})return true;return valid_dungeon_light_state({v.dungeon,v.light})&&v.atmosphere>=0&&v.atmosphere<6;}
json dungeon_sound_state_json(const DungeonSoundState& v){if(v==DungeonSoundState{})return nullptr;return {{"dungeon",dungeon_config_state_json(v.dungeon)},{"light",v.light},{"atmosphere",v.atmosphere}};}
DungeonSoundState dungeon_sound_state_read(const json& j){if(j.is_null())return {};exact(j,{"dungeon","light","atmosphere"});DungeonSoundState v;v.dungeon=dungeon_config_state_read(j.at("dungeon"));ability_array(j.at("light"),v.light,scalar);v.atmosphere=i32(j.at("atmosphere"));return v;}
