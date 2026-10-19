bool region_sale_effect(const RegionPropertyState& before,const RegionPropertyState& after,const Evidence& e){
    if(!before.owned||after.owned||before.type<=0||!before.actors||!after.actors||
       !before.appearance||!after.appearance||!e.undo_confirmed||
       !e.before.record_present||e.before.flat_day!=e.after.flat_day||e.before.record_day!=e.after.record_day)
        return false;
    auto expected=before;expected.owned=false;expected.status=0;expected.level=expected.max_level=0;
    expected.content.random=after.content.random;expected.type_random=after.type_random;expected.actors=after.actors;
    if(after!=expected||before.actors->counts!=after.actors->counts||
       before.actors->roster_sha256!=after.actors->roster_sha256||
       before.actors->expected_sha256!=after.actors->levels_sha256||
       after.actors->expected_sha256!=after.actors->levels_sha256)return false;
    const int64_t refund=int64_t(e.after.cash)-e.before.cash;if(refund<0||refund>INT32_MAX)return false;
    auto ledger=e.before;const auto add=[](int32_t& field,int64_t delta){const auto value=int64_t(field)+delta;
        if(value<INT32_MIN||value>INT32_MAX)return false;field=int32_t(value);return true;};
    return add(ledger.cash,refund)&&add(ledger.record_cash,refund)&&add(ledger.record_income,refund)&&
        add(ledger.line_counts[7],1)&&add(ledger.line_income[7],refund)&&ledger==e.after;
}
