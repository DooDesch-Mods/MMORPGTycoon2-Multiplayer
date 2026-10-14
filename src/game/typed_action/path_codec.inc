bool valid_path_state(const PathState& p) {
    return p==PathState{}||(p.paths<=1000000&&p.nodes<=1000000&&p.cost>=0&&digest(p.graph_sha256));
}
json path_delete_json(const PathDeleteIntent& p){return {{"region",p.region},{"path_index",p.path_index},{"graph_before_sha256",p.graph_before_sha256}};}
PathDeleteIntent path_delete_read(const json& j){
    exact(j,{"region","path_index","graph_before_sha256"});PathDeleteIntent p;
    p.region=i32(j.at("region"));p.path_index=i32(j.at("path_index"));p.graph_before_sha256=j.at("graph_before_sha256").get<std::string>();
    if(!valid_path_delete(p))throw std::runtime_error("path delete intent invalid");return p;
}
json path_json(const PathIntent& p){json out={{"region",p.region},{"start",p.start},{"end",p.end},{"quoted_length",p.quoted_length},{"spline",p.spline},{"preview",p.preview}};
    if(path_connected(p)){out["start_node"]=p.start_node;out["end_node"]=p.end_node;out["graph_before_sha256"]=p.graph_before_sha256;}return out;}
PathIntent path_read(const json& j,bool connected) {
    PathIntent p;
    if(connected){exact(j,{"region","start","end","quoted_length","spline","preview","start_node","end_node","graph_before_sha256"});
        p.start_node=i32(j.at("start_node"));p.end_node=i32(j.at("end_node"));p.graph_before_sha256=j.at("graph_before_sha256").get<std::string>();
        if(!path_connected(p))throw std::runtime_error("connected path schema requires existing endpoint");
    }else exact(j,{"region","start","end","quoted_length","spline","preview"});
    p.region=i32(j.at("region"));p.quoted_length=scalar(j.at("quoted_length"));
    auto point=[](const json& j){if(!j.is_array()||j.size()!=3)throw std::runtime_error("path point requires3 coordinates");PathPoint out;for(size_t i=0;i<3;++i)out[i]=scalar(j[i]);return out;};
    p.start=point(j.at("start"));p.end=point(j.at("end"));
    if(!j.at("spline").is_array()||j.at("spline").size()!=12)throw std::runtime_error("path spline requires12 scalars");
    for(size_t i=0;i<12;++i)p.spline[i]=scalar(j.at("spline")[i]);
    if(!j.at("preview").is_array()||j.at("preview").size()>128)throw std::runtime_error("path preview bound exceeded");
    for(const auto& v:j.at("preview"))p.preview.push_back(point(v));
    if(!valid_path(p))throw std::runtime_error("path intent invalid");return p;
}
json path_state_json(const PathState& p){return {{"paths",p.paths},{"nodes",p.nodes},{"graph_sha256",p.graph_sha256},{"cost",p.cost}};}
PathState path_state_read(const json& j) {
    exact(j,{"paths","nodes","graph_sha256","cost"});PathState p;
    p.paths=u64(j.at("paths"));p.nodes=u64(j.at("nodes"));p.graph_sha256=j.at("graph_sha256").get<std::string>();p.cost=i32(j.at("cost"));
    if(!valid_path_state(p))throw std::runtime_error("path state invalid");return p;
}
