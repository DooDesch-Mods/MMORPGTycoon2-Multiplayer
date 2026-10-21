#include "character_import_payload.h"
#include "character_import_golden.h"
#include <iostream>
#include <map>
#include <cstring>
namespace character_import_fixture {
namespace payload=mt2mp::game::character_import_payload;
struct Memory {
    uintptr_t failed=0;std::map<uintptr_t,std::vector<uint8_t>> values;
    template<class T>void set(uintptr_t at,T value){const auto* p=reinterpret_cast<const uint8_t*>(&value);values[at]={p,p+sizeof value};}
    template<class T>bool read(uintptr_t base,size_t offset,T& out,std::string& error){
        if(base+offset==failed){error="exact native Store read failed";return false;}const auto found=values.find(base+offset);
        if(found==values.end()||found->second.size()!=sizeof out){error="fixture Store field missing";return false;}std::memcpy(&out,found->second.data(),sizeof out);return true;}
};
struct Zlib {
    int scenario=0;unsigned begins=0,inflates=0,ends=0;
    bool begin(payload::Stream&,std::string& error){++begins;if(scenario==1){error="exact zlib init failed";return false;}return true;}
    bool inflate(payload::Stream& stream,std::string& error){++inflates;
        if(scenario==2){error="exact original inflate failed";return false;}
        const auto size=uint32_t(v2.size());std::memcpy(stream.next_out,v2.data(),size);stream.total_out=size;stream.avail_out-=size;
        stream.total_in=stream.avail_in;stream.avail_in=0;
        if(scenario==4){--stream.total_in;stream.avail_in=1;}if(scenario==5){stream.total_out=0;}
        return true;}
    bool end(payload::Stream&,std::string& error){++ends;if(scenario==3){error="exact zlib cleanup failed";return false;}return true;}
};
}
int main()try{using namespace character_import_fixture;unsigned checks=0;std::string error;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(std::string(why)+" cause="+error);};
    std::vector<mt2mp::subscriber_lifecycle::Record> rows;
    for(const auto* data:{&v1,&v2}){
        check(payload::record(*data,rows,error)&&rows.size()==3&&rows[1].parent==0&&rows[1].child_index==0&&rows[2].child_index==1,"Complete original V1/V2 ownership/order decode failed");
        check(rows[1].label.text=="name"&&rows[1].tokens.size()==1&&rows[1].tokens[0].type==2&&rows[1].tokens[0].text=="Fixture","Original string/word token distinction lost");
        check(rows[2].tokens.size()==1&&rows[2].tokens[0].type==3&&rows[2].tokens[0].bits==0x3fa00000,"Original exact float bits lost");
        auto negative_zero=*data;const auto at=negative_zero.size()-8;negative_zero[at]=0;negative_zero[at+1]=0;negative_zero[at+2]=0;negative_zero[at+3]=0x80;
        check(payload::record(negative_zero,rows,error)&&rows[2].tokens[0].bits==0x80000000,"Original negative-zero float bits normalized");
        auto bad=*data;bad.push_back(0);check(!payload::record(bad,rows,error)&&error.find("complete expanded payload")!=std::string::npos,"Trailing Record data admitted");
        for(size_t length=0;length<data->size();++length){bad.assign(data->begin(),data->begin()+length);check(!payload::record(bad,rows,error),"Truncated original Record admitted");}
        bad=*data;const auto f=bad.size()-8;bad[f]=0;bad[f+1]=0;bad[f+2]=0xc0;bad[f+3]=0x7f;
        check(!payload::record(bad,rows,error),"Nonfinite native value admitted before RTTI load");
    }
    auto bad=v2;bad[10]=5;check(!payload::record(bad,rows,error),"Pointer/foreign root token kind admitted");
    bad=v2;bad[13]='X';check(!payload::record(bad,rows,error)&&error.find("mmoCharacterDefinition")!=std::string::npos,"Foreign RTTI root admitted");
    bad=v2;bad[7]='X';check(!payload::record(bad,rows,error)&&error.find("RecordV1/RecordV2")!=std::string::npos,"Foreign binary format admitted");
    bad=v1;bad[62]=0xff;check(!payload::record(bad,rows,error)&&error.find("string-table index")!=std::string::npos,"Invalid legacy string-table owner admitted");
    std::vector<uint8_t> decoded;check(payload::bytes(payload::hex(v2),decoded,error)&&decoded==v2,"Exact compressed byte transport changed payload");
    for(const std::string malformed:{"", "0", "0X", "gg"})check(!payload::bytes(malformed,decoded,error),"Malformed byte transport admitted");
    check(!payload::bytes(std::string(payload::maximum_compressed_bytes*2+2,'0'),decoded,error),"Existing packet resource bound bypassed");
    Memory memory;constexpr uintptr_t object=0x100,buffer=0x1000,ap=0x3000;
    memory.set(object,ap);memory.set(object+8,buffer);memory.set(object+0x10,uint64_t(10));memory.set(object+0x18,buffer+10);memory.set(object+0x20,buffer);memory.set(object+0x28,buffer+8);
    auto read=[&](uintptr_t base,size_t offset,auto& out,std::string& e){return memory.read(base,offset,out,e);};payload::Store store;
    check(payload::store(read,object,ap,store,error)&&store.write-store.buffer==8,"Actual original Store extent failed");
    memory.failed=object+0x20;check(!payload::store(read,object,ap,store,error)&&error=="exact native Store read failed","Store read failure cause replaced");memory.failed=0;
    memory.set(object+0x20,buffer+1);check(!payload::store(read,object,ap,store,error),"Advanced original Store head hid a prefix");memory.set(object+0x20,buffer);
    memory.set(object+0x28,buffer+11);check(!payload::store(read,object,ap,store,error),"Write head outside allocation admitted");
    for(int scenario=0;scenario<6;++scenario){Zlib calls;calls.scenario=scenario;error.clear();std::vector<uint8_t> expanded;
        const bool ok=payload::expand(calls,std::vector<uint8_t>{1,2,3},expanded,error);
        if(!scenario)check(ok&&expanded==v2&&calls.begins==1&&calls.inflates==1&&calls.ends==1,"Bounded zlib stream was not complete/original-once");
        if(scenario==1)check(!ok&&error=="exact zlib init failed"&&calls.inflates==0&&calls.ends==0,"Failed zlib init entered body or lost cause");
        if(scenario==2)check(!ok&&error=="exact original inflate failed"&&calls.ends==1&&expanded.empty(),"Original inflate failure lost cleanup/cause");
        if(scenario==3)check(!ok&&error.find("exact zlib cleanup failed")!=std::string::npos&&calls.ends==1&&expanded.empty(),"Failed zlib cleanup produced success");
        if(scenario==4||scenario==5)check(!ok&&error.find("complete bounded stream")!=std::string::npos&&expanded.empty(),"Partial/empty zlib result admitted");
    }
    std::cout<<checks<<" actual Character import payload/Record/Store/zlib contract checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
