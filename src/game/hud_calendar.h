#pragma once
#include "hud_time_reader.h"
#include <mutex>
namespace mt2mp::game::hud_time {
struct CalendarContext {
    bool active=false,client=false;
    uint64_t epoch=0,time_revision=0;
    bool operator==(const CalendarContext&) const=default;
};
struct CalendarFields {
    int32_t year=0,month=0,day=0,flat_day=0,seconds=0;
    bool software_calendar_enabled=false,calendar_format=false;
    bool operator==(const CalendarFields&) const=default;
};
struct CalendarSample {
    uint64_t epoch=0,revision=0,time_revision=0;
    CalendarFields fields{};
    bool operator==(const CalendarSample&) const=default;
};
using OriginalPanelUpdate=void(*)(void*,float);
using OriginalTimeString=void*(*)(void* result,void* clock);
struct CalendarServices {
    void* user=nullptr;
    bool (*context)(void*,CalendarContext&,std::string&)=nullptr;
    bool (*call_panel)(void*,OriginalPanelUpdate,void*,float,std::string&)=nullptr;
};
struct CalendarStatus {
    bool initialized=false,bound=false,has_sample=false,locked=false;
    uint32_t hook_mask=0;
    uint64_t epoch=0,revision=0,probes=0,received=0,duplicate=0,rejected=0,
        panel_original_attempted=0,formatter_original_attempted=0,formatter_substituted=0,formatter_returned=0;
    uint64_t formatted_epoch=0,formatted_revision=0,formatted_time_revision=0;
    CalendarFields formatted_fields{};
    std::string error;
};
class Calendar {
public:
    bool initialize(const Config&,const Services&,const CalendarServices&,std::string&);
    bool set_hook_mask(uint32_t,std::string&);
    bool bind(std::string&);
    bool suspend(const std::string&,std::string&);
    bool read_host_probe(CalendarSample&,std::string&);
    bool receive(uint64_t authenticated_peer,const CalendarSample&,std::string&);
    bool read_sample(CalendarSample&,std::string&) const;
    bool panel_update(void*,float,OriginalPanelUpdate,std::string&);
    void* format(void* result,void* clock,uintptr_t caller_rva,OriginalTimeString);
    CalendarStatus status() const {std::lock_guard lock(mutex_);return status_;}
private:
    bool inspect(CalendarContext&,CalendarFields&,Reader::Witness&,std::string&);
    bool scope_ready(CalendarContext&,CalendarFields&,Reader::Witness&,std::string&);
    void retire();
    Reader reader_;
    CalendarServices io_{};
    CalendarStatus status_{};
    CalendarContext context_{};
    Reader::Witness binding_{};
    CalendarSample sample_{};
    bool busy_=false,attempted_=false,formatter_inflight_=false;
    uintptr_t panel_scope_=0;
    uintptr_t bound_text_pane_=0,inspected_text_pane_=0;
    uint32_t scope_depth_=0;
    uint64_t retired_epoch_=0;
    mutable std::recursive_mutex mutex_;
};
bool process_call_calendar_panel(void*,OriginalPanelUpdate,void*,float,std::string&);
}
