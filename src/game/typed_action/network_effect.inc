bool network_effect(const Request& request,const Evidence& e,std::string& error) {
    auto fail=[&](const char* why){error=why;return false;};
    const auto& a=e.before;const auto& b=e.after;const int64_t price=request.network.quoted_cost;
    if(request.action!=Action::PlaceNetworkNode||!valid_network(request.network)||!e.undo_confirmed||e.target_retired||e.selection_confirmed||
       e.object_before!=BuildingState{}||e.object_after!=BuildingState{}||e.region_before!=RegionState{}||e.region_after!=RegionState{}||
       e.terrain_before!=TerrainState{}||e.terrain_after!=TerrainState{}||e.path_before!=PathState{}||e.path_after!=PathState{}||!e.region_retired_sha256.empty())return fail("network receipt action/object flags differ");
    if(e.network_after.nodes!=e.network_before.nodes+1||e.network_after.pipes!=e.network_before.pipes||e.network_before.connections!=e.network_after.connections||
       e.network_before.graph_sha256.size()!=64||e.network_after.graph_sha256.size()!=64||e.network_before.graph_sha256==e.network_after.graph_sha256)return fail("network node topology delta differs");
    if(!a.record_present||!b.record_present||a.flat_day!=b.flat_day||a.record_day!=b.record_day||a.record_income!=b.record_income||
       int64_t(a.cash)-b.cash!=price||int64_t(a.record_cash)-b.record_cash!=price||int64_t(b.record_expense)-a.record_expense!=price)return fail("network native cash/day record delta differs from original cost");
    for(size_t i=0;i<20;++i)if(int64_t(b.line_counts[i])-a.line_counts[i]!=(i==6?1:0)||int64_t(b.line_income[i])-a.line_income[i]!=(i==6?-price:0))return fail("network native category6 ledger delta differs");
    return true;
}
