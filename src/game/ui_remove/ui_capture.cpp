#include "../snapshot_building_key.h"
#include "../npc_target_key.h"
#include "ui_capture.h"
#include <regex>
#include <stdexcept>

namespace mt2mp::action_session::ui_capture {
namespace {
bool valid(const Intent& i) {
    static const std::regex uuid("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    return std::regex_match(i.action_uuid,uuid)&&std::regex_match(i.snapshot_uuid,uuid)&&(game::npc_target_key::valid(i.canonical_record_key)||game::snapshot_building_key::valid(i.canonical_record_key));
}
bool local(const LocalBinding& b) {return b.manager&&b.object&&b.uid&&b.native_epoch&&b.native_generation&&
    (unsigned(b.supported_normal_building)+unsigned(b.supported_normal_scenery)+unsigned(b.supported_npc)==1);}
template<class Function>bool read(Function f,std::string& cause) {
    try {const bool okay=f();if(!okay&&cause.empty())cause="native UI adapter failed without original cause";return okay;}
    catch(const std::exception& e){cause=e.what();return false;}
    catch(...){cause="native UI adapter nonstandard exception";return false;}
}
}
void Capture::error(const std::string& cause) {
    status_.cause=cause;
    if(last_error_!=cause){last_error_=cause;services_.log("error","UI Remove capture",cause);}
    else ++status_.suppressed_errors;
}
bool Capture::publish_disabled(DestroyProps original,std::string& cause) {
    if(original_||enabled_||capturing_||authorizing_||status_.replay_active||!original) {cause="UI Remove original publication requires disabled fresh owner and actual trampoline";error(cause);return false;}
    original_=original;cause.clear();return true;
}
bool Capture::set_enabled(bool value,std::string& cause) {
    if(!original_||capturing_||authorizing_||status_.replay_active){cause="UI Remove enable/disable requires published original and idle scopes";error(cause);return false;}
    enabled_=value;cause.clear();return true;
}
Report Capture::forward(Disposition why,void* manager,NativePropArray* array,bool replay) {
    Report out;out.disposition=why;
    if(!original_){out.cause="UI Remove typed original was never published";error(out.cause);throw std::runtime_error(out.cause);}
    out.original_attempted=true;
    if(replay){status_.replay_consumed=true;++status_.replay_originals;}
    else ++status_.foreign_originals;
    try {services_.original(original_,manager,array);out.original_returned=true;if(replay)status_.replay_returned=true;return out;}
    catch(const std::exception& e){out.cause=e.what();status_.unknown=true;error("DestroyProps original: "+out.cause);throw;}
    catch(...){out.cause="DestroyProps original nonstandard exception";status_.unknown=true;error(out.cause);throw;}
}
Report Capture::reject(const std::string& original) {
    Report out;out.cause=original.empty()?"UI Remove rejected without original cause":original;
    ++status_.rejected;error(out.cause);services_.show_rejection(out.cause);return out;
}
void Capture::record_entry_failure(const std::string& cause)noexcept {
    if(!services_.on_game_thread()){services_.log("error","UI Remove entry failure","entry failure reported outside game thread");return;}
    status_.unknown=true;++status_.rejected;
    try {
        error(cause.empty()?"UI Remove exact caller frame unavailable":cause);
        services_.show_rejection(status_.cause);
    }catch(const std::exception& failure){
        services_.log("error","UI Remove entry failure reporting",failure.what());
        services_.show_rejection("UI Remove is Unknown after caller frame failure");
    }catch(...){
        services_.log("error","UI Remove entry failure reporting","nonstandard reporting exception");
        services_.show_rejection("UI Remove is Unknown after caller frame failure");
    }
}
Report Capture::forward_worker(void* manager,NativePropArray* array) {
    Report out;out.disposition=Disposition::ForeignOriginal;
    if(!original_){out.cause="worker DestroyProps original was never published";services_.log("error","DestroyProps worker",out.cause);throw std::runtime_error(out.cause);}
    out.original_attempted=true;
    try{services_.original(original_,manager,array);out.original_returned=true;return out;}
    catch(const std::exception& e){services_.log("error","DestroyProps worker original",e.what());throw;}
    catch(...){services_.log("error","DestroyProps worker original","nonstandard exception");throw;}
}
Report Capture::destroy_props(void* manager,NativePropArray* array,Origin origin) {
    // Foreign and worker paths keep OriginalOnce even when capture services fail.
    if(!services_.on_game_thread())return forward_worker(manager,array);
    if(!enabled_)return forward(Disposition::ForeignOriginal,manager,array);
    if(status_.replay_active) {
        if(origin!=Origin::Foreign)return reject("UI Remove cannot enter an active authoritative replay scope");
        if(uintptr_t(manager)!=replay_.binding.manager)return forward(Disposition::ForeignOriginal,manager,array);
        std::string cause;ArrayObservation view;
        if(!read([&]{return services_.array(array,view,cause);},cause)) {
            status_.unknown=true;error("own replay array: "+cause);
            services_.log("warn","DestroyProps foreign fallback","array observation failed in replay scope; forward OriginalOnce without counting own replay");
            return forward(Disposition::ForeignOriginal,manager,array);
        }
        if(uintptr_t(manager)==replay_.binding.manager&&view.count==1&&view.capacity>=1&&view.data&&view.object==replay_.binding.object) {
            if(status_.replay_consumed){status_.unknown=true;error("own replay attempted DestroyProps twice");throw std::runtime_error(status_.cause);}
            LocalBinding current;
            if(!read([&]{return services_.resolve(uintptr_t(manager),view.object,current,cause);},cause)||current!=replay_.binding) {
                status_.unknown=true;error("own replay current target binding differs: "+cause);throw std::runtime_error(status_.cause);
            }
            return forward(Disposition::OwnReplayOriginal,manager,array,true);
        }
        return forward(Disposition::ForeignOriginal,manager,array);
    }
    if(origin==Origin::Foreign)return forward(Disposition::ForeignOriginal,manager,array);
    if(capturing_||authorizing_)return reject("UI Remove capture is reentrant");
    if(status_.unknown)return reject("UI Remove capture has an Unknown original effect: "+status_.cause);
    capturing_=true;
    struct Reset {bool& flag;~Reset(){flag=false;}} reset{capturing_};
    std::string cause;Policy mode=Policy::Unavailable;
    if(!read([&]{mode=services_.policy(cause);return mode!=Policy::Unavailable;},cause))return reject("UI Remove authority unavailable: "+cause);
    if(mode==Policy::Standalone){capturing_=false;return forward(Disposition::StandaloneOriginal,manager,array);}
    if(mode!=Policy::AuthorityRequired)return reject("UI Remove authority policy invalid");
    ArrayObservation view;
    if(!read([&]{return services_.array(array,view,cause);},cause))return reject("UI Remove array: "+cause);
    if(view.count==0&&view.capacity>=0)return forward(Disposition::EmptyOriginal,manager,array);
    if(view.count!=1||view.capacity<view.count||!view.data||!view.object)return reject("UI Remove supports one shared prop");
    LocalBinding target;
    if(!read([&]{return services_.resolve(uintptr_t(manager),view.object,target,cause);},cause))return reject("UI Remove target: "+cause);
    if(!local(target)||target.manager!=uintptr_t(manager)||target.object!=view.object)return reject("UI Remove target is not a current shared prop");
    if(target.supported_npc&&origin!=Origin::NpcWindow)return reject("NPC Remove requires original NPC InfoWindow DestroyNPC caller");
    Intent intent;intent.snapshot_uuid=target.snapshot_uuid;intent.canonical_record_key=target.canonical_record_key;
    if(!read([&]{return services_.fresh_action_uuid(intent.action_uuid,cause);},cause))return reject("UI Remove action identity: "+cause);
    if(!valid(intent))return reject("UI Remove action identity: invalid action UUID, snapshot UUID or canonical key");
    if(!read([&]{return target.supported_npc?services_.submit_npc(intent,cause):services_.submit(intent,cause);},cause))return reject("UI Remove request: "+cause);
    ++status_.captured;Report out;out.disposition=Disposition::IntentCaptured;
    services_.log("info","UI Remove capture",target.supported_npc?
        "typed RemoveNPC intent submitted; native effects await Host authority":
        "typed RemoveSnapshotBuilding intent submitted; native effects await Host authority");return out;
}
bool Capture::with_replay(const Replay& value,const std::function<bool(std::string&)>& native_call,std::string& cause) {
    if(!services_.on_game_thread()||!enabled_||status_.replay_active||authorizing_||status_.unknown||!native_call||!local(value.binding)||
       !valid(value.intent)||value.intent.snapshot_uuid!=value.binding.snapshot_uuid||value.intent.canonical_record_key!=value.binding.canonical_record_key||
       (value.purpose!=ReplayPurpose::HostExecution&&value.purpose!=ReplayPurpose::ConfirmedHostApplication)) {
        cause="own native replay requires exact current authoritative game-thread scope";error(cause);return false;
    }
    authorizing_=true;
    const bool authorized=read([&]{return services_.authorize_replay(value,cause);},cause);
    authorizing_=false;
    if(!authorized){error("own replay authorization: "+cause);return false;}
    replay_=value;status_.replay_active=true;status_.replay_consumed=false;status_.replay_returned=false;
    struct Reset {Status& status;Replay& replay;~Reset(){status.replay_active=false;replay={};}} reset{status_,replay_};
    bool accepted=false;
    try {accepted=native_call(cause);}
    catch(const std::exception& e){status_.unknown=true;error("own native replay exception: "+std::string(e.what()));throw;}
    catch(...){status_.unknown=true;error("own native replay nonstandard exception");throw;}
    if(!accepted||!status_.replay_consumed||!status_.replay_returned||status_.unknown) {
        if(cause.empty())cause="own native replay did not return exactly one expected DestroyProps original";
        if(status_.replay_consumed)status_.unknown=true;
        error(cause);return false;
    }
    cause.clear();return true;
}
}
