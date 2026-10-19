bool network_history(const Request& r){return (r.action==Action::UndoBuilding||r.action==Action::RedoBuilding)&&uuid(r.target_action_uuid)&&r.canonical_record_key=="network:"+r.target_action_uuid;}
bool network_history_effect(const Request& r,const Evidence& e,std::string& error){
    auto fail=[&](const char* cause){error=cause;return false;};
    if(!network_history(r)||!e.network_history||!e.network_removal_before||!e.network_removal_after||
       !e.undo_confirmed||e.selection_confirmed||e.target_retired||e.object_before!=BuildingState{}||e.object_after!=BuildingState{}||
       e.region_before!=RegionState{}||e.region_after!=RegionState{}||e.terrain_before!=TerrainState{}||e.terrain_after!=TerrainState{}||
       e.path_before!=PathState{}||e.path_after!=PathState{}||e.network_before!=NetworkState{}||e.network_after!=NetworkState{}||!e.region_retired_sha256.empty())return fail("Network history action/evidence domain differs");
    const auto& proof=*e.network_history;const auto& a=*e.network_removal_before;const auto& b=*e.network_removal_after;
    if((proof.source!=8&&proof.source!=9&&proof.source!=129)||proof.amount<0||(proof.source!=9&&proof.junction)||
       !valid_network_removal(a)||!valid_network_removal(b)||a.graph.graph_sha256==b.graph.graph_sha256||a.identities_sha256==b.identities_sha256)return fail("Network history original source/value proof invalid");
    const bool destroying=(r.action==Action::UndoBuilding)!=(proof.source==129);
    const auto& large=destroying?a.graph:b.graph;const auto& small=destroying?b.graph:a.graph;
    if(proof.source==8){if(large.nodes!=small.nodes+1||large.pipes!=small.pipes||large.connections<small.connections)return fail("Network Node history full topology count differs");}
    else if(proof.source==9){if(large.pipes!=small.pipes+1||large.nodes!=small.nodes+uint64_t(proof.junction)||large.connections<small.connections)return fail("Network Pipe history full topology count differs");}
    else if(large.nodes<small.nodes||large.pipes<small.pipes||large.connections<small.connections||large.nodes+large.pipes<=small.nodes+small.pipes)return fail("Network Remove history full topology count differs");
    auto expected=e.before;const int64_t amount=destroying?int64_t(proof.amount):-int64_t(proof.amount);
    if(amount){auto add=[](int32_t& value,int64_t delta){const auto next=int64_t(value)+delta;if(next<INT32_MIN||next>INT32_MAX)return false;value=int32_t(next);return true;};
        if(!expected.record_present||!add(expected.cash,amount)||!add(expected.record_cash,amount)||!add(expected.record_income,amount)||!add(expected.line_counts[6],1)||!add(expected.line_income[6],amount))return fail("Network history native category6 cash inverse overflows");}
    if(expected!=e.after)return fail("Network history complete original category6 Ledger differs");error.clear();return true;
}
