#pragma once
#include "placement_adapter.h"
#include <cstddef>

namespace placement_kiss::family {
using IdentifierAt = void*(__fastcall*)(void*, int);
using VariantAt = void*(__fastcall*)(void*, int);
using Clear = void(__fastcall*)(void*);
using Add = void(__fastcall*)(void*, const void*);
struct ArrayCalls { VariantAt variant; Clear clear; Add add; };
bool definition(void* value, std::uintptr_t image, const Request&, void*& variants,
    std::int32_t& count, std::string& cause, std::uint32_t& exception);
bool select_identifier(IdentifierAt, void* header, const Request&, std::int32_t& index,
    void*& identifier, std::string& cause, std::uint32_t& exception);
bool populate_identifiers(ArrayCalls, void* header, void* definition, std::uintptr_t image,
    const Request&, std::string& cause, std::uint32_t& exception);
bool set_private_selection(void* receiver, std::int32_t index, std::string& cause, std::uint32_t& exception);
}
