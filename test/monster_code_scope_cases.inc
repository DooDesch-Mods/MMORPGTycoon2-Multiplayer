namespace monster_code_scope_fixture {
namespace live_host_state=mt2mp::game::live_host_state;
#include "monster_live_codes.h"
struct LiveHook {uintptr_t rva=0;void* original=nullptr;std::array<unsigned char,32> entry{};std::array<unsigned char,64> trampoline{};};
LiveHook live_update_hook{0x3c0750},live_adjust_hook{0x39fb80};
struct {uintptr_t image=0;} state;
size_t observer_calls=0,hash_calls=0;
uintptr_t observer_owned=0,observer_failure=0;
bool hash_failure=false,hash_wrong=false;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
struct Memory {
    size_t copies=0;uintptr_t failure=0;bool throws=false,drift=false;
    bool copy(uintptr_t address,void* out,size_t size,std::string& e){
        need(t::PageReadScope::reader_active(),"Monster code bytes require the pure Reader extent");++copies;
        if(throws)throw std::runtime_error("original Monster code read exception");
        if(address==failure)return fail(e,"original Monster code read failure");
        MEMORY_BASIC_INFORMATION info{};
        if(t::virtual_query(reinterpret_cast<LPCVOID>(address),&info,sizeof info,t::Part::QueryWorldProp)!=sizeof info)
            return fail(e,"fixture Monster code metadata unavailable");
        unsigned char marker=0;
        for(size_t i=0;i<std::size(monster_live_codes::values);++i)
            if(address==state.image+monster_live_codes::values[i].rva)marker=static_cast<unsigned char>(i+1);
        if(drift&&size==32)++marker;
        std::memset(out,marker,size);return true;
    }
};
namespace interventions::actors::live {
bool verify_observer(uintptr_t rva,const char*,bool& handled,std::string& e){
    need(t::PageReadScope::reader_active(),"Monster observer receipt remains within pure code validation");++observer_calls;
    if(rva==observer_failure)return fail(e,"original Monster observer failure");
    handled=rva==observer_owned;return true;
}
}
namespace quest_bridge {
bool sha256(const std::string& bytes,std::string& out,std::string& e){
    ++hash_calls;if(hash_failure)return fail(e,"original Monster hash failure");
    const size_t i=static_cast<unsigned char>(bytes.at(0))-1;
    need(i<std::size(monster_live_codes::values),"Monster actual body passes freshly read bytes into hash");
    const auto& code=monster_live_codes::values[i];
    out=hash_wrong?"changed":bytes.size()==code.size?code.sha:code.tail;
    return true;
}
}
#include "monster_code_scope_actual.inc"
void run(){
    struct Owned {void* memory=nullptr;~Owned(){if(memory&&!VirtualFree(memory,0,MEM_RELEASE))std::cerr<<"Monster code fixture release failed "<<GetLastError()<<'\n';}} owned;
    owned.memory=VirtualAlloc(nullptr,32*1024*1024,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    need(owned.memory!=nullptr,"Monster code fixture allocation");state.image=uintptr_t(owned.memory);
    Memory memory;std::string error;
    const auto reset=[&]{memory={};observer_calls=hash_calls=0;observer_owned=observer_failure=0;hash_failure=hash_wrong=false;error.clear();};
    t::Scope take(t::Part::Take);
    reset();const auto before=count(t::Part::QueryWorldProp);
    need(live_code(memory,false,error)&&observer_calls==46&&memory.copies==46&&hash_calls==46,"Monster scope retains all 46 fresh code and observer checks");
    need(count(t::Part::QueryWorldProp)-before<=2,"Monster code reads share only permission metadata");
    need(!t::PageReadScope::reader_active(),"Monster successful code check closes its Reader scope");
    hash_wrong=true;need(!live_code(memory,false,error)&&error.find("Monster live body differs RVA=")==0,"Second Monster check reads again and rejects changed bytes");
    reset();observer_owned=monster_live_codes::values[2].rva;
    need(live_code(memory,false,error)&&observer_calls==46&&memory.copies==45&&hash_calls==45,"Owned observer retains its existing verification route");
    reset();observer_failure=monster_live_codes::values[1].rva;
    need(!live_code(memory,false,error)&&error=="original Monster observer failure"&&memory.copies==1,"Observer rejection preserves cause and stops at same body");
    reset();memory.failure=state.image+monster_live_codes::values[1].rva;
    need(!live_code(memory,false,error)&&error=="original Monster code read failure"&&hash_calls==1,"Code read failure remains distinguishable");
    reset();hash_failure=true;need(!live_code(memory,false,error)&&error=="original Monster hash failure","Hash failure preserves original cause");
    reset();live_update_hook.original=reinterpret_cast<void*>(state.image+0x1e00000);live_adjust_hook.original=reinterpret_cast<void*>(state.image+0x1e01000);
    live_update_hook.entry.fill(1);live_adjust_hook.entry.fill(2);
    need(live_code(memory,true,error)&&memory.copies==50&&hash_calls==46,"Hooked check retains both entries, trampolines and tails");
    reset();memory.drift=true;need(!live_code(memory,true,error)&&error=="Monster live owned hook entry/trampoline changed","Hook entry drift is still rejected");
    reset();memory.failure=uintptr_t(live_update_hook.original);
    need(!live_code(memory,true,error)&&error=="original Monster code read failure","Trampoline read failure retains its cause");
    reset();memory.throws=true;bool threw=false;try{live_code(memory,false,error);}catch(const std::runtime_error& exception){threw=std::string(exception.what())=="original Monster code read exception";}
    need(threw&&!t::PageReadScope::reader_active(),"Monster exception closes Reader scope without swallowing failure");
    reset();{t::PageReadScope parent;auto* original=t::page_read_scope;
        need(live_code(memory,false,error)&&t::page_read_scope==original,"Monster borrows existing pure Reader without resetting it");}
    need(!t::PageReadScope::reader_active(),"Monster nested scope ends at original boundary");
}
}
