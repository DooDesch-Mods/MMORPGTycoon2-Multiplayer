#pragma once

#include <cstdint>
#include <string>
#include <functional>

namespace mt2mp::core {

// Bridge to access VectorStorm engine internals from our injected MSVC DLL.
//
// The engine uses vsSingletonManager (RTTI-based hash table) to register
// all singletons. We find this manager at runtime via pattern/string scanning
// and use it to access any engine singleton.
//
// We also locate key game objects (coreGame instance, ModeInGame, etc.)
// via string reference scanning in the MT2.exe binary.

class SingletonBridge {
public:
    static SingletonBridge& instance();

    // Initialize: scan the binary for key addresses.
    // Should be called after the game has fully initialized
    // (e.g., from MT2MP_Tick after a few frames).
    bool init();

    // === Key Object Pointers ===

    // The active coreGame-derived instance (MMORPG Tycoon 2's game class).
    // Found via the static `s_game` variable in Core.cpp.
    void* game_instance() const { return game_instance_; }

    // === Key Function Pointers ===

    // ModeInGame::Save - triggers a save
    void* fn_save() const { return fn_save_; }

    // ModeInGame::LoadBinary - loads a compressed binary save
    void* fn_load_binary() const { return fn_load_binary_; }

    // EditManager::BeginEdit
    void* fn_edit_begin() const { return fn_edit_begin_; }

    // EditManager::EndEdit
    void* fn_edit_end() const { return fn_edit_end_; }

    // === Singleton Access ===

    // Try to find and return a pointer to a named singleton from vsSingletonManager.
    // `rtti_name` should be the MinGW typeid().name() result for the pointer type.
    // Returns nullptr if not found.
    void* get_singleton(const char* rtti_name);

    // === Game Loop Hook ===

    // Hook coreGame::Update(float) via vtable patching.
    // `callback` is called every frame with the timestep.
    // Returns true if hook was installed.
    bool hook_game_update(std::function<void(float)> callback);

    // Unhook coreGame::Update.
    void unhook_game_update();

    bool initialized() const { return initialized_; }

    // Called from the detour trampoline
    void on_game_update(void* this_ptr, float dt);

private:
    SingletonBridge() = default;

    bool find_game_instance();
    bool find_key_functions();
    bool find_singleton_manager();

    bool initialized_ = false;

    // Key pointers
    void* game_instance_ = nullptr;
    void* singleton_manager_ = nullptr;

    // Function addresses (found via string scanning)
    void* fn_save_ = nullptr;
    void* fn_load_binary_ = nullptr;
    void* fn_edit_begin_ = nullptr;
    void* fn_edit_end_ = nullptr;
    void* fn_game_init_ = nullptr;

    // Game update hook state
    void* orig_game_update_ = nullptr;
    std::function<void(float)> update_callback_;
    int update_vtable_index_ = -1;
};

} // namespace mt2mp::core
