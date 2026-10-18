#include "mt2mp/net/steam_presence.h"
#ifdef NDEBUG
#error Steam presence behavior tests require active assertions
#endif
#include <cassert>
#include <iostream>
#include <map>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
using namespace mt2mp::net::presence;

struct Api {
    std::map<std::string, std::string> values;
    std::vector<std::string> writes;
    std::string rejected, wrong_readback;
    bool get(const char* key, std::string& value, std::string&) {
        value = values[key];
        if (wrong_readback == key && !writes.empty()) value = "wrong";
        return true;
    }
    bool set(const char* key, const std::string& value, std::string& error) {
        if (rejected == key) { error = "setter rejected key=" + rejected; return false; }
        values[key] = value; writes.emplace_back(key); return true;
    }
};

int main() {
    std::string copied, copy_error;
    assert(!copy_value(nullptr,"status",copied,copy_error));
    const std::string too_long(256,'x');
    assert(!copy_value(too_long.c_str(),"status",copied,copy_error));
    assert(copy_value(std::string(255,'x').c_str(),"status",copied,copy_error)&&copied.size()==255);
#ifdef _WIN32
    SYSTEM_INFO system{};GetSystemInfo(&system);
    auto* pages=static_cast<char*>(VirtualAlloc(nullptr,system.dwPageSize*2,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    assert(pages);
    DWORD previous=0;
    assert(VirtualProtect(pages+system.dwPageSize,system.dwPageSize,PAGE_NOACCESS,&previous));
    for(size_t length=0;length<64;++length){
        auto* text=pages+system.dwPageSize-length-1;
        for(size_t i=0;i<length;++i)text[i]='x';
        text[length]='\0';
        assert(copy_value(text,"status",copied,copy_error)&&copied==std::string(length,'x'));
    }
    assert(VirtualFree(pages,0,MEM_RELEASE));
#endif
    const auto lobby = describe(17, 2, 4, false, false);
    assert(lobby[0] == "Multiplayer-Mod | Co-op lobby - 2/4 players");
    assert(lobby[1] == "mt2mp:17" && lobby[2] == "2");
    assert(describe(17, 2, 4, true, true)[0].find("Building together") != std::string::npos);
    assert(describe(17, 2, 4, false, true)[0].find("Loading co-op") != std::string::npos);
    assert(describe(17, 1, 4, false, false)[2] == "1");
    assert(describe(0, 2, 4, true, false) == generic());
    assert(describe(17, 0, 4, true, false) == generic());
    assert(describe(17, 5, 4, true, false) == generic());
    assert(describe(17, 2, 5, true, false) == generic());
    assert(generic()[0] == "Multiplayer-Mod" && generic()[1].empty() && generic()[2].empty());
    Api api;
    api.values = {{"status", "Building solo"}, {"steam_player_group", "old group"},
        {"steam_player_group_size", "1"}, {"steam_display", "#Status_ONLINE"}, {"subscribercount", "58"},
        {"connect", "original connect"}, {"other_mod", "untouched"}};
    const auto original = api.values;
    OwnedKeys owned;
    std::string error;
    assert(owned.apply(lobby, api, error) && error.empty() && owned.active());
    assert(api.writes.size() == 4);
    assert(api.values["subscribercount"] == "Multiplayer-Mod | Co-op 2/4 | 58");
    assert(owned.apply(lobby, api, error) && api.writes.size() == 4);
    assert(owned.apply(describe(17, 3, 4, true, false), api, error));
    assert(api.values["steam_player_group_size"] == "3");
    assert(api.writes.size() == 7);
    assert(owned.restore(api, error) && !owned.active());
    assert(api.values == original);
    const auto count = api.writes.size();
    assert(owned.restore(api, error) && api.writes.size() == count);
    assert(owned.apply(lobby, api, error));
    api.values["status"] = "Written later by game";
    assert(owned.restore(api, error));
    assert(api.values["status"] == "Written later by game");
    assert(api.values["steam_player_group"] == "old group");
    assert(owned.apply(lobby, api, error));
    api.values["subscribercount"] = "61";
    assert(owned.apply(lobby, api, error));
    assert(api.values["subscribercount"] == "Multiplayer-Mod | Co-op 2/4 | 61");
    assert(owned.restore(api, error));
    assert(api.values["subscribercount"] == "61");
    assert(owned.apply(lobby, api, error));
    auto desktop = lobby; desktop[3].clear();
    assert(owned.apply(desktop, api, error));
    assert(api.values["subscribercount"] == "61");
    assert(owned.restore(api, error));
    assert(decorate_count("Co-op 2/4 | ", "1,234") == "Co-op 2/4 | 1,234");
    assert(decorate_count("Co-op 2/4 | ", "Co-op 2/4 | 61").empty());
    assert(decorate_count("Co-op 2/4 | ", "58\nunsafe").empty());
    api.rejected = "steam_player_group";
    assert(!owned.apply(lobby, api, error) && error.find("setter rejected") != std::string::npos);
    assert(owned.restore(api, error));
    assert(api.values["status"] == "Written later by game");
    api.rejected.clear();
    assert(owned.apply(lobby, api, error));
    api.rejected = "status";
    assert(!owned.restore(api, error) && owned.active());
    assert(api.values["steam_player_group"] == "old group");
    api.rejected.clear();
    assert(owned.restore(api, error) && !owned.active());
    assert(api.values["steam_display"] == original.at("steam_display"));
    assert(api.values["connect"] == original.at("connect"));
    assert(api.values["other_mod"] == "untouched");
    assert(owned.apply(generic(), api, error));
    assert(api.values["status"] == "Multiplayer-Mod");
    assert(api.values["subscribercount"] == "Multiplayer-Mod | 61");
    assert(api.values["steam_player_group"] == "old group" && api.values["steam_player_group_size"] == "1");
    assert(owned.apply(lobby, api, error));
    assert(owned.apply(generic(), api, error));
    assert(api.values["subscribercount"] == "Multiplayer-Mod | 61");
    assert(api.values["steam_player_group"] == "old group");
    assert(owned.restore(api, error));
    assert(api.values["subscribercount"] == "61");
    Api title;
    title.values["steam_display"] = "#Status_Desktop";
    auto title_values = generic(); title_values[3].clear();
    OwnedKeys title_owned;
    assert(title_owned.apply(title_values, title, error));
    assert(title.values["status"] == "Multiplayer-Mod");
    assert(title.values["subscribercount"].empty());
    assert(title.values["steam_display"] == "#Status_Desktop");
    assert(title.writes.size() == 1);
    assert(title_owned.restore(title, error) && title.values["status"].empty());
    Api rejected_readback;
    rejected_readback.wrong_readback = "status";
    OwnedKeys failed;
    assert(!failed.apply(lobby, rejected_readback, error));
    assert(error == "Steam presence readback differs key=status");
    rejected_readback.wrong_readback.clear();
    assert(failed.restore(rejected_readback, error));
    assert(rejected_readback.values["status"].empty());
    std::cout << "Steam presence behavior assertions passed\n";
}
