int64_t terrain_i64(const json& j) {
    if(!j.is_number_integer()||(j.is_number_unsigned()&&j.get<uint64_t>()>uint64_t(INT64_MAX)))throw std::runtime_error("terrain coordinate requires int64");
    return j.get<int64_t>();
}
json terrain_json(const TerrainIntent& v) {
    if(v.height){const auto& h=*v.height;return {{"id",v.id},{"height",{{"up",h.up},{"hit",h.hit},{"picked",h.picked},{"previous",h.previous},{"radius",h.radius},{"cell",h.cell},{"before_sha256",h.before_sha256}}}};}
    json stamps=json::array();
    for(const auto& s:v.stamps) {
        json paths=json::array();
        for(const auto& path:s.shape) {
            json points=json::array();for(const auto& p:path){points.push_back(p.x);points.push_back(p.z);}paths.push_back(std::move(points));
        }
        stamps.push_back({{"position",s.position},{"radius",s.radius},{"paths",std::move(paths)}});
    }
    return {{"id",v.id},{"style",v.style},{"start_label",v.start_label},{"initial_area",v.initial_area},{"reset_after",v.reset_after},{"finish",uint64_t(v.finish)},{"stamps",std::move(stamps)}};
}
TerrainIntent terrain_read(const json& j) {
    if(j.contains("height")){
        exact(j,{"id","height"});TerrainIntent v;v.id=i32(j.at("id"));const auto& h=j.at("height");exact(h,{"up","hit","picked","previous","radius","cell","before_sha256"});TerrainHeightIntent height;
        if(!h.at("up").is_boolean())throw std::runtime_error("terrain height up requires bool");height.up=h.at("up").get<bool>();
        auto position=[](const json& a,std::array<float,3>& out){if(!a.is_array()||a.size()!=3)throw std::runtime_error("terrain height exact three floats required");for(size_t i=0;i<3;++i)out[i]=scalar(a[i]);};
        position(h.at("hit"),height.hit);position(h.at("picked"),height.picked);position(h.at("previous"),height.previous);height.radius=scalar(h.at("radius"));height.cell=i32(h.at("cell"));height.before_sha256=h.at("before_sha256").get<std::string>();v.height=height;
        if(!valid_terrain(v))throw std::runtime_error("terrain height intent outside native domain");return v;
    }
    exact(j,{"id","style","start_label","initial_area","reset_after","finish","stamps"});TerrainIntent v;
    v.id=i32(j.at("id"));v.style=i32(j.at("style"));v.start_label=j.at("start_label").get<std::string>();
    v.initial_area=scalar(j.at("initial_area"));const auto reset=u64(j.at("reset_after"));if(reset>UINT32_MAX)throw std::runtime_error("terrain area reset boundary invalid");v.reset_after=uint32_t(reset);
    const auto finish=u64(j.at("finish"));if(finish>2)throw std::runtime_error("terrain completion enum invalid");v.finish=uint8_t(finish);
    if(!j.at("stamps").is_array())throw std::runtime_error("terrain stamps require array");
    for(const auto& value:j.at("stamps")) {
        exact(value,{"position","radius","paths"});TerrainStamp stamp;
        if(!value.at("position").is_array()||value.at("position").size()!=3||!value.at("paths").is_array())throw std::runtime_error("terrain position/path array differs");
        for(size_t i=0;i<3;++i)stamp.position[i]=scalar(value.at("position")[i]);stamp.radius=scalar(value.at("radius"));
        for(const auto& points:value.at("paths")) {
            if(!points.is_array()||points.size()%2)throw std::runtime_error("terrain path requires paired int64 coordinates");
            TerrainPath path;for(size_t i=0;i<points.size();i+=2)path.push_back({terrain_i64(points[i]),terrain_i64(points[i+1])});
            stamp.shape.push_back(std::move(path));
        }
        v.stamps.push_back(std::move(stamp));
    }
    if(!valid_terrain(v))throw std::runtime_error("terrain intent outside native domain");return v;
}
bool valid_terrain_state(const TerrainState& s) {
    return s==TerrainState{}||(digest(s.contours_sha256)&&digest(s.bugs_sha256)&&s.bug_count<=1000000&&(s.heights_sha256.empty()?(s.height_cell==-1&&s.height_bits==0):(digest(s.heights_sha256)&&s.height_cell>=0&&s.height_cell<1000000)));
}
json terrain_state_json(const TerrainState& s) {
    json out={{"global_random",s.global_random},{"release_edits",s.release_edits},{"contours_sha256",s.contours_sha256},{"bugs_sha256",s.bugs_sha256},{"bug_count",s.bug_count}};
    if(!s.heights_sha256.empty()){out["heights_sha256"]=s.heights_sha256;out["height_cell"]=s.height_cell;out["height_bits"]=s.height_bits;}return out;
}
TerrainState terrain_state_read(const json& j) {
    TerrainState s;
    if(j.contains("heights_sha256")){exact(j,{"global_random","release_edits","contours_sha256","bugs_sha256","bug_count","heights_sha256","height_cell","height_bits"});s.heights_sha256=j.at("heights_sha256").get<std::string>();s.height_cell=i32(j.at("height_cell"));const auto bits=u64(j.at("height_bits"));if(bits>UINT32_MAX||s.heights_sha256.empty())throw std::runtime_error("terrain height native bits/digest invalid");s.height_bits=uint32_t(bits);}
    else exact(j,{"global_random","release_edits","contours_sha256","bugs_sha256","bug_count"});
    if(!j.at("global_random").is_array()||j.at("global_random").size()!=4||!j.at("release_edits").is_array()||j.at("release_edits").size()!=8)throw std::runtime_error("terrain state arrays differ");
    for(size_t i=0;i<4;++i)s.global_random[i]=u64(j.at("global_random")[i]);
    for(size_t i=0;i<8;++i)s.release_edits[i]=i32(j.at("release_edits")[i]);
    s.contours_sha256=j.at("contours_sha256").get<std::string>();s.bugs_sha256=j.at("bugs_sha256").get<std::string>();s.bug_count=u64(j.at("bug_count"));
    if(!valid_terrain_state(s))throw std::runtime_error("terrain state outside domain");return s;
}
