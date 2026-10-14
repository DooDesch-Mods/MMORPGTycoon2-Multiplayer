bool path_delete_effect(const Request& request,const Evidence& e,std::string& error) {
    auto fail=[&](const char* why){error=why;return false;};
    if(request.action!=Action::RemovePath||!valid_path_delete(request.path_delete)||!e.undo_confirmed||!e.target_retired||!e.selection_confirmed||e.object_before!=BuildingState{}||e.object_after!=BuildingState{}||!e.region_retired_sha256.empty())return fail("path delete receipt flags differ");
    if(e.path_before.graph_sha256!=request.path_delete.graph_before_sha256||e.path_after.graph_sha256.size()!=64||e.path_before.cost||e.path_after.cost<0||uint64_t(request.path_delete.path_index)>=e.path_before.paths||e.path_after.paths+1!=e.path_before.paths||e.path_after.nodes>e.path_before.nodes||e.path_before.nodes-e.path_after.nodes>2)return fail("path delete graph/count/cost receipt differs");
    const auto& a=e.before;const auto& b=e.after;const int64_t refund=e.path_after.cost;
    if(!a.record_present||!b.record_present||a.flat_day!=b.flat_day||a.record_day!=b.record_day||a.record_expense!=b.record_expense||int64_t(b.cash)-a.cash!=refund||int64_t(b.record_cash)-a.record_cash!=refund||int64_t(b.record_income)-a.record_income!=refund)return fail("path delete native refund record differs");
    for(size_t i=0;i<20;++i)if(int64_t(b.line_counts[i])-a.line_counts[i]!=(i==6&&refund?1:0)||int64_t(b.line_income[i])-a.line_income[i]!=(i==6?refund:0))return fail("path delete native category6 refund differs");
    if(e.region_before.id!=request.path_delete.region||!same_region_content(e.region_before,e.region_after)||e.terrain_before.contours_sha256!=e.terrain_after.contours_sha256)return fail("path delete region/scenery/contour content changed");
    return true;
}
bool path_effect(const Request& request,const Evidence& e,std::string& error) {
    auto fail=[&](const char* why){error=why;return false;};
    const auto& a=e.before;const auto& b=e.after;const int64_t price=e.path_after.cost;
    if(request.action!=Action::PlacePath||!valid_path(request.path)||!e.undo_confirmed||e.target_retired||e.selection_confirmed||e.object_before!=BuildingState{}||e.object_after!=BuildingState{}||!e.region_retired_sha256.empty())return fail("path receipt action/object flags differ");
    if(e.path_before.graph_sha256.size()!=64||e.path_after.graph_sha256.size()!=64||e.path_before.cost||price<0||e.path_after.paths!=e.path_before.paths+1||e.path_after.nodes!=e.path_before.nodes+path_new_nodes(request.path))return fail("path graph count/cost receipt differs");
    if(path_connected(request.path)&&e.path_before.graph_sha256!=request.path.graph_before_sha256)return fail("path receipt differs from captured endpoint graph");
    if(!a.record_present||!b.record_present||a.flat_day!=b.flat_day||a.record_day!=b.record_day||a.record_income!=b.record_income||int64_t(a.cash)-b.cash!=price||int64_t(a.record_cash)-b.record_cash!=price||int64_t(b.record_expense)-a.record_expense!=price)return fail("path native cash/day record delta differs from original CostOf");
    for(size_t i=0;i<20;++i)if(int64_t(b.line_counts[i])-a.line_counts[i]!=(i==6?1:0)||int64_t(b.line_income[i])-a.line_income[i]!=(i==6?-price:0))return fail("path native category6 ledger delta differs");
    if(e.region_before.id!=request.path.region||!same_region_content(e.region_before,e.region_after)||e.terrain_before.contours_sha256!=e.terrain_after.contours_sha256)return fail("path region/contour/scenery content changed");
    // Original SpawnBugs can append a bug, change an existing bug, or do nothing.
    // The client must reproduce the complete Host terrain_after digest and RNG.
    if(e.terrain_after.bug_count<e.terrain_before.bug_count)return fail("path original SpawnBugs removed existing bugs");
    for(size_t i=0;i<8;++i){const int64_t delta=int64_t(e.terrain_after.release_edits[i])-e.terrain_before.release_edits[i];if(i==3?delta<=0:delta!=0)return fail("path original Release edit3 consequence differs");}
    return true;
}
bool path_receipt_fits(const Request& request,const Ledger& money,const RegionState& region,std::string& error) {
    Message worst;worst.kind=Kind::Completion;worst.outcome=Outcome::Unknown;worst.session=worst.revision=UINT64_MAX;worst.required_mask=15;worst.request=request;
    worst.request.sequence=worst.request.host_revision=UINT64_MAX;worst.cause=std::string(1024,'\x01');
    auto& e=worst.evidence;e.before=e.after=money;
    for(auto* m:{&e.before,&e.after}){m->cash=m->flat_day=m->record_day=m->record_cash=m->record_income=m->record_expense=INT32_MIN;m->line_counts.fill(INT32_MIN);m->line_income.fill(INT32_MIN);}
    e.region_before=region;e.region_before.random.fill(UINT64_MAX);e.region_before.scenery_count=1000000;e.region_after=e.region_before;
    e.terrain_before.global_random.fill(UINT64_MAX);e.terrain_before.release_edits.fill(INT32_MIN);e.terrain_before.contours_sha256=e.terrain_before.bugs_sha256=std::string(64,'f');e.terrain_before.bug_count=1000000;e.terrain_after=e.terrain_before;
    e.path_before={1000000,1000000,std::string(64,'f'),INT32_MAX};e.path_after=e.path_before;std::vector<uint8_t> bytes;
    if(!encode(worst,bytes,error)){error="path receipt exceeds transport before native effect: "+error;return false;}return true;
}
