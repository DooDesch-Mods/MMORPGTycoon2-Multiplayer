json network_pipe_json(const NetworkPipeIntent& p){return {{"region",p.region},{"type",p.type},{"key",p.key},{"node_type",p.node_type},{"graph_before_sha256",p.graph_before_sha256},{"start_node",p.start_node},{"end_node",p.end_node},{"start",p.start},{"end",p.end},{"quoted_length",p.quoted_length},{"quoted_cost",p.quoted_cost},{"preview",p.preview},{"splines",p.splines}};}
NetworkPipeIntent network_pipe_read(const json& j){
    exact(j,{"region","type","key","node_type","graph_before_sha256","start_node","end_node","start","end","quoted_length","quoted_cost","preview","splines"});NetworkPipeIntent p;
    p.region=i32(j.at("region"));p.start_node=i32(j.at("start_node"));p.end_node=i32(j.at("end_node"));p.quoted_cost=i32(j.at("quoted_cost"));p.quoted_length=scalar(j.at("quoted_length"));
    p.type=j.at("type").get<std::string>();p.key=j.at("key").get<std::string>();p.node_type=j.at("node_type").get<std::string>();p.graph_before_sha256=j.at("graph_before_sha256").get<std::string>();
    auto point=[&](const json& a){if(!a.is_array()||a.size()!=3)throw std::runtime_error("network pipe point requires three coordinates");NetworkPoint v;for(size_t i=0;i<3;++i)v[i]=scalar(a[i]);return v;};
    p.start=point(j.at("start"));p.end=point(j.at("end"));
    const auto& preview=j.at("preview");if(!preview.is_array()||preview.size()>2048)throw std::runtime_error("network pipe preview outside wire bound");for(const auto& v:preview)p.preview.push_back(point(v));
    const auto& splines=j.at("splines");if(!splines.is_array()||splines.size()!=1)throw std::runtime_error("network pipe requires one original span");
    for(const auto& a:splines){if(!a.is_array()||a.size()!=12)throw std::runtime_error("network pipe spline requires twelve scalars");std::array<float,12> v;for(size_t i=0;i<12;++i)v[i]=scalar(a[i]);p.splines.push_back(v);}
    if(!valid_network_pipe(p))throw std::runtime_error("network pipe intent invalid");return p;
}
