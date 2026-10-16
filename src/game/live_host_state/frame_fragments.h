#pragma once
#include "state.h"
#include <algorithm>
#include <chrono>
#include <functional>
#include <set>
namespace mt2mp::game::live_host_state::fragments {
using Json=nlohmann::json;
using Digest=std::function<std::string(const std::vector<uint8_t>&)>;
using Packets=std::vector<std::vector<uint8_t>>;
inline bool fail(std::string& e,const std::string& cause){e=cause;return false;}
inline Json packet_json(const uint8_t* bytes,size_t size){
    if(!bytes||!size||size>max_frame_payload)throw std::runtime_error("live Channel9 packet bound invalid");
    std::vector<std::set<std::string>> fields;
    return Json::parse(bytes,bytes+size,[&](int depth,Json::parse_event_t event,Json& value){
        if(depth>7)throw std::runtime_error("live Channel9 discriminator nesting exceeds7");
        if(event==Json::parse_event_t::object_start)fields.emplace_back();
        if(event==Json::parse_event_t::key&&!fields.back().insert(value.get<std::string>()).second)throw std::runtime_error("duplicate live Channel9 envelope field");
        if(event==Json::parse_event_t::object_end)fields.pop_back();return true;
    });
}
inline bool digest_text(const std::string& text){return text.size()==64&&std::all_of(text.begin(),text.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
inline std::string hex(const uint8_t* data,size_t size){
    constexpr char digits[]="0123456789abcdef";std::string out;out.reserve(size*2);
    for(size_t i=0;i<size;++i){out+=digits[data[i]>>4];out+=digits[data[i]&15];}return out;
}
inline bool unhex(const std::string& text,std::vector<uint8_t>& out,std::string& e){
    out.clear();if(text.size()%2)return fail(e,"Host frame fragment odd hex length");out.reserve(text.size()/2);
    const auto digit=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
    for(size_t i=0;i<text.size();i+=2){const auto a=digit(text[i]),b=digit(text[i+1]);if(a<0||b<0)return fail(e,"Host frame fragment invalid hex");out.push_back(uint8_t((a<<4)|b));}return true;
}
inline Json identity(const Frame& f){return {{"session",f.session},{"epoch",f.epoch},{"source_generation",f.source_generation},{"revision",f.revision},{"snapshot_uuid",f.snapshot}};}
inline bool encode(const Frame& f,const std::vector<uint8_t>& bytes,const Digest& digest,Packets& out,std::string& e){
    out.clear();if(bytes.empty()||bytes.size()>max_logical_frame_payload)return fail(e,"Host logical frame exceeds 1048576-byte reassembly bound bytes="+std::to_string(bytes.size()));
    if(bytes.size()<=action_session::max_transport_payload){out.push_back(bytes);e.clear();return true;}
    try{
        const auto hash=digest(bytes);if(!digest_text(hash))return fail(e,"Host frame SHA256 result invalid");
        const auto count=(bytes.size()+frame_chunk_bytes-1)/frame_chunk_bytes;Packets packets;packets.reserve(count);
        for(size_t index=0;index<count;++index){const auto offset=index*frame_chunk_bytes;const auto size=std::min(frame_chunk_bytes,bytes.size()-offset);
            auto packet=identity(f);packet["kind"]="host_state_fragment";packet["schema"]=1;packet["bytes"]=bytes.size();packet["chunks"]=count;packet["index"]=index;packet["sha256"]=hash;packet["data"]=hex(bytes.data()+offset,size);
            const auto text=packet.dump();if(text.size()>2*size+frame_envelope_reserve||text.size()>action_session::max_transport_payload)return fail(e,"Host frame fragment envelope exceeds queue-budget reserve");
            packets.emplace_back(text.begin(),text.end());
        }
        out=std::move(packets);e.clear();return true;
    }catch(const std::exception& x){return fail(e,std::string("Host frame fragment encode original=")+x.what());}
}
class Assembly {
    Json metadata_;std::vector<uint8_t> bytes_;
    Json completed_;
    size_t next_=0,total_=0,count_=0;
    bool active_=false,failed_=false;
    std::string failure_;
    std::chrono::steady_clock::time_point started_{};
    bool reject(std::string& e,const std::string& cause){failed_=true;failure_=cause;return fail(e,cause);}
public:
    bool pending()const{return active_;}
    const Json& completed()const{return completed_;}
    void clear(){*this=Assembly{};}
    bool timely(std::chrono::steady_clock::time_point now,std::string& e){
        if(failed_)return fail(e,failure_);
        if(active_&&now-started_>=std::chrono::seconds(30))return reject(e,"Host frame incomplete after 30 seconds chunks="+std::to_string(next_)+" required="+std::to_string(count_)+"; no partial Apply");
        return true;
    }
    bool take(uint64_t sender,const Context& context,uint64_t epoch,uint64_t generation,const Json& packet,const Digest& digest,
              std::vector<uint8_t>& complete,std::string& e,std::chrono::steady_clock::time_point now=std::chrono::steady_clock::now()){
        complete.clear();if(!timely(now,e))return false;
        try{
            if(context.role!=action_session::Role::Client||sender!=context.host_peer||sender==context.local_peer||
               sender<1||sender>4||!(context.peer_mask&(1u<<(sender-1))))return reject(e,"Host frame fragment sender/Client authority invalid");
            if(!packet.is_object()||packet.size()!=12||packet.at("kind")!="host_state_fragment"||packet.at("schema")!=1)return reject(e,"Host frame fragment exact schema1 required");
            for(const char* field:{"session","epoch","source_generation","revision","bytes","chunks","index"})if(!packet.at(field).is_number_unsigned())return reject(e,std::string("Host frame fragment unsigned field required=")+field);
            if(packet.at("session").get<uint64_t>()!=context.session||packet.at("epoch").get<uint64_t>()!=epoch||
               packet.at("snapshot_uuid")!=context.snapshot_uuid||!packet.at("revision").get<uint64_t>()||!packet.at("source_generation").get<uint64_t>()||
               (generation&&packet.at("source_generation").get<uint64_t>()!=generation))return reject(e,"Host frame fragment session/snapshot/epoch/generation/revision differs");
            const auto total=packet.at("bytes").get<uint64_t>(),count=packet.at("chunks").get<uint64_t>(),index=packet.at("index").get<uint64_t>();
            if(total<=action_session::max_transport_payload||total>max_logical_frame_payload||count!=(total+frame_chunk_bytes-1)/frame_chunk_bytes||index>=count)return reject(e,"Host frame fragment declared length/count/index invalid");
            const auto hash=packet.at("sha256").get<std::string>();if(!digest_text(hash))return reject(e,"Host frame fragment SHA256 text invalid");
            const auto data=packet.at("data").get<std::string>();const auto expected=std::min<uint64_t>(frame_chunk_bytes,total-index*frame_chunk_bytes);
            if(data.size()!=expected*2)return reject(e,"Host frame fragment data length differs");
            std::vector<uint8_t> chunk;if(!unhex(data,chunk,e))return reject(e,e);
            Json metadata=packet;metadata.erase("index");metadata.erase("data");
            if(metadata_!=metadata){
                if(active_)return reject(e,"Host frame fragment changed identity before completion");
                if(index)return reject(e,"Host frame fragment first index must be zero");
                if(!metadata_.is_null()&&packet.at("revision").get<uint64_t>()<=metadata_.at("revision").get<uint64_t>())return reject(e,"Host frame fragment stale/conflicting revision");
                metadata_=std::move(metadata);bytes_.clear();total_=size_t(total);count_=size_t(count);next_=0;active_=true;started_=now;
            }
            if(index<next_){const auto offset=size_t(index)*frame_chunk_bytes;if(offset+chunk.size()>bytes_.size()||!std::equal(chunk.begin(),chunk.end(),bytes_.begin()+offset))return reject(e,"Host frame duplicate fragment conflicts");e.clear();return true;}
            if(index!=next_||!active_)return reject(e,"Host frame fragment order differs index="+std::to_string(index)+" required="+std::to_string(next_));
            bytes_.insert(bytes_.end(),chunk.begin(),chunk.end());++next_;
            if(next_==count_){
                if(bytes_.size()!=total_||digest(bytes_)!=hash)return reject(e,"Host frame reassembly length/SHA256 differs; no Apply");
                const auto value=Json::parse(bytes_,[](int depth,Json::parse_event_t,Json&){if(depth>10)throw std::runtime_error("Host frame reassembly nesting exceeds10");return true;});const Json* frame=&value;
                if(value.at("kind")=="host_state_economy")frame=&value.at("day").at("frame");
                else if(value.at("kind")=="host_state_day")frame=&value.at("frame");
                else if(value.at("kind")!="host_state")return reject(e,"Host frame fragment payload kind invalid");
                for(const char* field:{"session","epoch","source_generation","revision","snapshot_uuid"})if(frame->at(field)!=metadata_.at(field))return reject(e,std::string("Host frame fragment payload identity differs=")+field);
                completed_={{"bytes",total_},{"chunks",count_},{"revision",packet.at("revision")},{"sha256",hash},{"sha256_verified",true},{"complete",true}};
                active_=false;complete=bytes_;
            }
            e.clear();return true;
        }catch(const std::exception& x){return reject(e,std::string("Host frame fragment receive original=")+x.what());}
    }
};
}
