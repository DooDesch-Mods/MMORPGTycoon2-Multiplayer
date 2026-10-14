bool terrain_height_effect(const Request& request,const Evidence& evidence,std::string& error){
    auto fail=[&](const char* cause){error=cause;return false;};
    if(request.action!=Action::TerrainStroke||!request.terrain.height||!valid_terrain(request.terrain))return fail("height effect requires exact typed height action");
    const auto& h=*request.terrain.height;const auto& a=evidence.terrain_before;const auto& b=evidence.terrain_after;
    auto desired=evidence.before;
    auto add=[](int32_t& field,int32_t delta){const auto value=int64_t(field)+delta;if(value<INT32_MIN||value>INT32_MAX)return false;field=int32_t(value);return true;};
    if(desired.cash<100||desired.record_present!=1||!add(desired.cash,-100)||!add(desired.record_cash,-100)||!add(desired.record_expense,100)||!add(desired.line_counts[6],1)||!add(desired.line_income[6],-100))return fail("height original full Ledger overflow/cash admission differs");
    if(evidence.after!=desired)return fail("height original SubtractCash100/category6 full Ledger differs");
    if(!valid_region(evidence.region_before)||evidence.region_before!=evidence.region_after||evidence.region_before.id!=request.terrain.id||
       evidence.object_before!=BuildingState{}||evidence.object_after!=BuildingState{}||evidence.target_retired||!evidence.undo_confirmed||evidence.selection_confirmed||!digest(evidence.region_retired_sha256))return fail("height original Region/unchanged Undo/retirement receipt differs");
    if(!valid_terrain_state(a)||!valid_terrain_state(b)||!digest(a.heights_sha256)||!digest(b.heights_sha256)||a.heights_sha256!=h.before_sha256||a.height_cell!=h.cell||b.height_cell!=h.cell)return fail("height original before/after digest or selected cell differs");
    const float before=std::bit_cast<float>(a.height_bits),after=std::bit_cast<float>(b.height_bits),wanted=before+(h.up?20.0f:-20.0f);
    if(!std::isfinite(before)||!std::isfinite(after)||!std::isfinite(wanted)||b.height_bits!=std::bit_cast<uint32_t>(wanted)||a.heights_sha256==b.heights_sha256)return fail("height original selected cell +/-20 consequence differs");
    if(a.contours_sha256!=b.contours_sha256||a.bugs_sha256!=b.bugs_sha256||a.bug_count!=b.bug_count)return fail("height original unrelated Terrain content differs");
    error.clear();return true;
}
