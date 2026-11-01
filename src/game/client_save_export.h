#pragma once
#include "selected_snapshot.h"
#include <functional>

namespace mt2mp::game::client_save_export {
namespace fs=std::filesystem;
using Json=nlohmann::json;
using Current=std::function<bool(std::string&)>;
inline void require(bool value,const std::string& cause){if(!value)throw std::runtime_error("Client Save export: "+cause);}
struct Identity {
    uint64_t session=0,world=0,requester=0;
    uint8_t mask=0;
    std::string nonce;
    bool operator==(const Identity&)const=default;
};
inline void valid(const Identity& value){
    require(value.session&&value.world&&value.requester>=2&&value.requester<=4&&value.mask<16&&(value.mask&1)&&
        (value.mask&(1u<<(value.requester-1)))&&net::snapshot_world_path::valid_uuid(value.nonce),"request identity invalid");
}
struct File {std::string sha;uint64_t bytes=0;bool operator==(const File&)const=default;};
struct Offer {Identity identity;File native,companion,metadata;bool operator==(const Offer&)const=default;};
inline Json wire(const Offer& value){
    return {{"revision",1},{"session",value.identity.session},{"world",value.identity.world},{"requester",value.identity.requester},
        {"mask",value.identity.mask},{"nonce",value.identity.nonce},{"native_sha256",value.native.sha},{"native_bytes",value.native.bytes},
        {"companion_sha256",value.companion.sha},{"companion_bytes",value.companion.bytes},
        {"metadata_sha256",value.metadata.sha},{"metadata_bytes",value.metadata.bytes}};
}
inline Offer parse(const Json& value,const Identity& expected){
    require(value.is_object()&&value.size()==12&&value.at("revision")==1,"exact manifest schema differs");
    for(const char* field:{"session","world","requester","mask","native_bytes","companion_bytes","metadata_bytes"})
        require(value.at(field).is_number_unsigned(),std::string("manifest unsigned field differs: ")+field);
    const auto mask=value.at("mask").get<uint64_t>();require(mask<16,"manifest mask exceeds slots");
    Offer result{{value.at("session").get<uint64_t>(),value.at("world").get<uint64_t>(),value.at("requester").get<uint64_t>(),
        uint8_t(mask),value.at("nonce").get<std::string>()},
        {value.at("native_sha256").get<std::string>(),value.at("native_bytes").get<uint64_t>()},
        {value.at("companion_sha256").get<std::string>(),value.at("companion_bytes").get<uint64_t>()},
        {value.at("metadata_sha256").get<std::string>(),value.at("metadata_bytes").get<uint64_t>()}};
    valid(result.identity);require(result.identity==expected,"manifest differs from immutable local request");
    require(net::valid_snapshot_digest(result.native.sha)&&net::valid_snapshot_digest(result.companion.sha)&&result.native.bytes&&
        result.native.bytes<=net::snapshot_max_bytes&&result.companion.bytes&&result.companion.bytes<=monster_snapshot_companion::maximum_bytes,
        "manifest file identity/bounds differ");
    require(result.metadata.bytes&&result.metadata.bytes<=net::snapshot_max_bytes&&
        net::valid_snapshot_digest(result.metadata.sha),"native-produced metadata identity/bounds differ");return result;
}
inline File hash(const fs::path& path){
    selected_snapshot::regular(path,true);File result;std::string error;
    require(net::hash_snapshot_file(path,result.sha,result.bytes,error),error);return result;
}
inline fs::path sidecar(const fs::path& path){return fs::path(path.wstring()+L".monsters.json");}
inline fs::path metadata(const fs::path& path){auto result=path;result.replace_extension(L".meta.vrz");return result;}
inline void companion(const fs::path& path,const Offer& offer){
    require(hash(path)==offer.companion,"complete companion file SHA/bytes differs");
    std::ifstream input(path,std::ios::binary);std::string bytes(size_t(offer.companion.bytes),'\0');input.read(bytes.data(),std::streamsize(bytes.size()));
    require(bool(input),"complete companion read failed");monster_snapshot_companion::Manifest manifest;std::string error;
    require(monster_snapshot_companion::decode(bytes,offer.native.sha,offer.native.bytes,manifest,error),error);
    require(manifest.region_rng.present,"current export lacks preserved revision8 Region rows");
}
inline void current(const Current& callback){std::string error;require(callback&&callback(error),error.empty()?"current bound owner unavailable":error);}
struct Chunk {uint32_t index=0;std::vector<uint8_t> bytes;};

// A source holds both exact files until the requester acknowledges the verified
// pair. File paths never enter the offer or chunk protocol.
class Source {
    Offer offer_;Current current_;net::SnapshotFileSender native_,companion_,metadata_;
    uint32_t sent_=0,acknowledged_=0;bool active_=false,complete_=false;
public:
    Offer begin(const Identity& identity,const fs::path& native,Current owner){
        require(!active_&&!complete_,"source transaction already used");valid(identity);current_=std::move(owner);current(current_);
        require(native.is_absolute(),"source requires an owned absolute native path");
        private_profile::DirectoryPins pins;pins.verify(native.parent_path());
        offer_={identity,hash(native),hash(sidecar(native)),hash(metadata(native))};companion(sidecar(native),offer_);
        parse(wire(offer_),identity);
        std::string error;require(native_.begin(native,offer_.native.sha,error),error);
        try{require(companion_.begin(sidecar(native),offer_.companion.sha,error),error);require(metadata_.begin(metadata(native),offer_.metadata.sha,error),error);}
        catch(const std::exception& failure){std::string retirement;if(!retire(retirement))throw std::runtime_error(
            std::string(failure.what())+"; Client Save export source pin retirement failed: "+retirement);throw;}
        active_=true;return offer_;
    }
    uint32_t chunks()const{return native_.chunks()+companion_.chunks()+metadata_.chunks();}
    bool next(Chunk& chunk){
        current(current_);require(active_,"source is not active");
        if(sent_==chunks()||sent_-acknowledged_>=net::snapshot_window_chunks)return false;
        chunk.index=sent_;std::string error;
        auto& sender=sent_<native_.chunks()?native_:sent_<native_.chunks()+companion_.chunks()?companion_:metadata_;
        require(sender.next(chunk.bytes,error),error);++sent_;return true;
    }
    void acknowledge(uint64_t sender,const Identity& identity,uint32_t next){
        current(current_);require(active_&&sender==offer_.identity.requester&&identity==offer_.identity&&next>acknowledged_&&next<=sent_,
            "chunk acknowledgement owner/order differs");acknowledged_=next;
    }
    void verified(uint64_t sender,const Json& receipt){
        current(current_);require(active_&&sender==offer_.identity.requester&&acknowledged_==chunks()&&parse(receipt,offer_.identity)==offer_,
            "verified pair acknowledgement differs");std::string error;
        require(native_.finish(error),error);require(companion_.finish(error),error);require(metadata_.finish(error),error);active_=false;complete_=true;
    }
    bool complete()const{return complete_;}
    bool retire(std::string& error){
        const bool first=native_.retire(error);std::string second;
        const bool second_ok=companion_.retire(second);if(!second_ok){if(!error.empty())error+="; ";error+=second;}
        const bool last=metadata_.retire(second);if(!last){if(!error.empty())error+="; ";error+=second;}
        active_=false;return first&&second_ok&&last;
    }
};

// The receiver accepts only a locally supplied destination. Its verified files
// are separate from the existing user save until the caller publishes the pair.
// There is no world selection, native load or identity-map publication here.
class Receiver {
    Offer offer_;Current current_;net::SnapshotBuffer native_,companion_,metadata_;
    fs::path destination_,verified_native_;uint32_t received_=0;bool active_=false,verified_=false;
public:
    void begin(uint64_t sender,const Identity& expected,const Json& manifest,const fs::path& local_destination,const fs::path& local_staging,Current owner){
        require(!active_&&!verified_,"receiver transaction already used");valid(expected);current_=std::move(owner);current(current_);
        require(sender==1,"export manifest did not come from physical Host");offer_=parse(manifest,expected);
        require(local_destination.is_absolute()&&local_destination.extension()==L".vrz"&&
            net::snapshot_world_path::valid_folder(net::snapshot_world_path::utf8(local_destination.filename())),"local destination is not an original native filename");
        destination_=local_destination;private_profile::DirectoryPins pins;pins.verify(destination_.parent_path());
        require(local_staging.is_absolute()&&!private_profile::equal(local_staging,destination_.parent_path()),
            "export staging must be separate from the native save directory");pins.verify(local_staging);
        verified_native_=local_staging/net::snapshot_world_path::utf8_path("export-"+expected.nonce+".vrz");
        auto native_partial=verified_native_;native_partial+=L".incoming";
        auto companion_target=sidecar(verified_native_),companion_partial=companion_target;companion_partial+=L".incoming";
        std::string error;require(native_.begin(offer_.native.bytes,uint32_t((offer_.native.bytes+net::snapshot_chunk_bytes-1)/net::snapshot_chunk_bytes),
            native_partial,verified_native_,offer_.native.sha,error),error);
        try{require(companion_.begin(offer_.companion.bytes,uint32_t((offer_.companion.bytes+net::snapshot_chunk_bytes-1)/net::snapshot_chunk_bytes),
            companion_partial,companion_target,offer_.companion.sha,error),error);
            auto metadata_target=metadata(verified_native_),metadata_partial=metadata_target;metadata_partial+=L".incoming";
            require(metadata_.begin(offer_.metadata.bytes,uint32_t((offer_.metadata.bytes+net::snapshot_chunk_bytes-1)/net::snapshot_chunk_bytes),
                metadata_partial,metadata_target,offer_.metadata.sha,error),error);}
        catch(const std::exception& failure){std::string retirement;if(!retire(retirement))throw std::runtime_error(
            std::string(failure.what())+"; Client Save export receiver retirement failed: "+retirement);throw;}
        active_=true;
    }
    uint32_t receive(uint64_t sender,const Identity& identity,const Chunk& chunk){
        current(current_);require(active_&&sender==1&&identity==offer_.identity&&chunk.index==received_,"chunk physical owner/identity/order differs");
        const auto native_chunks=uint32_t((offer_.native.bytes+net::snapshot_chunk_bytes-1)/net::snapshot_chunk_bytes);
        const auto companion_chunks=uint32_t((offer_.companion.bytes+net::snapshot_chunk_bytes-1)/net::snapshot_chunk_bytes);
        const bool is_native=chunk.index<native_chunks,is_companion=!is_native&&chunk.index<native_chunks+companion_chunks;
        auto& receiver=is_native?native_:is_companion?companion_:metadata_;
        require(chunk.index<native_chunks||native_.committed(),"companion arrived before complete native bytes");
        require(is_native||is_companion||companion_.committed(),"metadata arrived before complete companion bytes");
        const auto offset=is_native?0:is_companion?native_chunks:native_chunks+companion_chunks;
        std::string error;require(receiver.add(chunk.index-offset,chunk.bytes.data(),chunk.bytes.size(),error),error);
        if(receiver.complete())require(receiver.commit(error),error);++received_;
        if(native_.committed()&&companion_.committed()&&metadata_.committed()){
            require(hash(verified_native_)==offer_.native,"verified native file changed");companion(sidecar(verified_native_),offer_);
            require(hash(metadata(verified_native_))==offer_.metadata,"verified native metadata changed");verified_=true;active_=false;
        }
        return received_;
    }
    Json receipt()const{current(current_);require(verified_,"complete pair not yet verified");return wire(offer_);}
    const fs::path& verified_native()const{current(current_);require(verified_,"native path requested before pair verification");return verified_native_;}
    const fs::path& destination()const{return destination_;}
    bool verified()const{return verified_;}
    bool retire(std::string& error){
        const bool first=native_.retire(error);std::string second;const bool second_ok=companion_.retire(second);
        if(!second_ok){if(!error.empty())error+="; ";error+=second;}
        const bool last=metadata_.retire(second);if(!last){if(!error.empty())error+="; ";error+=second;}active_=false;return first&&second_ok&&last;
    }
};
}
