#pragma once
#include "subscriber_lifecycle/record_graph.h"
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <limits>
#include <string>
#include <vector>
namespace mt2mp::game::character_import_payload {
inline constexpr size_t maximum_compressed_bytes=30000;
inline constexpr size_t maximum_expanded_bytes=32*1024*1024;
inline std::string hex(const std::vector<uint8_t>& bytes){
    constexpr char digits[]="0123456789abcdef";std::string out;out.reserve(bytes.size()*2);
    for(auto value:bytes){out.push_back(digits[value>>4]);out.push_back(digits[value&15]);}return out;
}
inline bool bytes(const std::string& encoded,std::vector<uint8_t>& out,std::string& error){
    out.clear();if(encoded.empty()||(encoded.size()&1)||encoded.size()>maximum_compressed_bytes*2){error="Character import compressed value exceeds existing action packet extent";return false;}
    const auto digit=[](char c){return c>='0'&&c<='9'?int(c-'0'):c>='a'&&c<='f'?int(c-'a'+10):-1;};
    out.reserve(encoded.size()/2);
    for(size_t i=0;i<encoded.size();i+=2){const auto high=digit(encoded[i]),low=digit(encoded[i+1]);
        if(high<0||low<0){error="Character import compressed value requires exact lowercase hexadecimal bytes";return false;}out.push_back(uint8_t(high*16+low));}return true;
}
inline bool definition(const std::vector<subscriber_lifecycle::Record>& records,std::string& error){
    if(records.empty()||records.size()>100000||records[0].parent!=SIZE_MAX||records[0].child_index||
       records[0].label.type!=1||records[0].label.text!="mmoCharacterDefinition"){
        error="Character import original root is not mmoCharacterDefinition";return false;}
    size_t token_count=0,text_bytes=0;std::vector<size_t> children(records.size());
    const auto token=[&](const subscriber_lifecycle::Token& value){
        if(value.type<0||value.type>4||value.text.find('\0')!=std::string::npos)return false;
        if(value.text.size()>maximum_expanded_bytes-text_bytes)return false;text_bytes+=value.text.size();
        if((value.type==1||value.type==2)?value.bits!=0:!value.text.empty())return false;
        return value.type!=3||std::isfinite(std::bit_cast<float>(value.bits));};
    for(size_t i=0;i<records.size();++i){const auto& row=records[i];
        if(i&&(row.parent>=i||row.child_index!=children[row.parent]++)){error="Character import original Record ownership/order differs";return false;}
        if(row.label.type>2||!token(row.label)||row.tokens.size()>1000000-token_count){error="Character import original Record label/token budget differs";return false;}
        token_count+=row.tokens.size();for(const auto& value:row.tokens)if(!token(value)){error="Character import original Record scalar value is not finite owned data";return false;}}
    return true;
}
// These are the existing RecordV1/V2 saved-value formats, bounded before any original
// RTTI constructor or loader receives a user-supplied Character prefab.
inline bool record(const std::vector<uint8_t>& raw,std::vector<subscriber_lifecycle::Record>& out,std::string& error)try{
    out.clear();if(raw.empty()||raw.size()>maximum_expanded_bytes){error="Character import expanded Record extent invalid";return false;}
    struct Reader {
        const std::vector<uint8_t>& raw;std::vector<subscriber_lifecycle::Record>& rows;size_t offset=0,tokens=0,text=0;bool legacy=false;std::vector<std::string> strings;
        uint32_t value(unsigned bytes,bool little=false){
            if(bytes>raw.size()-offset)throw std::runtime_error("Character import truncated Record value at offset="+std::to_string(offset));
            uint32_t out=0;for(unsigned i=0;i<bytes;++i){const auto byte=uint32_t(raw[offset++]);if(little)out|=byte<<(i*8);else out=(out<<8)|byte;}return out;
        }
        std::string string(){const auto length=size_t(value(2));
            if(length>raw.size()-offset||length>maximum_expanded_bytes-text)throw std::runtime_error("Character import truncated/over-budget Record string at offset="+std::to_string(offset));
            std::string out(reinterpret_cast<const char*>(raw.data()+offset),length);offset+=length;text+=length;return out;
        }
        subscriber_lifecycle::Token token(){subscriber_lifecycle::Token out;out.type=int32_t(value(1));
            if(out.type==1||out.type==2){if(legacy){const auto index=size_t(value(4));if(index>=strings.size())throw std::runtime_error("Character import RecordV1 string-table index is outside its complete owner");out.text=strings[index];}else out.text=string();}
            else if(out.type==3)out.bits=value(4,true);else if(out.type==4)out.bits=value(4);
            else if(out.type!=0)throw std::runtime_error("Character import invalid original token kind="+std::to_string(out.type));return out;
        }
        void node(size_t parent,size_t child,unsigned depth){
            if(depth>256||rows.size()>=100000)throw std::runtime_error("Character import Record ownership budget exceeded");
            subscriber_lifecycle::Record row;row.parent=parent;row.child_index=child;row.label=token();
            const auto count=size_t(value(4));if(count>1000000-tokens||count>raw.size()-offset)throw std::runtime_error("Character import Record token count exceeds remaining extent");
            tokens+=count;row.tokens.reserve(count);for(size_t i=0;i<count;++i)row.tokens.push_back(token());
            const auto children=size_t(value(4));if(children>100000-rows.size()||children>(raw.size()-offset)/9)throw std::runtime_error("Character import Record child count exceeds remaining extent");
            const auto index=rows.size();rows.push_back(std::move(row));for(size_t i=0;i<children;++i)node(index,i,depth+1);
        }
    } read{raw,out};
    const auto version=read.string();read.legacy=version=="RecordV1";
    if(!read.legacy&&version!="RecordV2"){error="Character import payload is not original RecordV1/RecordV2";return false;}
    if(read.legacy){const auto count=size_t(read.value(4));if(count>100000||count>(raw.size()-read.offset)/2){error="Character import RecordV1 string-table count exceeds its complete owner";return false;}
        read.strings.reserve(count);for(size_t i=0;i<count;++i)read.strings.push_back(read.string());}
    read.node(SIZE_MAX,0,0);
    if(read.offset!=raw.size()){error="Character import Record did not consume complete expanded payload: "+std::to_string(read.offset)+"/"+std::to_string(raw.size());return false;}
    return definition(out,error);
}catch(const std::exception& exception){error=std::string("Character import original Record decode: ")+exception.what();return false;}
struct Store {uintptr_t pointer=0,buffer=0,end=0,read=0,write=0;uint64_t capacity=0;};
template<class Read>bool store(Read& read,uintptr_t object,uintptr_t expected_ap,Store& out,std::string& error){
    out={};out.pointer=object;uintptr_t ap=0;
    if(!read(object,0,ap,error)||!read(object,8,out.buffer,error)||!read(object,0x10,out.capacity,error)||
       !read(object,0x18,out.end,error)||!read(object,0x20,out.read,error)||!read(object,0x28,out.write,error))return false;
    if(ap!=expected_ap||!out.buffer||out.capacity>UINTPTR_MAX-out.buffer||out.end!=out.buffer+out.capacity||
       out.read!=out.buffer||out.write<=out.buffer||out.write>out.end||out.write-out.buffer>maximum_compressed_bytes){
        error="Character import original decoded Store extent/AP/read/write heads differ";return false;}return true;
}
// The public Windows zlib ABI used by the original game's 0x58-byte stream.
struct Stream {
    const uint8_t* next_in=nullptr;uint32_t avail_in=0,total_in=0;
    uint8_t* next_out=nullptr;uint32_t avail_out=0,total_out=0;
    const char* message=nullptr;void* state=nullptr;
    void* allocate=nullptr;void* release=nullptr;void* opaque=nullptr;
    int32_t data_type=0;uint32_t adler=0,reserved=0;
};
static_assert(sizeof(Stream)==0x58&&offsetof(Stream,next_out)==0x10&&offsetof(Stream,state)==0x28);
template<class Calls>bool expand(Calls& calls,const std::vector<uint8_t>& source,std::vector<uint8_t>& output,std::string& error){
    output.clear();if(source.empty()||source.size()>maximum_compressed_bytes){error="Character import compressed zlib extent invalid";return false;}
    Stream stream;stream.next_in=source.data();stream.avail_in=uint32_t(source.size());
    output.resize(maximum_expanded_bytes);stream.next_out=output.data();stream.avail_out=uint32_t(output.size());
    if(!calls.begin(stream,error)){output.clear();return false;}
    const bool inflated=calls.inflate(stream,error);std::string cleanup;
    const bool ended=calls.end(stream,cleanup);
    if(!ended){if(!error.empty())error+="; ";error+="Character import zlib cleanup: "+cleanup;}
    if(!inflated||!ended){output.clear();return false;}
    if(stream.avail_in||stream.total_in!=source.size()||!stream.total_out||stream.total_out>output.size()||
       stream.avail_out!=output.size()-stream.total_out){error="Character import zlib did not consume one complete bounded stream";output.clear();return false;}
    output.resize(stream.total_out);return true;
}
}
