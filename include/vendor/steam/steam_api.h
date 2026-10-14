// Minimal Steamworks SDK stub - only types needed for our mod
// We access Steam functions via GetProcAddress at runtime
#pragma once

#include <stdint.h>

typedef uint64_t CSteamID;
typedef uint64_t SteamAPICall_t;
typedef uint32_t HSteamPipe;
typedef uint32_t HSteamUser;
typedef int32_t HSteamListenSocket;

// P2P send types
enum EP2PSend {
    k_EP2PSendUnreliable = 0,
    k_EP2PSendUnreliableNoDelay = 1,
    k_EP2PSendReliable = 2,
    k_EP2PSendReliableWithBuffering = 3,
};

// Lobby types
enum ELobbyType {
    k_ELobbyTypePrivate = 0,
    k_ELobbyTypeFriendsOnly = 1,
    k_ELobbyTypePublic = 2,
    k_ELobbyTypeInvisible = 3,
};
