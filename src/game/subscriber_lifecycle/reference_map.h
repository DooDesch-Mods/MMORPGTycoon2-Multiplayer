#pragma once
#include "record_graph.h"

namespace mt2mp::subscriber_lifecycle::first_append {
enum class ReferenceKind { RootShard, RootRegion, CharacterAbility, SharedBuilding, SubscriberToon, SharedNPC, SharedQuest };
struct ReferenceKey {
    ReferenceKind kind = ReferenceKind::RootShard;
    int32_t region_id = -1, character_kind = -1, character_id = -1, ability_index = -1;
    std::string building_key;
    int32_t subscriber_id = -1;
    std::string npc_key;
    int32_t sequence_id = -1;
    bool operator==(const ReferenceKey&) const = default;
};
// Caller holds the original drained SaveScope and a fresh NativeOpen world pin.
// These addresses are private readers' inputs, never serialized reference keys.
struct BuildingReferences {
    virtual ~BuildingReferences() = default;
    virtual bool identify(uintptr_t, std::string&, std::string&) = 0;
    virtual bool resolve(const std::string&, uintptr_t&, std::string&) = 0;
};
struct NpcReferences {
    virtual bool identify_quest(uintptr_t, std::string&, int32_t&, std::string&) = 0;
    virtual bool resolve_quest(const std::string&, int32_t, uintptr_t&, std::string&) = 0;
    virtual ~NpcReferences() = default;
    virtual bool identify(uintptr_t, std::string&, std::string&) = 0;
    virtual bool resolve(const std::string&, uintptr_t&, std::string&) = 0;
};
bool valid_npc_key(const std::string&);
bool valid_building_key(const std::string&);
struct ReferenceWorld { uintptr_t image = 0, state = 0, map = 0; BuildingReferences* buildings = nullptr; NpcReferences* npcs = nullptr; };
bool record_character_type(const std::vector<Record>&, int32_t&, int32_t&, std::string&);
bool identify_reference(Memory&, const ReferenceWorld&, uintptr_t object,
                        int32_t character_kind, int32_t character_id, ReferenceKey&, std::string&);
bool resolve_reference(Memory&, const ReferenceWorld&, const ReferenceKey&, uintptr_t&, std::string&);
}
