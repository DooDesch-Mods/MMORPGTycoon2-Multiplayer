#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace mt2mp::net {

// Packet type IDs
enum class PacketType : uint8_t {
    // Session (0x01-0x06)
    HANDSHAKE_REQUEST   = 0x01,
    HANDSHAKE_RESPONSE  = 0x02,
    PLAYER_JOINED       = 0x03,
    PLAYER_LEFT         = 0x04,
    HEARTBEAT           = 0x05,
    KICK                = 0x06,

    // Cursor (0x10)
    CURSOR_UPDATE       = 0x10,

    // Edits (0x20-0x23)
    EDIT_REQUEST        = 0x20,
    EDIT_APPROVED       = 0x21,
    EDIT_REJECTED       = 0x22,
    EDIT_BATCH          = 0x23,

    // Time (0x30-0x33)
    TIME_SPEED_CHANGE   = 0x30,
    TIME_PAUSE          = 0x31,
    TIME_UNPAUSE        = 0x32,
    TIME_SYNC           = 0x33,

    // Shard (0x40-0x41)
    SHARD_CHANGE        = 0x40,
    CAMERA_POSITION     = 0x41,

    // Save (0x50-0x54)
    SAVE_INITIATED      = 0x50,
    SAVE_CHUNK          = 0x51,
    SAVE_COMPLETE       = 0x52,
    FULL_STATE_REQUEST  = 0x53,
    FULL_STATE_CHUNK    = 0x54,

    // Chat (0x60)
    CHAT_MESSAGE        = 0x60,

    // Region Lock (0x70-0x73)
    LOCK_REQUEST        = 0x70,
    LOCK_GRANTED        = 0x71,
    LOCK_DENIED         = 0x72,
    LOCK_RELEASE        = 0x73,

    // Desync (0x80)
    CHECKSUM            = 0x80,
    RESYNC_REQUEST      = 0x81,
};

// Edit operation type IDs
enum class EditOpType : uint16_t {
    BUILDING_PLACE      = 0x0100,
    BUILDING_DESTROY    = 0x0101,
    BUILDING_MOVE       = 0x0102,
    BUILDING_CONFIG     = 0x0103,

    MONSTER_PLACE       = 0x0200,
    MONSTER_DESTROY     = 0x0201,
    MONSTER_CONFIG      = 0x0202,
    NPC_PLACE           = 0x0210,
    NPC_DESTROY         = 0x0211,
    NPC_CONFIG          = 0x0212,
    BOSS_PLACE          = 0x0220,

    TERRAIN_HEIGHT      = 0x0300,
    TERRAIN_STYLE       = 0x0301,

    ZONE_CREATE         = 0x0400,
    ZONE_DESTROY        = 0x0401,
    ZONE_RESIZE         = 0x0402,
    ZONE_CONFIG         = 0x0403,

    PATH_CREATE         = 0x0500,
    PATH_DESTROY        = 0x0501,
    PATH_EXTEND         = 0x0502,

    DUNGEON_TILE        = 0x0600,
    DUNGEON_DOOR        = 0x0601,
    DUNGEON_KEY         = 0x0602,
    DUNGEON_PAINT       = 0x0603,

    CLASS_CREATE        = 0x0700,
    CLASS_MODIFY        = 0x0701,
    ABILITY_MODIFY      = 0x0702,

    NET_SERVER_PLACE    = 0x0800,
    NET_UPLINK_PLACE    = 0x0801,
    NET_PIPE_PLACE      = 0x0802,
    NET_DESTROY         = 0x0803,

    SCENERY_PLACE       = 0x0900,
    SCENERY_DESTROY     = 0x0901,
    SCENERY_MOVE        = 0x0902,

    REGION_SET_LEVEL    = 0x0A00,
    REGION_SET_TINT     = 0x0A01,
    REGION_SET_SKY      = 0x0A02,
    REGION_SET_OWNER    = 0x0A03,

    PRICING_CHANGE      = 0x0B00,
    MARKETING_ACTION    = 0x0B01,

    GENERIC_EDIT        = 0xFF00,
};

// Wire format packet header
#pragma pack(push, 1)
struct PacketHeader {
    PacketType type;
    uint32_t sequence;
    uint32_t payload_length;
};
#pragma pack(pop)

static_assert(sizeof(PacketHeader) == 9);

// Cursor update payload
#pragma pack(push, 1)
struct CursorPayload {
    uint64_t steam_id;
    uint32_t shard_id;
    float world_x;
    float world_y;
    float world_z;
    uint8_t cursor_type;
};
#pragma pack(pop)

static_assert(sizeof(CursorPayload) == 25);

// Time sync payload
#pragma pack(push, 1)
struct TimeSyncPayload {
    uint32_t game_day;
    float day_fraction;
    uint8_t speed_setting; // 0=pause, 1=normal, 2=fast, 3=fastest
};
#pragma pack(pop)

// Region lock payload
#pragma pack(push, 1)
struct RegionLockPayload {
    uint64_t steam_id;
    uint32_t region_id;
};
#pragma pack(pop)

// Checksum payload (for desync detection)
#pragma pack(push, 1)
struct ChecksumPayload {
    uint32_t building_count;
    uint32_t monster_count;
    uint32_t npc_count;
    uint64_t gold;
    uint32_t game_day;
    float day_fraction;
    uint32_t hash; // quick state hash
};
#pragma pack(pop)

// Full packet (header + payload buffer)
struct Packet {
    PacketHeader header;
    std::vector<uint8_t> payload;

    // Serialize to byte buffer for sending
    std::vector<uint8_t> serialize() const;

    // Deserialize from received byte buffer
    static Packet deserialize(const uint8_t* data, size_t length);
};

} // namespace mt2mp::net
