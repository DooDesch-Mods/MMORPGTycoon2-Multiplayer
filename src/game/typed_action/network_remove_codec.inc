json network_removal_json(const NetworkRemoval& v){return {{"nodes",v.graph.nodes},{"pipes",v.graph.pipes},{"connections",v.graph.connections},{"graph_sha256",v.graph.graph_sha256},{"identities_sha256",v.identities_sha256}};}
NetworkRemoval parse_network_removal(const json& j){exact(j,{"nodes","pipes","connections","graph_sha256","identities_sha256"});NetworkRemoval v;
    v.graph={u64(j.at("nodes")),u64(j.at("pipes")),u64(j.at("connections")),j.at("graph_sha256").get<std::string>()};
    v.identities_sha256=j.at("identities_sha256").get<std::string>();
    if(!valid_network_removal(v))throw std::runtime_error("Network removal complete canonical census invalid");return v;
}
