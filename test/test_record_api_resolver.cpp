#include "subscriber_lifecycle/append_calls.h"
#include "subscriber_lifecycle/native_codes.h"
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
namespace api=mt2mp::subscriber_lifecycle::first_append;
namespace {
size_t checks=0;
void need(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
constexpr uintptr_t image=0x10000000;
constexpr std::array<uintptr_t,13> record_entries={0xa02670,0xa01260,0x9667c0,0x9676a0,0x11c0260,0x11bd1e0,
    0x964920,0x96d370,0x96d360,0x964b50,0x96d380,0x96d3a0,0x96d390};
struct Memory final:mt2mp::subscriber_lifecycle::Memory {
    std::map<uintptr_t,std::array<unsigned char,16>> code;
    std::vector<uintptr_t> reads;
    uintptr_t fail_address=0;
    Memory(){for(const auto& entry:api::native_codes)code.emplace(image+entry.rva,entry.bytes);}
    bool read(uintptr_t address,void* out,size_t size,std::string& error)override{
        reads.push_back(address);
        if(address==fail_address){error="owned code read failed address="+std::to_string(address);return false;}
        const auto found=code.find(address);
        if(found==code.end()||size!=16){error="unexpected code read address/size";return false;}
        std::memcpy(out,found->second.data(),size);return true;
    }
};
std::array<uintptr_t,13> pointers(const api::DecodeApi& a){return {
    reinterpret_cast<uintptr_t>(a.allocate),reinterpret_cast<uintptr_t>(a.release),
    reinterpret_cast<uintptr_t>(a.record_construct),reinterpret_cast<uintptr_t>(a.record_destroy),
    reinterpret_cast<uintptr_t>(a.string_construct),reinterpret_cast<uintptr_t>(a.string_destroy),
    reinterpret_cast<uintptr_t>(a.add_child),reinterpret_cast<uintptr_t>(a.set_label),reinterpret_cast<uintptr_t>(a.set_string),
    reinterpret_cast<uintptr_t>(a.token_count),reinterpret_cast<uintptr_t>(a.set_integer),reinterpret_cast<uintptr_t>(a.set_type),reinterpret_cast<uintptr_t>(a.set_float)};}
bool empty(const api::DecodeApi& a){return pointers(a)==std::array<uintptr_t,13>{};}
}
int main(){try{
    Memory memory;api::DecodeApi records;api::AppendApi append;std::string error;
    need(api::resolve_record_api(memory,image,records,error)&&error.empty(),"valid record API rejected: "+error);
    const auto actual=pointers(records);
    for(size_t i=0;i<record_entries.size();++i)need(actual[i]==image+record_entries[i],"record function pointer assignment differs");
    std::set<uintptr_t> expected_reads;for(const auto rva:record_entries)expected_reads.insert(image+rva);
    need(memory.reads.size()==13&&std::set<uintptr_t>(memory.reads.begin(),memory.reads.end())==expected_reads,"record resolver read unrelated bytes or skipped a function");
    need(api::resolve_append_api(memory,image,append,error)&&pointers(append.records)==actual&&append.context_construct&&append.calls.array_load,"unchanged full Append API failed");
    memory.code[image+0x89e0f0][0]^=0xff;memory.reads.clear();
    need(api::resolve_record_api(memory,image,records,error)&&pointers(records)==actual,"installed ContextLookup hook blocked record-only use");
    need(memory.reads.size()==13&&std::set<uintptr_t>(memory.reads.begin(),memory.reads.end())==expected_reads,"record resolver touched ContextLookup after hook");
    need(!api::resolve_append_api(memory,image,append,error)&&error=="Subscriber append native code mismatch RVA="+std::to_string(0x89e0f0)&&empty(append.records)&&!append.context_construct,"full Append accepted modified ContextLookup or published partial pointers");
    for(const auto rva:record_entries){
        for(const size_t byte:{size_t(0),size_t(15)}){
            Memory changed;changed.code[image+rva][byte]^=0x01;records=api::DecodeApi{};records.allocate=reinterpret_cast<api::Allocate>(1);
            need(!api::resolve_record_api(changed,image,records,error)&&empty(records)&&error=="Subscriber Record API native code mismatch RVA="+std::to_string(rva),"record fingerprint mismatch accepted or partial API published");
        }
        Memory unreadable;unreadable.fail_address=image+rva;records.allocate=reinterpret_cast<api::Allocate>(1);
        need(!api::resolve_record_api(unreadable,image,records,error)&&empty(records)&&error=="owned code read failed address="+std::to_string(image+rva),"original bound-read failure cause lost");
    }
    for(const auto& entry:api::native_codes){
        Memory changed;changed.code[image+entry.rva][0]^=0x01;
        need(!api::resolve_append_api(changed,image,append,error)&&empty(append.records)&&error=="Subscriber append native code mismatch RVA="+std::to_string(entry.rva),"full Append lost an existing fingerprint guard");
    }
    Memory absent;absent.reads.clear();records.allocate=reinterpret_cast<api::Allocate>(1);
    need(!api::resolve_record_api(absent,0,records,error)&&empty(records)&&absent.reads.empty()&&error=="Subscriber Record API image missing","missing image admitted or touched memory");
    for(const auto bad_image:{UINTPTR_MAX,UINTPTR_MAX-record_entries[0]-uintptr_t(7)}){
        absent.reads.clear();records.allocate=reinterpret_cast<api::Allocate>(1);
        need(!api::resolve_record_api(absent,bad_image,records,error)&&empty(records)&&absent.reads.empty()&&error.find("address overflow")!=std::string::npos,"overflowed entry or 16-byte span reached Memory");
    }
    std::cout<<"PASS "<<checks<<" actual record/full-Append resolver checks\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
