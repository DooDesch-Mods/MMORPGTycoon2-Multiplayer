#pragma once
#include "hud_time_reader.h"
#include <mutex>
namespace mt2mp::game::hud_time {
using OriginalClockUpdate=void(*)(void*,float);
struct Authority {
    bool available=false, client=false;
    uint64_t epoch=0, revision=0;
    float selected_scale=0, pause_scale=0;
    bool operator==(const Authority&) const = default;
};
struct WriterServices {
    void* user=nullptr;
    bool (*authority)(void*,Authority&,std::string&)=nullptr;
    bool (*store_i32)(void*,uintptr_t,int32_t,std::string&)=nullptr;
    bool (*call_clock)(void*,OriginalClockUpdate,void*,float,std::string&)=nullptr;
};
struct UpdateResult {
    bool forward_original=true, attempted=false, confirmed=false, changed=false, unknown=false;
    uint64_t epoch=0, revision=0;
    std::string error;
};
struct ShowResult {
    bool visible=false, replaced=false;
    uint64_t epoch=0, revision=0;
    std::string error;
};
struct WriterStatus {
    bool initialized=false,bound=false,locked=false;
    uint32_t hook_mask=0;
    uint64_t epoch=0, revision=0, item_attempted=0,item_confirmed=0,item_failed=0,
        item_forwarded=0,show_replaced=0,show_forwarded=0,clock_entered=0,clock_forwarded=0;
    std::string error;
};
class Writer {
public:
    bool initialize(const Config&,const Services&,const WriterServices&,std::string&);
    bool bind(std::string&);
    bool set_hook_mask(uint32_t,std::string&);
    bool suspend(const std::string& reason,std::string&);
    UpdateResult on_update(void* type,void* item);
    ShowResult on_show(void* window,bool native_visible);
    bool clock_update(void* receiver,float dt,OriginalClockUpdate,std::string&);
    WriterStatus status() const {std::lock_guard lock(mutex_);return status_;}
private:
    bool preflight(State&,Reader::Witness&,Authority&,bool require_bound,std::string&);
    bool current_authority(Authority&,std::string&);
    bool same_roots(const Reader::Witness&,const Reader::Witness&) const;
    void lock_unknown(const std::string&);
    void retire();
    Reader reader_;
    WriterServices io_{};
    WriterStatus status_{};
    Reader::Witness binding_{};
    Authority authority_{};
    bool busy_=false,attempted_=false;
    uintptr_t clock_scope_=0;
    uint64_t retired_epoch_=0;
    mutable std::recursive_mutex mutex_;
};
}
