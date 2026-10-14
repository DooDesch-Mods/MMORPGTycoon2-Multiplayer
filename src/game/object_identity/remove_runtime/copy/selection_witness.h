#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mt2mp::build_actions::private_native {
// This module is private to the native adapter. Addresses must not enter DTOs.
struct SelectionWitness {
    uintptr_t receiver=0, info_window=0, focused=0;
    std::array<std::vector<uintptr_t>,4> lists;
    std::array<uint8_t,9> filters{};
    uint8_t selection_in_progress=0, hide_selected_outline=0, queue_mode=0;
    bool operator==(const SelectionWitness&)const=default;
};
struct Memory {
    virtual ~Memory()=default;
    virtual bool copy(uintptr_t,void*,size_t,std::string&)=0;
};
struct SelectionCounts {
    size_t queued=0,raw=0,filtered=0,highlighted=0;
    bool complete=false;
};
bool read_selection(Memory&,uintptr_t image,uintptr_t receiver,SelectionWitness&,SelectionCounts&,std::string&);
// Before ClearSelection, the native selective removal has already run.
bool validate_selective_removal(const SelectionWitness& before,const SelectionWitness& now,
    uintptr_t target,std::string&);
// After retirement, highlights must equal the old list minus the retired target.
bool validate_restored_selection(const SelectionWitness& before,const SelectionWitness& now,
    uintptr_t target,std::string&);
std::vector<uintptr_t> surviving_highlights(const SelectionWitness&,uintptr_t target);
}
