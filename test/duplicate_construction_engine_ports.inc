namespace duplicate_engine_fixture {
unsigned native_calls=0,history_calls=0,finished=0;
uint64_t create_revision=2;
bool native_ok=true,finish_ok=true,history_ok=true,commit_ok=true;
std::map<uint64_t,std::string> targets;
void clear(){create_revision=2;native_calls=history_calls=finished=0;native_ok=finish_ok=history_ok=commit_ok=true;targets.clear();}
}
namespace ui_move_runtime::contract {
bool pending(const json&,const action::Request& request,uint64_t& revision,std::string& error){
    if(request.snapshot_uuid!=fixture::request.snapshot_uuid){error="original Duplicate pending snapshot differs";return false;}
    revision=(request.action==action::Action::DuplicateSelection||request.action==action::Action::EditModularBuilding)?duplicate_engine_fixture::create_revision:request.action==action::Action::UndoBuilding?3:4;return true;
}
}
namespace ui_move_runtime::duplicate_edit {
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence*,std::string& error){
    fixture::need(binding==fixture::binding&&request==fixture::request,"Duplicate original received foreign request/binding");
    ++duplicate_engine_fixture::native_calls;fixture::calls.push_back("duplicate-original");
    if(fixture::expected_retired_before_place)fixture::need(*fixture::expected_retired_before_place,"Duplicate did not retire old absent Job before allocator reuse");
    if(!duplicate_engine_fixture::native_ok){error="original Duplicate native no-effect";return action::Outcome::RejectedNoEffect;}
    return action::Outcome::Confirmed;
}
bool history_targets(const action::Binding&,const action::Request&,std::map<uint64_t,std::string>& out,std::string&){out=duplicate_engine_fixture::targets;return true;}
action::Outcome perform_history(const action::Binding&,const action::Request&,const action::Evidence*,std::string& error){
    ++duplicate_engine_fixture::history_calls;fixture::calls.push_back("duplicate-history-original");
    if(!duplicate_engine_fixture::history_ok){error="original Duplicate history failed after execution";return action::Outcome::Unknown;}
    return action::Outcome::Confirmed;
}
}
namespace mt2mp::game::developer_staff::construction {
bool finish_duplicate(const PlacementBefore&,const std::string& snapshot,const std::map<uint64_t,std::string>& targets,
                      std::vector<LocalJob>& out,std::string& error){
    ++duplicate_engine_fixture::finished;fixture::calls.push_back("duplicate-finish");out.clear();
    fixture::need(snapshot==fixture::request.snapshot_uuid&&targets==duplicate_engine_fixture::targets,
        "Duplicate queue admission did not receive exact native-created canonical key/full UID");
    if(!duplicate_engine_fixture::finish_ok){error="original Duplicate Job ownership failure";return false;}
    for(const auto& [uid,key]:targets)out.push_back({key,0x5678,uid,true,false,0x1000,0x2000,0x3000});
    return true;
}
}
namespace remove_diagnostics {
bool is_modular_target(const LocalTarget&,std::string& error){error="fixture target is not Modular";return false;}
bool history_target_absent(const std::string&,const std::string&,uint64_t uid,std::string& error){
    if(uid!=500){error="original Duplicate absent target full UID differs";return false;}return true;
}
}
namespace ui_move_runtime::modular {
bool targets(const action::Binding&,const action::Request& request,std::vector<std::string>& out,std::string&){out.clear();if(request.action==action::Action::UndoBuilding)out.push_back("action:"+request.target_action_uuid);return true;}
action::Outcome perform(const action::Binding&,const action::Request&,const action::Evidence*,std::string& error){
    ++duplicate_engine_fixture::native_calls;fixture::calls.push_back("modular-original");
    if(fixture::expected_retired_before_place)fixture::need(*fixture::expected_retired_before_place,"Modular did not retire old absent Job before allocator reuse");
    if(!duplicate_engine_fixture::native_ok){error="original Modular no-effect";return action::Outcome::RejectedNoEffect;}return action::Outcome::Confirmed;
}
bool publication(const action::Binding&,const action::Request& request,action::ModularEffect& out,std::string&){
    out={};if(request.action==action::Action::EditModularBuilding&&request.modular.mode)return true;
    action::DuplicateObject object;object.family=action::DuplicateFamily::Building;object.key="action:"+(request.action==action::Action::EditModularBuilding?request.action_uuid:request.target_action_uuid);
    if(request.action==action::Action::UndoBuilding)out.removed.push_back(object);else out.created.push_back(object);return true;
}
}
