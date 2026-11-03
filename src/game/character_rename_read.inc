json diagnostic_character(const a::CharacterRenameState& value){return {
    {"day",value.day},{"rename_day",value.rename_day},{"count",value.count},{"name",value.name},{"definition_name",value.definition_name},
    {"all_sha256",value.all_sha256},{"others_sha256",value.others_sha256},{"edited",value.edited},{"released",value.released},
    {"attributes",value.attributes},{"speed",value.speed},{"boss_health",value.boss_health},{"boss_phases",value.boss_phases},
    {"undo_count",value.undo_count},{"release_edits",value.release_edits},{"global_random",value.global_random}};}
json diagnostic_money(const a::Ledger& value){return {{"cash",value.cash},{"flat_day",value.flat_day},{"record_present",value.record_present},
    {"record_day",value.record_day},{"record_cash",value.record_cash},{"record_income",value.record_income},{"record_expense",value.record_expense},
    {"line_counts",value.line_counts},{"line_income",value.line_income}};}
json diagnostic_sample(const Sample& sample){return {{"character",diagnostic_character(sample.value)},
    {"proof",sample.proof},{"ledger",diagnostic_money(sample.money)}};}
bool read_native(const json& args,json& result,std::string& error)try{
    result=json::object();
    if(!args.is_object()||args.size()>1||(args.size()==1&&(!args.contains("action_uuid")||!args.at("action_uuid").is_string()||
       !token_valid(args.at("action_uuid").get<std::string>()))))return fail(error,"Character read requires empty args or one exact action_uuid");
    if(!Config::instance().local_test||!ready||GetCurrentThreadId()!=state.thread)
        return fail(error,"Character read requires its owned ready Mainthread");
    if(args.contains("action_uuid")){
        const auto id=args.at("action_uuid").get<std::string>();const auto found=operations.find(id);
        if(found==operations.end()||found->second.request.action!=a::Action::RenameCharacter||
           found->second.request.snapshot_uuid!=net::current_snapshot_id())return fail(error,"Character read lacks that retained current Rename operation");
        const auto& op=found->second;
        result={{"action_uuid",id},{"snapshot_uuid",op.request.snapshot_uuid},{"native_epoch",op.binding.native_epoch},
            {"native_generation",op.binding.native_generation},{"ordered",op.ordered},{"confirmed",op.confirmed},{"retired",op.retired},
            {"original_calls",op.calls},{"seed_attempted",op.seed_attempted},{"cause",op.cause},
            {"character_before",diagnostic_character(op.evidence.character_rename_before)},{"character_after",diagnostic_character(op.evidence.character_rename_after)},
            {"proof_before",op.before.proof},{"proof_after",op.after.proof},
            {"ledger_before",diagnostic_money(op.evidence.before)},{"ledger_after",diagnostic_money(op.evidence.after)}};return true;
    }
    a::Context context;a::Binding binding;character_definition_identity::Census definitions;character_membership_reader::Users users;
    if(!a::selected_session().current(context,error)||context.snapshot_uuid!=net::current_snapshot_id()||
       Config::instance().is_host!=(context.role==a::Role::Host)||!typed_action_runtime::network_binding(context.snapshot_uuid,binding,error)||
       !character_actor_records::read_current(context,binding,definitions,users,error))return false;
    Memory memory;json candidates=json::array();
    for(const auto& definition:definitions.definitions){
        if(definition.kind||definition.parent||definition.deleted||!definition.uid)continue;std::string name;
        if(!npc_quest_records::read_string(memory,definition.object,0xb0,"Character class candidate name",name,error))return false;
        const auto found=users.by_definition.find(definition.key);
        candidates.push_back({{"kind",definition.kind},{"id",definition.id},{"name",name},{"released",definition.released},
            {"current_users",found==users.by_definition.end()?size_t(0):found->second.size()}});
    }
    result={{"snapshot_uuid",context.snapshot_uuid},{"candidates",candidates},{"scope","Live base class candidates; original menu/card determines eligibility"}};return true;
}catch(const std::exception& exception){return fail(error,std::string("Character read boundary: ")+exception.what());}
catch(...){return fail(error,"Character read non-standard boundary exception");}
