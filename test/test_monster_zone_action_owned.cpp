#include "zone_memory_fixture.h"
#include "monster_zone_action_codes.h"
namespace monster_zone_action_codes=mt2mp::game::monster_zone_action_codes;
void press_entry(void*,void*,const float*){}void paint_entry(void*,const void*){}void finished_entry(void*,const void*){}void* selection_entry(void*){return nullptr;}
using Press=void(*)(void*,void*,const float*);using Binary=void(*)(void*,const void*);using GetSelection=void*(*)(void*);
struct Runtime {uintptr_t image=0x10000000;Press press=nullptr;Binary paint=nullptr,finished=nullptr;GetSelection get=nullptr;} state;
bool fail(std::string& e,const std::string& reason){e=reason;return false;}
std::map<std::string,std::string> hashes;
namespace quest_bridge {bool sha256(const std::string& input,std::string& out,std::string&){const auto p=hashes.find(input);out=p==hashes.end()?std::string(64,'0'):p->second;return true;}}
#include "zone_code_body.inc"
struct Test {
    Memory m;size_t index;uintptr_t original=0x30000000,relay=0x40000000;std::string e;
    explicit Test(size_t i):index(i){const auto& c=monster_zone_action_codes::values[i];std::string bytes(c.size,'\0');
        std::memcpy(bytes.data(),c.prefix.data(),c.prefix.size());for(size_t n=0;n<bytes.size();++n)m.data[state.image+c.rva+n]=uint8_t(bytes[n]);
        hashes.emplace(c.span?bytes.substr(5):bytes,c.tail_sha);
        state.press=reinterpret_cast<Press>(original);state.paint=reinterpret_cast<Binary>(original);state.finished=reinterpret_cast<Binary>(original);state.get=reinterpret_cast<GetSelection>(original);
    }
    void hook(){const auto& c=monster_zone_action_codes::values[index];m.put(state.image+c.rva,uint8_t(0xe9));
        m.put(state.image+c.rva+1,int32_t(relay-(state.image+c.rva+5)));
        for(size_t n=0;n<c.span;++n)m.data[original+n]=c.prefix[n];
        const std::array<uint8_t,6> ff25{0xff,0x25,0,0,0,0};m.put(original+c.span,ff25);m.put(original+c.span+6,state.image+c.rva+c.span);
        m.put(relay,ff25);const uintptr_t entries[]{uintptr_t(&press_entry),uintptr_t(&paint_entry),uintptr_t(&finished_entry),uintptr_t(&selection_entry)};m.put(relay+6,entries[index]);
    }
};
int main(){try{unsigned checks=0;
    for(size_t i=0;i<std::size(monster_zone_action_codes::values);++i){Test t(i);require(code(t.m,i,false,t.e),t.e);++checks;}
    for(size_t i=0;i<4;++i){Test t(i);t.hook();require(code(t.m,i,true,t.e),t.e);++checks;}
    const auto bad=[&](auto alter,const char* expected){Test t(3);t.hook();alter(t);require(!code(t.m,3,true,t.e)&&t.e.find(expected)!=std::string::npos,t.e);++checks;};
    bad([](Test& t){t.m.put(t.relay+6,uintptr_t(0x1234));},"relay");
    bad([](Test& t){t.m.put(t.original+7+6,uintptr_t(0x1234));},"trampoline");
    bad([](Test& t){t.m.put(t.original,uint8_t(0x90));},"trampoline");
    bad([](Test& t){t.m.put(t.relay,uint8_t(0x90));},"relay");
    bad([](Test& t){t.m.put(state.image+0x67fb40,uint8_t(0xeb));},"trampoline");
    bad([](Test& t){t.m.put(state.image+0x67fb40+16,uint8_t(0x90));},"immutable");
    {Test t(3);t.hook();require(!code(t.m,3,false,t.e)&&t.e.find("original entry")!=std::string::npos,t.e);++checks;}
    std::cout<<checks<<" actual owned Zone hook guard cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
