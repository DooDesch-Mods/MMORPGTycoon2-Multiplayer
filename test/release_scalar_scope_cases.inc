#include "release_edit_reader.h"
#include <cstring>
namespace release_scalar_scope_fixture {
namespace p=mt2mp::game::release_edit;
namespace live=mt2mp::game::live_host_state;
struct Runtime {uintptr_t image=0x10000000;};
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;unsigned reads=0,funds_reads=0;bool drift=false,throws=false;
    template<class T>void put(uintptr_t address,const T& value){const auto* data=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=data[i];}
    template<class T>bool at(uintptr_t object,size_t offset,T& value,std::string& error){
        need(t::PageReadScope::reader_active(),"Release paired scalar read escaped its pure scope");++reads;
        if(throws)throw std::runtime_error("fixture scalar read exception");
        if(object==0x2000&&offset==0xb8&&++funds_reads==2&&drift)put(object+offset,int32_t(99));
        auto* out=reinterpret_cast<unsigned char*>(&value);for(size_t n=0;n<sizeof value;++n){const auto found=bytes.find(object+offset+n);if(found==bytes.end()){error="fixture scalar byte absent";return false;}out[n]=found->second;}return true;
    }
    void setup(uintptr_t image){
        bytes.clear();reads=funds_reads=0;drift=throws=false;
        put(image+0x13df4f0,uintptr_t(0x2000));put(image+0x13df500,uintptr_t(0x3000));put(image+0x164b250,uintptr_t(0x5000));
        put(0x2000,image+0x1714ed0);put(0x3000,image+0x1715450);put(0x2038,uintptr_t(0x4000));put(0x4000,image+0x16c12b0);
        put(0x5000,std::array<uint64_t,4>{1,2,3,4});put(0x3088,std::array<int32_t,3>{1,0,0});put(0x4050,std::array<int32_t,3>{1,0,1});
        put(0x4060,int32_t(1));put(0x4064,int32_t(0));put(0x20b8,int32_t(0));put(0x4100,uint8_t(0));
        for(uintptr_t address:{0x4150,0x4180,0x2048,0x2090}){put(address,uintptr_t(0));put(address+8,int32_t(0));put(address+12,int32_t(0));}
    }
};
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
bool sample(Runtime& r,Memory& memory,p::Local& out,p::State& state,std::string& e){
    auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& error){return memory.at(object,offset,value,error);};
    auto text=[](uintptr_t,std::string& value,std::string&){need(t::PageReadScope::reader_active(),"Release text read escaped pure scope");value="stable";return true;};
    p::Local before;p::State first;
#include "release_scalar_pair_actual.inc"
    return true;
}
void run(){
    Runtime runtime;Memory memory;p::Local local;p::State state;std::string error;
    memory.setup(runtime.image);need(sample(runtime,memory,local,state,error)&&state.text[0]=="stable"&&memory.funds_reads==2,"Release actual paired sample keeps both complete live reads");
    need(!t::PageReadScope::reader_active(),"Release successful sample retained page scope");
    memory.setup(runtime.image);memory.drift=true;need(!sample(runtime,memory,local,state,error)&&error=="Release actual owners/text/state changed during double read","Release paired reader still rejects fresh value drift");
    need(!t::PageReadScope::reader_active(),"Release value rejection retained page scope");
    memory.setup(runtime.image);memory.bytes.erase(0x2000);need(!sample(runtime,memory,local,state,error)&&error=="fixture scalar byte absent","Release reader preserves original failure");
    need(!t::PageReadScope::reader_active(),"Release failed read retained page scope");
    memory.setup(runtime.image);memory.throws=true;bool threw=false;try{sample(runtime,memory,local,state,error);}catch(const std::runtime_error& exception){threw=std::string(exception.what())=="fixture scalar read exception";}
    need(threw&&!t::PageReadScope::reader_active(),"Release original exception propagates and closes page scope");
}
}
