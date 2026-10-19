action::Outcome ownership_history_perform(Runtime& r,const action::Binding& binding,const action::Request& request,
                                          const PropertyOperation& source,const action::Evidence* authority,std::string& e){
    if(!source.ownership_before||!source.ownership_after||!source.ownership_graph){
        e="Region ownership history lacks complete confirmed original source";return action::Outcome::RejectedNoEffect;}
    const bool undo_direction=request.action==action::Action::UndoBuilding;
    const auto& destination=undo_direction?*source.ownership_before:*source.ownership_after;
    const auto& origin=undo_direction?*source.ownership_after:*source.ownership_before;
    const std::array<int32_t,2> range{destination.level,destination.max_level};
    const int64_t original_amount=int64_t(source.evidence.before.cash)-source.evidence.after.cash;
    const int64_t amount=undo_direction?original_amount:-original_amount;
    if(amount<=INT32_MIN||amount>INT32_MAX||range[0]<0||range[1]<range[0]||destination.owned==origin.owned){
        e="Region ownership history signed cash/range/direction invalid";return action::Outcome::RejectedNoEffect;}
    action::RegionPropertyState before;LocalView view;uintptr_t global=0,graph=0;action::Ledger money,expected_money;
    if(!ownership_read(r,source.request.region_property.id,0,range,before,view,global,e)||
       !bound(view,binding,request.snapshot_uuid,e)||!ledger(money,e)||
       !building_history_slice::graph_for(request.action,request.target_action_uuid,graph,e)||
       !ownership_inverse(r,graph,view.region,destination,int32_t(amount),source.request.region_property.hit,e)||
       !building_history_slice::cash_effect(money,int32_t(amount),expected_money,e))return action::Outcome::RejectedNoEffect;
    if(before.owned!=origin.owned||before.type!=origin.type||before.level!=origin.level||before.max_level!=origin.max_level){
        e="Region ownership history current owner/type/range differs from original source";return action::Outcome::RejectedNoEffect;}
    if(authority&&(!authority->property_before.actors||!authority->property_after.actors||
       !authority->property_before.appearance||!authority->property_after.appearance||
       !authority->undo_confirmed||authority->selection_confirmed||authority->target_retired||
       authority->before!=money||authority->after!=expected_money||
       !property_same_before(before,authority->property_before)||authority->property_after.owned!=destination.owned||
       authority->property_after.level!=range[0]||authority->property_after.max_level!=range[1])){
        e="Region ownership history Client complete before/result differs from Host";return action::Outcome::RejectedNoEffect;}
    RegionHistoryOperation value;value.binding=binding;value.request=request;value.region_id=source.request.region_property.id;
    value.range=range;value.ownership=true;value.evidence.before=money;value.evidence.property_before=authority?authority->property_before:before;
    auto& op=region_history_operations.emplace(request.action_uuid,std::move(value)).first->second;
    auto unknown=[&](const std::string& cause){e=op.cause=cause;return action::Outcome::Unknown;};
    if(authority){
        std::memcpy(reinterpret_cast<void*>(view.random),authority->property_before.content.random.data(),32);
        std::memcpy(reinterpret_cast<void*>(view.region+0x268),authority->property_before.type_random.data(),32);
        std::memcpy(reinterpret_cast<void*>(global),authority->property_before.actors->global_random.data(),32);
    }
    op.attempted=true;const auto outcome=building_history_slice::perform(request.action,request.target_action_uuid,e);
    if(outcome!=action::Outcome::Confirmed){op.cause=e;return authority||outcome==action::Outcome::Unknown?unknown(e):outcome;}
    const auto inverse=undo_direction?action::Action::RedoBuilding:action::Action::UndoBuilding;
    action::RegionPropertyState after;LocalView after_view;uintptr_t after_global=0;
    if(!building_history_slice::graph_for(inverse,request.target_action_uuid,op.inverse_graph,e)||
       !ownership_inverse(r,op.inverse_graph,view.region,before,int32_t(-amount),source.request.region_property.hit,e)||
       !ownership_read(r,op.region_id,view.region,range,after,after_view,after_global,e)||
       !bound(after_view,binding,request.snapshot_uuid,e)||!ledger(op.evidence.after,e))return unknown(e);
    if(after_view.region!=view.region||after_global!=global||!sale_members(view,after_view,e))
        return unknown(e.empty()?"Region ownership history current native owner changed":e);
    op.evidence.property_after=after;op.evidence.undo_confirmed=true;
    if(!sha256(json::array().dump(),op.evidence.region_retired_sha256,e))return unknown(e);
    // ConfigureRegion clamps the current actors and keeps their identities.
    auto expected=op.evidence.property_before;expected.owned=destination.owned;expected.status=destination.owned?1:0;
    expected.level=range[0];expected.max_level=range[1];expected.content.random=after.content.random;
    expected.type_random=after.type_random;expected.actors=after.actors;
    if(after!=expected||before.actors->counts!=after.actors->counts||before.actors->roster_sha256!=after.actors->roster_sha256||
       before.actors->expected_sha256!=after.actors->levels_sha256||after.actors->expected_sha256!=after.actors->levels_sha256||
       op.evidence.after!=expected_money)return unknown("Region ownership history original clamp/membership/Ledger consequence differs");
    if(authority&&(op.evidence.property_after!=authority->property_after||op.evidence.after!=authority->after||
       op.evidence.region_retired_sha256!=authority->region_retired_sha256))return unknown("Region ownership history original result differs from Host");
    op.confirmed=true;return action::Outcome::Confirmed;
}
