namespace scenery_stroke {
namespace {
template<class Value>json operation_receipt(const Value& op){
    const auto& evidence=op.evidence;const auto& request=op.request;
    if(!evidence.scenery_stroke_before||!evidence.scenery_stroke_after||!evidence.scenery_stroke_result)return nullptr;
    auto ledger=[](const action::Ledger& v){return json{{"cash",v.cash},{"flat_day",v.flat_day},{"record_present",v.record_present},
        {"record_day",v.record_day},{"record_cash",v.record_cash},{"record_income",v.record_income},{"record_expense",v.record_expense},
        {"line_counts",v.line_counts},{"line_income",v.line_income}};};
    auto state=[](const action::SceneryStrokeState& v){return json{{"props_sha",v.props_sha},{"bugs_sha",v.bugs_sha},
        {"bug_count",v.bug_count},{"random",v.random},{"region_random",v.region_random},{"release",v.release}};};
    auto object=[](const action::DuplicateObject& v){return json{{"key",v.key},{"parent",v.parent},{"record_sha",v.record_sha},
        {"family",uint8_t(v.family)},{"region",v.region},{"pose",v.pose},{"members",v.members}};};
    auto objects=[&](const auto& values){json out=json::array();for(const auto& value:values)out.push_back(object(value));return out;};
    const auto& effect=*evidence.scenery_stroke_result;json changed=json::array();
    for(const auto& pair:effect.changed)changed.push_back(json::array({object(pair.before),object(pair.after)}));
    return {{"action_uuid",request.action_uuid},{"snapshot_uuid",request.snapshot_uuid},{"action",uint8_t(request.action)},
        {"origin_peer",request.peer},{"request_sequence",request.sequence},{"observed_revision",request.host_revision},
        {"ordered",op.ordered},{"confirmed",op.confirmed},{"retired",op.retired},
        {"region",request.scenery_stroke.region},{"mode",request.scenery_stroke.mode},{"stamps",request.scenery_stroke.stamps.size()},
        {"type",request.scenery_stroke.type},{"variant",request.scenery_stroke.variant},
        {"before",state(*evidence.scenery_stroke_before)},{"after",state(*evidence.scenery_stroke_after)},
        {"ledger_before",ledger(evidence.before)},{"ledger_after",ledger(evidence.after)},
        {"created",objects(effect.created)},{"removed",objects(effect.removed)},{"changed",std::move(changed)},{"untouched_sha",effect.untouched_sha}};
}
json ordered_receipt(){
    if(!Config::instance().local_test)return nullptr;
    const Operation* latest=nullptr;
    for(const auto& [id,op]:operations){(void)id;
        if(op.ordered&&op.confirmed&&(!latest||op.request.host_revision>latest->request.host_revision))latest=&op;
    }
    if(!latest)return nullptr;
    return operation_receipt(*latest);
}
}
}
