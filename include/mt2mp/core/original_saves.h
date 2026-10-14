#pragma once
#include "mt2mp/core/private_profile_paths.h"
#include <limits>

namespace mt2mp::original_saves {
struct Native {
    const char* (*write_dir)() = nullptr;
    int (*mount)(const char*, const char*, int) = nullptr;
    int (*unmount)(const char*) = nullptr;
    const char* (*mount_point)(const char*) = nullptr;
    char** (*search_path)() = nullptr;
    void (*free_list)(void*) = nullptr;
    int (*error_code)() = nullptr;
    const char* (*error_text)(int) = nullptr;
};
enum class Result { Waiting, Isolated, Absent, Visible, Failed };
struct State { std::string owned_source, account; };
inline std::filesystem::path normalized(std::filesystem::path value) {
    while (value.has_relative_path() && value.filename().empty()) value = value.parent_path();
    return value.lexically_normal();
}
inline bool mount_is(const char* value, const std::string& expected) {
    if (!value) return false;
    std::string actual(value);
    while (!actual.empty() && actual.front() == '/') actual.erase(actual.begin());
    while (!actual.empty() && actual.back() == '/') actual.pop_back();
    return actual == expected;
}
inline std::string native_error(const Native& io) {
    const int code = io.error_code();
    const auto* text = io.error_text(code);
    return " code=" + std::to_string(code) + " cause=" + (text ? std::string(text) : "native error text unavailable");
}
// This runs after the native private user volume mount, before desktop save enumeration.
inline Result maintain(State& state, const Native& io, const std::filesystem::path& data_root,
                       const std::filesystem::path& proof_root, const std::filesystem::path& appdata,
                       const char* mounted_directory, const char* mountpoint, std::string& detail) {
    const auto fail = [&](const std::string& cause) { detail = cause; return Result::Failed; };
    detail.clear();
    if (data_root.empty() || !mounted_directory || !mount_is(mountpoint, "user")) return Result::Waiting;
    if (!io.write_dir || !io.mount || !io.unmount || !io.mount_point || !io.search_path || !io.free_list ||
        !io.error_code || !io.error_text) return fail("original saves native exports unavailable");
    const auto* raw_write = io.write_dir();
    if (!raw_write || !*raw_write) return fail("original saves actual private write directory unavailable");
    const std::string saved_write(raw_write);
    const auto write = normalized(private_profile::wide_utf8(saved_write.c_str()));
    if (!private_profile::equal(normalized(private_profile::wide_utf8(mounted_directory)), write) ||
        !private_profile::equal(write.parent_path(), normalized(data_root))) return Result::Waiting;
    const auto account = write.filename().string();
    if (account.size() < 17 || account.size() > 20 || account.find_first_not_of("0123456789") != std::string::npos)
        return Result::Waiting;
    if (!mount_is(io.mount_point(mounted_directory), "user"))
        return fail("original saves private account volume is not mounted at user");
    if (!state.owned_source.empty() && state.account != account) {
        if (!mount_is(io.mount_point(state.owned_source.c_str()), "user/save"))
            return fail("original saves previous account mount ownership changed");
        if (!io.unmount(state.owned_source.c_str())) return fail("original saves previous account unmount failed" + native_error(io));
        state = {};
    }
    // Proof runs must opt into a fixture APPDATA beneath their existing owner root.
    if (!proof_root.empty() && (appdata.empty() || !appdata.is_absolute() ||
        !private_profile::descendant(normalized(appdata), normalized(proof_root)))) {
        detail = "original saves skipped in isolated proof: APPDATA is outside proof root";
        return Result::Isolated;
    }
    if (appdata.empty() || !appdata.is_absolute()) return fail("original saves APPDATA is missing or relative");
    const auto source = normalized(appdata / L"VectorStorm" / L"MMORPG Tycoon 2" / account / L"save");
    std::error_code ec;
    const bool exists = std::filesystem::is_directory(source, ec);
    if (ec && ec != std::errc::no_such_file_or_directory) return fail("original saves directory read failed: " + ec.message());
    if (!exists) { detail = "original saves directory absent for current Steam account"; return Result::Absent; }
    const auto canonical = private_profile::canonical_file(source);
    if (!proof_root.empty() && !private_profile::descendant(canonical, private_profile::canonical_file(proof_root)))
        return fail("original saves fixture source resolves outside proof root");
    const auto private_saves = normalized(write / L"save");
    if (private_profile::equal(canonical, private_saves)) return fail("original saves source aliases private saves");
    // Native cache writers may skip mkdir when the read-only virtual world already exists.
    // Materialize only their private destination directories before exposing original worlds.
    private_profile::DirectoryPins private_parents;
    private_parents.verify(write / L"save", true);
    for (const auto& entry : std::filesystem::directory_iterator(canonical)) {
        std::error_code directory_error;
        const bool directory = entry.is_directory(directory_error);
        if (directory_error) return fail("original saves world-directory read failed: " + directory_error.message());
        if (directory) private_parents.verify(write / L"save" / entry.path().filename(), true);
    }
    const std::string source_text = private_profile::utf8(canonical);
    if (!state.owned_source.empty() && state.owned_source != source_text) {
        if (!io.unmount(state.owned_source.c_str())) return fail("original saves previous account unmount failed" + native_error(io));
        state.owned_source.clear();
    }
    const auto* existing = io.mount_point(source_text.c_str());
    if (existing && !mount_is(existing, "user/save")) return fail("original saves existing source mount point differs");
    if (!existing) {
        if (!io.mount(source_text.c_str(), "user/save", 1)) return fail("original saves read mount failed" + native_error(io));
        state.owned_source = source_text; state.account = account;
    }
    const auto priority = [&]() {
        char** list = io.search_path();
        if (!list) { detail = "original saves search path read failed" + native_error(io); return -1; }
        struct List { char** value; void (*free)(void*); ~List() { free(value); } } held{list, io.free_list};
        size_t own = std::numeric_limits<size_t>::max(), original = own;
        size_t index = 0;
        for (; index < 256 && list[index]; ++index) {
            const auto path = normalized(private_profile::wide_utf8(list[index]));
            if (private_profile::equal(path, write)) own = index;
            if (private_profile::equal(path, canonical)) original = index;
        }
        if (index == 256) { detail = "original saves search path exceeds 256 entries"; return -1; }
        if (own == std::numeric_limits<size_t>::max() || original == std::numeric_limits<size_t>::max()) {
            detail = "original saves private or original source absent from search path"; return -1;
        }
        return own < original ? 1 : 0;
    };
    int order = priority();
    if (order < 0) return Result::Failed;
    if (!order) {
        if (state.owned_source != source_text) return fail("original saves foreign mount precedes private volume");
        if (!io.unmount(source_text.c_str())) return fail("original saves owned reorder unmount failed" + native_error(io));
        state.owned_source.clear();
        if (!io.mount(source_text.c_str(), "user/save", 1)) return fail("original saves owned reorder mount failed" + native_error(io));
        state.owned_source = source_text; state.account = account;
        order = priority();
        if (order < 0) return Result::Failed;
        if (!order) return fail("original saves private priority not restored");
    }
    if (!mount_is(io.mount_point(source_text.c_str()), "user/save")) return fail("original saves mounted source readback differs");
    const auto* after = io.write_dir();
    if (!after || !private_profile::equal(normalized(private_profile::wide_utf8(after)), write))
        return fail("original saves private write directory changed during read mount");
    detail = "original saves visible as read search path; private writes retained";
    return Result::Visible;
}
}
