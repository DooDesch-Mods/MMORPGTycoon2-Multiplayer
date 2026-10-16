#include "subscriber_lifecycle/record_decode.h"
#include "subscriber_lifecycle/native_calls.h"
#include <Windows.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace decode = mt2mp::subscriber_lifecycle::first_append;
namespace native = mt2mp::subscriber_lifecycle;
namespace {
struct String { char* data; size_t length; char storage[16]; };
struct Token { int32_t type, pad; String text; uint32_t bits, pad2; };
struct Array { uintptr_t ap; void* data; int32_t count, capacity; };
struct Record { Token label; Array tokens, children; uintptr_t last; int32_t a,b; uint8_t stream,pad[7]; };
static_assert(sizeof(Token)==0x30 && sizeof(Record)==0x78);
unsigned allocations=0, releases=0, loads=0, checks=0;
bool reject=false, crash=false;
void need(bool value,const char* cause) { if(!value) throw std::runtime_error(cause); ++checks; }
void* allocate(size_t bytes) { ++allocations; return std::calloc(1,bytes); }
void release(void* object) { ++releases; std::free(object); }
void construct_string(void* object,const char* bytes,size_t count,const void*) {
    auto& value=*static_cast<String*>(object); value.data=static_cast<char*>(std::malloc(count+1));
    std::memcpy(value.data,bytes,count); value.data[count]=0; value.length=count;
}
void destroy_string(void* object) { std::free(static_cast<String*>(object)->data); }
void construct(void* object) { auto& record=*static_cast<Record*>(object); record={}; construct_string(&record.label.text,"",0,nullptr); }
void destroy(void* object) {
    auto& record=*static_cast<Record*>(object); destroy_string(&record.label.text);
    for(int32_t i=0;i<record.tokens.count;++i) destroy_string(&static_cast<Token*>(record.tokens.data)[i].text);
    std::free(record.tokens.data);
}
void set_text(void* object,const void* input,int32_t type) {
    auto& token=*static_cast<Token*>(object); const auto& value=*static_cast<const String*>(input);
    destroy_string(&token.text); construct_string(&token.text,value.data,value.length,nullptr); token.type=type;
}
void label(void* object,const void* input) { set_text(object,input,1); }
void text(void* object,const void* input) { set_text(object,input,2); }
void count(void* object,int32_t size) {
    auto& values=static_cast<Record*>(object)->tokens; values.count=values.capacity=size;
    values.data=std::calloc(size_t(size),sizeof(Token));
    for(int32_t i=0;i<size;++i) construct_string(&static_cast<Token*>(values.data)[i].text,"",0,nullptr);
}
void integer(void* object,int32_t value) { auto& token=*static_cast<Token*>(object); token.type=4; std::memcpy(&token.bits,&value,4); }
struct Memory : native::Memory {
    bool read(uintptr_t address,void* output,size_t size,std::string&) override { std::memcpy(output,reinterpret_cast<void*>(address),size); return true; }
};
bool property(const void* descriptor,void* object,void* record,void* context) {
    ++loads; need(descriptor && object && record && !context,"scalar property did not use the pinned context-free ABI");
    if(crash) RaiseException(0xc0000005,0,0,nullptr);
    return !reject;
}
}
int main() { try {
    decode::DecodeApi api; api.allocate=allocate; api.release=release; api.record_construct=construct;
    api.record_destroy=destroy; api.string_destroy=destroy_string; api.set_label=label;
    api.set_string=text; api.token_count=count; api.set_integer=integer; api.string_construct=construct_string;
    Memory memory; std::string error;
    for(const std::string value : {std::string{},std::string("swords"),std::string(256,'x')}) {
        decode::NativeTree tree; decode::DecodeReceipt receipt;
        need(decode::decode_string_record(api,memory,"weaponCategory",value,tree,receipt,error),"string property record creation failed");
        const auto& record=*reinterpret_cast<Record*>(tree.root);
        need(receipt.complete && receipt.constructed==1 && record.tokens.count==1,"string property record shape differs");
        const auto& token=*static_cast<Token*>(record.tokens.data);
        need(token.type==2 && std::string(token.text.data,token.text.length)==value,"chosen category text changed");
        need(std::string(record.label.text.data,record.label.text.length)=="weaponCategory","property label changed");
        need(!decode::decode_string_record(api,memory,"weaponCategory",value,tree,receipt,error),"owned native record was replaced");
        need(decode::destroy_records(api,tree,receipt,error) && allocations==releases,"string property record cleanup failed");
    }
    decode::NativeTree tree; decode::DecodeReceipt receipt;
    need(decode::decode_integer_record(api,memory,"weaponVariantId",INT32_MIN,tree,receipt,error),"signed variant record creation failed");
    const auto& record=*reinterpret_cast<Record*>(tree.root);
    need(static_cast<Token*>(record.tokens.data)->bits==0x80000000u,"signed variant bits changed");
    native::CallReceipt call;
    need(native::load_scalar_property(property,&api,&memory,reinterpret_cast<void*>(tree.root),call,error) && call.attempted && call.returned,"scalar property result not confirmed");
    reject=true;
    need(!native::load_scalar_property(property,&api,&memory,reinterpret_cast<void*>(tree.root),call,error) && call.native_rejected && call.returned && error.find("rejected after entry")!=std::string::npos,"native property rejection lost");
    reject=false; crash=true;
    const auto before=loads;
    need(!native::load_scalar_property(property,&api,&memory,reinterpret_cast<void*>(tree.root),call,error) && call.attempted && !call.returned && call.seh==0xc0000005 && loads==before+1,"native property exception lost or replayed");
    need(!native::load_scalar_property(nullptr,&api,&memory,reinterpret_cast<void*>(tree.root),call,error) && !call.attempted && loads==before+1,"missing property function entered native code");
    need(decode::destroy_records(api,tree,receipt,error) && allocations==releases,"variant record cleanup failed");
    auto missing=api; missing.set_string=nullptr;
    need(!decode::decode_string_record(missing,memory,"weaponCategory","swords",tree,receipt,error) && !tree.root && allocations==releases,"missing string API allocated native objects");
    std::cout << "PASS " << checks << " actual native record/property cases\n"; return 0;
} catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; } }
