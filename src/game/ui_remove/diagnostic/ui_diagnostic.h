#pragma once
#include "../native_contract.h"
#include <array>
#include <string_view>

namespace mt2mp::action_session::ui_diagnostic {
namespace ui=mt2mp::action_session::ui_capture;
struct Request {std::string diagnostic_uuid,snapshot_uuid,canonical_record_key;};
enum class Family:uint8_t {Building,Scenery};
struct Context {uintptr_t image=0,manager=0,target=0;uint64_t uid=0,epoch=0,generation=0;std::string exe_sha256;Family family=Family::Building;};
struct Window {uintptr_t handler=0,window=0,proxy=0,target=0;};
using SelectProp=void(*)(void*,void*);
using SetInfoWindow=void(*)(void*);
using RecordCtor=void(*)(void*,const char*);
using RecordDtor=void(*)(void*);
using RecordEquals=bool(*)(const void*,const char*);
using UiCommand=bool(*)(void*,void*);
struct Api {SelectProp select=nullptr;SetInfoWindow info=nullptr;RecordCtor construct=nullptr;RecordDtor destroy=nullptr;RecordEquals label_equals=nullptr;UiCommand command=nullptr;const char* destroy_label="DestroyBuilding";};
struct Receipt {
    bool selection_attempted=false,selection_returned=false,window_confirmed=false;
    bool record_constructed=false,record_label_confirmed=false,ui_command_attempted=false,ui_command_returned=false,ui_command_accepted=false;
    bool record_cleanup_attempted=false,record_cleanup_confirmed=false,intent_observed=false;
    uint32_t native_exception=0,cleanup_exception=0;
    std::string failed_stage,cause;
};
struct alignas(8) NativeRecordStorage {std::array<uint8_t,0x78> bytes{};};
static_assert(sizeof(NativeRecordStorage)==0x78&&alignof(NativeRecordStorage)==8);
bool resolve_api(ui::Memory&,uintptr_t image,std::string_view actual_exe_sha,Api&,std::string& cause,Family family=Family::Building);
bool read_live_window(ui::Memory&,const Context&,Window&,std::string& cause);
class Calls {
public:
    virtual ~Calls()=default;
    virtual bool select(const Api&,const Context&,Receipt&,std::string& cause)=0;
    virtual bool command(const Api&,const Window&,Receipt&,std::string& cause)=0;
};
class RootServices {
public:
    virtual ~RootServices()=default;
    // Actual game-thread/session/snapshot/UID/normal Building mapping plus installed own
    // lower hook and real typed Controller admission; no wire/native-ready flag.
    // Admit the diagnostic UUID once before native effects. Reject a used UUID
    // even after an unknown outcome; do not convert observation into a replay.
    virtual bool preflight(const Request&,Context&,std::string& cause)=0;
    virtual bool preparation_unchanged(const Context&,const Window&,std::string& cause)=0;
    // Compare actual capture counter/BuildingWindow origin and Controller request.
    virtual bool observed_intent(const Request&,const Context&,std::string& cause)=0;
    virtual void log(const char* operation,const std::string& cause)noexcept=0;
};
bool invoke(const Request&,RootServices&,ui::Memory&,Calls&,Receipt&,std::string& cause);
// Real native calls with POD-only SEH boundaries. Caller owns MCP game-thread
// execution and the diagnostic UUID journal; no automatic command replay.
Calls& process_calls();
}
