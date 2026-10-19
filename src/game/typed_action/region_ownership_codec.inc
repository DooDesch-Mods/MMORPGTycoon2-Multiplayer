bool region_ownership_effect(const Request& request,const Evidence& e){
    const auto& before=e.property_before;const auto& after=e.property_after;
    const auto id=game::region_history_key::parse(request.canonical_record_key);
    if(!id||!valid_property_state(before)||!valid_property_state(after)||before.content.id!=*id||after.content.id!=*id||
       before.type<=0||before.owned==after.owned||!before.actors||!after.actors||!before.appearance||!after.appearance||
       !e.undo_confirmed||e.target_retired||e.selection_confirmed||!digest(e.region_retired_sha256)||
       e.object_before!=BuildingState{}||e.object_after!=BuildingState{}||after.level<0||after.max_level<after.level||
       (after.owned?(after.level<1):(after.level!=0||after.max_level!=0)))return false;
    auto expected=before;expected.owned=after.owned;expected.status=after.owned?1:0;
    expected.level=after.level;expected.max_level=after.max_level;
    expected.content.random=after.content.random;expected.type_random=after.type_random;expected.actors=after.actors;
    if(after!=expected||before.actors->counts!=after.actors->counts||before.actors->roster_sha256!=after.actors->roster_sha256||
       before.actors->expected_sha256!=after.actors->levels_sha256||after.actors->expected_sha256!=after.actors->levels_sha256)return false;
    const int64_t amount=int64_t(e.after.cash)-e.before.cash;
    if(amount<=INT32_MIN||amount>INT32_MAX||(after.owned?amount>0:amount<0))return false;
    auto money=e.before;
    if(amount){
        if(!money.record_present)return false;
        const auto add=[](int32_t& field,int64_t delta){const auto value=int64_t(field)+delta;
            if(value<INT32_MIN||value>INT32_MAX)return false;field=int32_t(value);return true;};
        // DoCash uses signed AddCash in category6 for both directions.
        if(!add(money.cash,amount)||!add(money.record_cash,amount)||!add(money.record_income,amount)||
           !add(money.line_counts[6],1)||!add(money.line_income[6],amount))return false;
    }
    return money==e.after;
}
