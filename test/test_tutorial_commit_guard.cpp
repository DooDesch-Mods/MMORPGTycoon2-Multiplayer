#include "tutorial_replication/owned_commit_entry.h"
#include <iostream>
#include <map>
#include <stdexcept>
using namespace mt2mp::game::tutorial_sync;
namespace {
constexpr uintptr_t target=0x100000,fixture_trampoline=0x110000,fixture_detour=0x120000;
struct Fixture {
    OwnedSequenceEntry owner{target,fixture_detour,fixture_trampoline,true,true};
    std::map<uintptr_t,unsigned char> memory;
    uintptr_t unreadable=0;
    template<class T>void store(uintptr_t p,const T& v){const auto* bytes=reinterpret_cast<const unsigned char*>(&v);for(size_t i=0;i<sizeof(T);++i)memory[p+i]=bytes[i];}
    Fixture(){
        for(size_t i=0;i<commit_entry.size();++i)memory[target+i]=commit_entry[i];
        memory[target]=0xe9;store<int32_t>(target+1,int32_t(fixture_trampoline+19-target-5));
        for(size_t i=0;i<5;++i)memory[fixture_trampoline+i]=commit_entry[i];
        constexpr std::array<unsigned char,6> jump{0xff,0x25,0,0,0,0};
        for(size_t i=0;i<jump.size();++i){memory[fixture_trampoline+5+i]=jump[i];memory[fixture_trampoline+19+i]=jump[i];}
        store<uintptr_t>(fixture_trampoline+11,target+5);store<uintptr_t>(fixture_trampoline+25,fixture_detour);
    }
    bool verify(std::string& e,uintptr_t requested=target){return verify_owned_commit_entry(owner,requested,[&](uintptr_t p,void* out,size_t n,std::string& error){if(p==unreadable){error="original owned-hook copy failure";return false;}auto* bytes=static_cast<unsigned char*>(out);for(size_t i=0;i<n;++i){if(!memory.contains(p+i)){error="fixture missing byte";return false;}bytes[i]=memory.at(p+i);}return true;},e);}
};
unsigned tests=0;
void need(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
void pass(const char* name){++tests;std::cout<<"PASS "<<name<<'\n';}
}
int main(){try{
    {Fixture f;std::string error="old";need(f.verify(error)&&error.empty(),"exact owner fixture_trampoline rejected");pass("exact physical owned Commit route is accepted");}
    for(unsigned flag=0;flag<2;++flag){Fixture f;if(!flag)f.owner.enabled=false;else f.owner.original_verified=false;std::string error;need(!f.verify(error)&&error=="tutorial Commit hook has no matching enabled owner","unconfirmed hook owner accepted");}pass("installed and original-verified ownership are both required");
    {Fixture f;std::string error;need(!f.verify(error,target+16),"other physical target inherited hook ownership");pass("exact physical entry target must match owner");}
    {Fixture f;f.memory[target+15]^=1;std::string error;need(!f.verify(error)&&error=="tutorial owned Commit entry patch/suffix differs","patched entry tail ignored");pass("untouched native entry suffix is checked");}
    {Fixture f;f.memory[target]=0x90;std::string error;need(!f.verify(error),"arbitrary patched entry accepted");pass("entry must contain exact owned E9 route");}
    {Fixture f;f.store<int32_t>(target+1,int32_t(fixture_trampoline+20-target-5));std::string error;need(!f.verify(error)&&error=="tutorial owned Commit relay differs","different relay inherited hook owner");pass("entry displacement must reach exact fixture_trampoline relay");}
    {Fixture f;f.memory[fixture_trampoline+4]^=1;std::string error;need(!f.verify(error)&&error=="tutorial owned Commit trampoline original/continuation differs","fixture_trampoline original instruction was unchecked");pass("fixture_trampoline must preserve exact original five bytes");}
    {Fixture f;f.store<uintptr_t>(fixture_trampoline+11,target+6);std::string error;need(!f.verify(error),"wrong original continuation accepted");pass("original continuation must return to entry plus five");}
    {Fixture f;f.memory[fixture_trampoline+19]=0x90;std::string error;need(!f.verify(error)&&error=="tutorial owned Commit trampoline detour differs","relay opcode unchecked");pass("relay opcode remains exact");}
    {Fixture f;f.store<uintptr_t>(fixture_trampoline+25,fixture_detour+1);std::string error;need(!f.verify(error)&&error=="tutorial owned Commit trampoline detour differs","foreign hook fixture_detour accepted");pass("relay must reach this owner's fixture_detour");}
    for(auto address:{target,fixture_trampoline}){Fixture f;f.unreadable=address;std::string error;need(!f.verify(error)&&error=="original owned-hook copy failure","copy failure cause replaced");}pass("entry and fixture_trampoline read failures preserve original cause");
    {Fixture f;f.owner.trampoline=UINTPTR_MAX-32;std::string error;need(!f.verify(error)&&error=="tutorial Commit hook range overflow","overflowed fixture_trampoline admitted");pass("overflow rejects before code reads");}
    std::cout<<tests<<" owned Commit guard scenarios passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
