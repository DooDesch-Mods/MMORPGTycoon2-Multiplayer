json zone_intent_json(const ZoneIntent& v){
    json stamps=json::array();for(const auto& s:v.stamps){json paths=json::array();for(const auto& path:s.shape){json points=json::array();for(const auto& p:path){points.push_back(p.x);points.push_back(p.z);}paths.push_back(std::move(points));}stamps.push_back({{"position",s.position},{"paths",std::move(paths)}});}
    return {{"region",v.region},{"character_type",v.character_type},{"level",v.level},{"stamps",std::move(stamps)}};
}
ZoneIntent zone_intent_read(const json& j){
    exact(j,{"region","character_type","level","stamps"});ZoneIntent v;v.region=i32(j.at("region"));v.character_type=i32(j.at("character_type"));v.level=i32(j.at("level"));
    const auto& stamps=j.at("stamps");if(!stamps.is_array()||stamps.size()>64)throw std::runtime_error("Zone stamps require bounded array");
    for(const auto& row:stamps){exact(row,{"position","paths"});ZoneStamp s;const auto& pos=row.at("position");const auto& paths=row.at("paths");
        if(!pos.is_array()||pos.size()!=3||!paths.is_array())throw std::runtime_error("Zone position/paths arrays differ");
        for(size_t i=0;i<3;++i)s.position[i]=scalar(pos[i]);
        for(const auto& p:paths){if(!p.is_array()||p.size()%2)throw std::runtime_error("Zone path requires int64 pairs");TerrainPath path;for(size_t i=0;i<p.size();i+=2)path.push_back({terrain_i64(p[i]),terrain_i64(p[i+1])});s.shape.push_back(std::move(path));}
        v.stamps.push_back(std::move(s));
    }
    if(!valid_zone_intent(v))throw std::runtime_error("Zone intent outside native domain");return v;
}
json zone_state_json(const ZoneState& v){return {{"zones",v.zones},{"monsters",v.monsters},{"undo_count",v.undo_count},
    {"next_tag",v.next_tag},{"created_monsters",v.created_monsters},{"global_random",v.global_random},{"region_random",v.region_random},{"release_edits",v.release_edits},{"content_sha256",v.content_sha256}};}
ZoneState zone_state_read(const json& j){
    exact(j,{"zones","monsters","undo_count","next_tag","created_monsters","global_random","region_random","release_edits","content_sha256"});ZoneState v;
    auto count=[&](const char* name){const auto n=u64(j.at(name));if(n>UINT32_MAX)throw std::runtime_error("Zone count exceeds uint32");return uint32_t(n);};
    v.zones=count("zones");v.monsters=count("monsters");v.undo_count=count("undo_count");v.next_tag=i32(j.at("next_tag"));v.created_monsters=i32(j.at("created_monsters"));v.content_sha256=j.at("content_sha256").get<std::string>();
    for(const auto* name:{"global_random","region_random"}){const auto& a=j.at(name);if(!a.is_array()||a.size()!=4)throw std::runtime_error("Zone RNG requires four uint64 words");auto& out=std::string(name)=="global_random"?v.global_random:v.region_random;for(size_t i=0;i<4;++i)out[i]=u64(a[i]);}
    const auto& edits=j.at("release_edits");if(!edits.is_array()||edits.size()!=8)throw std::runtime_error("Zone Release edits requires eight integers");for(size_t i=0;i<8;++i)v.release_edits[i]=i32(edits[i]);
    if(!valid_zone_state(v))throw std::runtime_error("Zone state outside domain");return v;
}
