bool valid_network_state(const NetworkState& p) {
    return p==NetworkState{}||(p.nodes<=1000000&&p.pipes<=1000000&&p.connections<=1000000&&digest(p.graph_sha256));
}
json network_json(const NetworkIntent& p){return {{"region",p.region},{"type",p.type},{"key",p.key},{"position",p.position},{"quoted_cost",p.quoted_cost}};}
NetworkIntent network_read(const json& j) {
    exact(j,{"region","type","key","position","quoted_cost"});NetworkIntent p;
    p.region=i32(j.at("region"));p.type=j.at("type").get<std::string>();p.key=j.at("key").get<std::string>();p.quoted_cost=i32(j.at("quoted_cost"));
    const auto& a=j.at("position");if(!a.is_array()||a.size()!=3)throw std::runtime_error("network position requires three coordinates");
    for(size_t i=0;i<3;++i)p.position[i]=scalar(a[i]);
    if(!valid_network(p))throw std::runtime_error("network intent invalid");return p;
}
json network_state_json(const NetworkState& p){return {{"nodes",p.nodes},{"pipes",p.pipes},{"connections",p.connections},{"graph_sha256",p.graph_sha256}};}
NetworkState network_state_read(const json& j) {
    exact(j,{"nodes","pipes","connections","graph_sha256"});NetworkState p;
    p.nodes=u64(j.at("nodes"));p.pipes=u64(j.at("pipes"));p.connections=u64(j.at("connections"));p.graph_sha256=j.at("graph_sha256").get<std::string>();
    if(!valid_network_state(p))throw std::runtime_error("network state invalid");return p;
}
