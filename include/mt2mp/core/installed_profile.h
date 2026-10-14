#pragma once
#include <windows.h>
#include <cstdint>
#include <filesystem>
#include <string>

namespace mt2mp::installed_profile {
struct Receipt {
    uint32_t size=sizeof(Receipt),version=1;
    HMODULE loader=nullptr;
    DWORD process_id=0;
    uint32_t isolation_flags=0;
    const wchar_t* game_dir=nullptr;
    const wchar_t* runtime_root=nullptr;
    const wchar_t* user_data_dir=nullptr;
    const char* run_id=nullptr;
};
struct Verified {
    std::filesystem::path game_dir,runtime_root,user_data_dir;
    std::string run_id;
};
bool accept(const Receipt& receipt,std::string& error);
const Verified* get();
}
