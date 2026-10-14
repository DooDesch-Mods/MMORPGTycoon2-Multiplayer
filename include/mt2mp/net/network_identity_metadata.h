#pragma once
#include <cstdint>
#include <optional>
#include <ostream>
#include <streambuf>
#include <string>
#include "vendor/nlohmann/json.hpp"

namespace mt2mp::net::network_identity_metadata {
inline constexpr uint32_t maximum_nodes=65536,maximum_control_bytes=8192;
enum class Role { Unknown,Host,Client };
enum class Disposition { Rejected,LegacyFileOnly,IdentityAttached };
struct Metadata { uint32_t expected_nodes=0; std::string keys_sha256; };
struct Receipt { bool ready=false; Role role=Role::Unknown; std::string uuid,snapshot_sha256,keys_sha256; uint64_t node_count=0; };
using Json=nlohmann::json;

namespace detail {
inline bool fail(std::string& error,const std::string& cause) { error=cause; return false; }
inline bool sha(const std::string& value) { return value.size()==64&&value.find_first_not_of("0123456789abcdef")==std::string::npos; }
inline bool uuid(const std::string& value) {
    if(value.size()!=36) return false;
    for(size_t index=0;index<value.size();++index) {
        const auto c=value[index];
        if(index==8||index==13||index==18||index==23) { if(c!='-') return false; }
        else if(!((c>='0'&&c<='9')||(c>='a'&&c<='f'))) return false;
    }
    return true;
}
inline bool unsigned_integer(const Json& value,uint64_t& output) {
    if(value.is_number_unsigned()) { output=value.get<uint64_t>(); return true; }
    if(value.is_number_integer()) { const auto number=value.get<int64_t>(); if(number<0) return false; output=uint64_t(number); return true; }
    return false;
}
class Counter:public std::streambuf {
    uint32_t count=0;
    std::streamsize xsputn(const char*,std::streamsize bytes) override {
        if(bytes<0||uint64_t(bytes)>maximum_control_bytes-count) throw std::length_error("snapshot control exceeds8192 bytes");
        count+=uint32_t(bytes); return bytes;
    }
    int_type overflow(int_type byte) override {
        if(traits_type::eq_int_type(byte,traits_type::eof())) return traits_type::not_eof(byte);
        if(count==maximum_control_bytes) throw std::length_error("snapshot control exceeds8192 bytes");
        ++count; return byte;
    }
};
inline bool bounded(const Json& value,std::string& error) {
    try { Counter buffer; std::ostream stream(&buffer); stream.exceptions(std::ios::badbit|std::ios::failbit); stream<<value; return true; }
    catch(const std::exception& cause) { return fail(error,std::string("snapshot control serialization rejected: ")+cause.what()); }
}
inline bool base(const Json& value,const char* type,std::string& id,std::string& hash,std::string& error,bool require_hash=true) {
    if(!value.is_object()||!value.contains("type")||!value.at("type").is_string()||value.at("type")!=type||!value.contains("id")||!value.at("id").is_string()) return fail(error,"snapshot control type/id shape invalid");
    id=value.at("id").get<std::string>(); if(!uuid(id)) return fail(error,"snapshot control UUID invalid");
    if(require_hash) {
        if(!value.contains("sha256")||!value.at("sha256").is_string()) return fail(error,"snapshot control file SHA shape invalid");
        hash=value.at("sha256").get<std::string>(); if(!sha(hash)) return fail(error,"snapshot control file SHA invalid");
    }
    return bounded(value,error);
}
inline bool parse(const Json& value,Metadata& output,std::string& error) {
    if(!value.is_object()||value.size()!=3||!value.contains("revision")||!value.contains("expected_nodes")||!value.contains("keys_sha256")) return fail(error,"network identity metadata requires exactly revision/expected_nodes/keys_sha256");
    uint64_t revision=0,count=0;
    if(!unsigned_integer(value.at("revision"),revision)||revision!=1) return fail(error,"network identity revision requires integer1");
    if(!unsigned_integer(value.at("expected_nodes"),count)) return fail(error,"network identity expected_nodes requires nonnegative integer");
    if(count>maximum_nodes) return fail(error,"network identity expected_nodes actual="+std::to_string(count)+" required<=65536");
    if(!value.at("keys_sha256").is_string()) return fail(error,"network identity keys SHA requires string");
    auto digest=value.at("keys_sha256").get<std::string>(); if(!sha(digest)) return fail(error,"network identity keys SHA requires64 lowercase hex");
    output={uint32_t(count),std::move(digest)}; return true;
}
inline Json json(const Metadata& value) { return {{"revision",1},{"expected_nodes",value.expected_nodes},{"keys_sha256",value.keys_sha256}}; }
inline bool receipt(const Receipt& value,Role role,const std::string& id,const std::string& hash,std::string& error) {
    if(!value.ready||value.role!=role) return fail(error,"network identity native map not ready for required role");
    if(value.uuid!=id) return fail(error,"network identity native map UUID differs");
    if(value.snapshot_sha256!=hash) return fail(error,"network identity native map file SHA differs");
    if(value.node_count>maximum_nodes) return fail(error,"network identity native node count exceeds65536");
    if(!sha(value.keys_sha256)) return fail(error,"network identity native keydigest invalid");
    return true;
}
}

// Caller logs LegacyFileOnly as a deliberate skip; it never authorizes Network writes.
inline bool attach_begin(Json& begin,const Receipt& host,Disposition& disposition,std::string& error) {
    disposition=Disposition::Rejected;
    try {
        std::string id,hash;
        if(!detail::base(begin,"snapshot_begin",id,hash,error)) return false;
        uint64_t transport=0;
        if(!begin.contains("transport_revision")||!detail::unsigned_integer(begin.at("transport_revision"),transport)||transport!=2) return detail::fail(error,"snapshot begin transport revision requires2");
        if(begin.contains("network_identity")) return detail::fail(error,"snapshot begin already contains network identity; retain original frame for retransmission");
        if(!host.ready) { disposition=Disposition::LegacyFileOnly; error.clear(); return true; }
        if(!detail::receipt(host,Role::Host,id,hash,error)) return false;
        Json candidate=begin; candidate["network_identity"]=detail::json({uint32_t(host.node_count),host.keys_sha256});
        if(!detail::bounded(candidate,error)) return false;
        begin=std::move(candidate); disposition=Disposition::IdentityAttached; error.clear(); return true;
    } catch(const std::exception& cause) { return detail::fail(error,std::string("network identity attach begin exception: ")+cause.what()); }
}
inline bool parse_begin(const Json& begin,uint64_t sender,std::optional<Metadata>& output,std::string& error) {
    output.reset();
    try {
        if(sender!=1) return detail::fail(error,"snapshot network manifest sender must be host1");
        std::string id,hash;
        if(!detail::base(begin,"snapshot_begin",id,hash,error)) return false;
        uint64_t transport=0;
        if(!begin.contains("transport_revision")||!detail::unsigned_integer(begin.at("transport_revision"),transport)||transport!=2) return detail::fail(error,"snapshot begin transport revision requires2");
        if(!begin.contains("network_identity")) { output.reset(); error.clear(); return true; }
        Metadata value; if(!detail::parse(begin.at("network_identity"),value,error)) return false;
        output=std::move(value); error.clear(); return true;
    } catch(const std::exception& cause) { return detail::fail(error,std::string("network identity parse begin exception: ")+cause.what()); }
}
inline bool make_loaded_ack(const std::string& id,const std::string& hash,const std::optional<Metadata>& expected,const Receipt& client,bool native_load_complete,Json& output,Disposition& disposition,std::string& error) {
    disposition=Disposition::Rejected;
    try {
        if(!detail::uuid(id)||!detail::sha(hash)) return detail::fail(error,"network identity loaded ack UUID/file SHA invalid");
        if(!native_load_complete) return detail::fail(error,"snapshot loaded ack before completed native load rejected");
        if(!expected) { output={{"type","snapshot_loaded"},{"id",id}}; disposition=Disposition::LegacyFileOnly; error.clear(); return true; }
        if(!detail::receipt(client,Role::Client,id,hash,error)) return false;
        if(client.node_count!=expected->expected_nodes) return detail::fail(error,"network identity loaded node count differs from manifest");
        if(client.keys_sha256!=expected->keys_sha256) return detail::fail(error,"network identity loaded keydigest differs from manifest");
        output={{"type","snapshot_loaded"},{"id",id},{"sha256",hash},{"network_identity",detail::json(*expected)}};
        if(!detail::bounded(output,error)) return false;
        disposition=Disposition::IdentityAttached; error.clear(); return true;
    } catch(const std::exception& cause) { return detail::fail(error,std::string("network identity loaded ack exception: ")+cause.what()); }
}
// Host calls only after its existing authenticated peer/chunk/SHA/native-loaded sequence guards.
inline bool verify_loaded_ack(const Json& ack,uint64_t sender,const Receipt& host,const std::optional<Metadata>& expected,bool& network_writes_allowed,std::string& error) {
    network_writes_allowed=false;
    try {
        if(sender<2||sender>4) return detail::fail(error,"snapshot loaded ack sender requires authenticated client2..4");
        std::string id,hash;
        if(!detail::base(ack,"snapshot_loaded",id,hash,error,expected.has_value())) return false;
        if(!expected) {
            if(ack.contains("network_identity")) return detail::fail(error,"unexpected network identity ack without offered manifest");
            error.clear(); return true;
        }
        if(!detail::receipt(host,Role::Host,id,hash,error)) return false;
        if(host.node_count!=expected->expected_nodes||host.keys_sha256!=expected->keys_sha256) return detail::fail(error,"host native identity differs from offered manifest");
        if(!ack.contains("network_identity")) return detail::fail(error,"required native network map acknowledgement absent");
        Metadata actual; if(!detail::parse(ack.at("network_identity"),actual,error)) return false;
        if(actual.expected_nodes!=expected->expected_nodes) return detail::fail(error,"native network ack node count differs");
        if(actual.keys_sha256!=expected->keys_sha256) return detail::fail(error,"native network ack keydigest differs");
        network_writes_allowed=true; error.clear(); return true;
    } catch(const std::exception& cause) { return detail::fail(error,std::string("network identity verify ack exception: ")+cause.what()); }
}
}
