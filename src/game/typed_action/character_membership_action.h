#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <climits>
#include <optional>
#include <set>
#include <string>
#include <vector>
namespace mt2mp::action_session {
enum class CharacterMembershipCommand:uint8_t {CreateVariant,Destroy,Import};
struct CharacterMembershipIntent {
    CharacterMembershipCommand command=CharacterMembershipCommand::CreateVariant;
    int32_t kind=-1,id=-1;std::string name,before_sha,compressed_record;
    bool operator==(const CharacterMembershipIntent&)const=default;
};
struct CharacterMembershipRow {
    int32_t kind=-1,id=-1,parent=-1;bool deleted=false;std::string name;
    std::vector<int32_t> variants;
    bool operator==(const CharacterMembershipRow&)const=default;
};
struct CharacterMembershipState {
    std::array<int32_t,5> counts{};
    std::array<int32_t,5> first_deleted{-1,-1,-1,-1,-1};CharacterMembershipRow selected;
    std::optional<CharacterMembershipRow> created;
    std::vector<int32_t> retired_definitions;
    std::string manager_sha,family_sha,others_sha,users_sha,quests_sha,economy_sha,monsters_sha,bosses_sha,import_art_sha,import_release_sha;
    std::array<uint64_t,4> global_random{};uint64_t undo_count=0;std::array<int32_t,8> release_edits{};
    bool operator==(const CharacterMembershipState&)const=default;
};
inline std::string character_membership_key(const CharacterMembershipIntent& value){return "character:"+std::to_string(value.kind)+":"+std::to_string(value.id);}
inline bool character_membership_row(const CharacterMembershipRow& row){
    if(row.kind<0||row.kind>4||row.id<0||row.parent< -1||row.parent==row.id||row.name.find('\0')!=std::string::npos)return false;
    std::set<int32_t> ids;for(const auto id:row.variants)if(id<0||id==row.id||!ids.insert(id).second)return false;
    return row.parent<0||row.variants.empty();
}
inline bool character_membership_transition(const CharacterMembershipIntent& intent,const CharacterMembershipState& before,const CharacterMembershipState& after){
    if(before.selected.kind!=intent.kind||before.selected.id!=intent.id||before.selected.deleted||before.created||
       !character_membership_row(before.selected)||!character_membership_row(after.selected)||
       (intent.command!=CharacterMembershipCommand::Import&&before.manager_sha==after.manager_sha)||!before.retired_definitions.empty()||
       before.selected.kind!=after.selected.kind||before.selected.id!=after.selected.id||
       (intent.command!=CharacterMembershipCommand::Import&&before.selected.name!=after.selected.name))return false;
    if(intent.command==CharacterMembershipCommand::Import)
        return !intent.compressed_record.empty()&&intent.name.empty()&&!after.selected.deleted&&
            before.counts==after.counts&&before.first_deleted==after.first_deleted&&before.selected.parent==after.selected.parent&&
            before.selected.variants==after.selected.variants&&!after.created&&after.retired_definitions.empty()&&
            before.undo_count==after.undo_count&&before.others_sha==after.others_sha&&
            !before.import_art_sha.empty()&&!after.import_art_sha.empty()&&!before.import_release_sha.empty()&&!after.import_release_sha.empty();
    if(!before.import_art_sha.empty()||!after.import_art_sha.empty()||!before.import_release_sha.empty()||!after.import_release_sha.empty())return false;
    if(intent.command==CharacterMembershipCommand::CreateVariant){
        auto counts=before.counts;if(intent.kind<0||intent.kind>4||counts[size_t(intent.kind)]==INT32_MAX)return false;
        const auto vacant=before.first_deleted[size_t(intent.kind)];const auto expected_id=vacant>=0?vacant:counts[size_t(intent.kind)]++;
        if(counts!=after.counts||before.selected.parent>=0||after.selected.parent!=before.selected.parent||after.selected.deleted||before.undo_count!=after.undo_count||!after.retired_definitions.empty()||
           !after.created||after.created->kind!=intent.kind||after.created->id!=expected_id||after.created->parent!=intent.id||
           after.created->deleted||after.created->name!=intent.name||!after.created->variants.empty())return false;
        auto variants=before.selected.variants;variants.push_back(expected_id);return variants==after.selected.variants;
    }
    if(intent.command!=CharacterMembershipCommand::Destroy||!intent.name.empty()||after.created||before.counts!=after.counts||!after.selected.deleted)return false;
    auto expected=before.selected.variants;expected.push_back(intent.id);std::sort(expected.begin(),expected.end());
    return after.selected.variants.empty()&&after.retired_definitions==expected;
}
}
