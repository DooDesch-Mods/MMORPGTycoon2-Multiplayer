#include "tutorial_replication/state.h"
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>
using namespace mt2mp::game::tutorial_replication;
namespace {
uintptr_t image=0x140000000,scoped_sequence=0,scoped_startingpoint_step=0;
bool restoring=false;
std::string fault;
std::map<uintptr_t,unsigned char> bytes;
std::map<uintptr_t,std::string> strings;
uintptr_t unreadable=0,unwritable=0;
unsigned originals=0,tests=0;
void original_tech(void*){++originals;}
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
void block(const std::string& cause){if(fault.empty())fault=cause;}
template<class T>bool at(uintptr_t p,size_t offset,T& out,std::string& error){
    if(p+offset==unreadable)return fail(error,"original scoped memory read failed");
    auto* target=reinterpret_cast<unsigned char*>(&out);
    for(size_t i=0;i<sizeof(T);++i){auto it=bytes.find(p+offset+i);if(it==bytes.end())return fail(error,"fixture byte absent");target[i]=it->second;}return true;
}
template<class T>bool put(uintptr_t p,size_t offset,const T& value,std::string& error){
    if(p+offset==unwritable)return fail(error,"original completion store failed");
    const auto* source=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof(T);++i)bytes[p+offset+i]=source[i];return true;
}
template<class T>void seed(uintptr_t p,const T& value){std::string error;if(!put(p,0,value,error))throw std::runtime_error(error);}
bool str(uintptr_t p,size_t offset,std::string& value,std::string& error){auto it=strings.find(p+offset);if(it==strings.end())return fail(error,"original scoped string read failed");value=it->second;return true;}
struct NativeCursor {uintptr_t instance=0,sequence=0,steps=0;Cursor value;};
#include "native_scope.inc"
constexpr uintptr_t sequence=0x200000,array=0x210000;
uintptr_t step(int32_t index){return 0x300000+uintptr_t(index)*0x100;}
NativeCursor setup(){
    bytes.clear();strings.clear();fault.clear();originals=0;unreadable=unwritable=0;scoped_sequence=scoped_startingpoint_step=0;restoring=false;
    NativeCursor n;n.sequence=sequence;n.steps=array;n.value.scenario="initial_tutorial";n.value.count=107;n.value.next=90;
    seed(sequence+0x20,array);seed<int32_t>(sequence+0x28,107);
    for(int32_t i=0;i<107;++i){seed(array+size_t(i)*8,step(i));seed(step(i),image+0x16fda00);strings[step(i)+0x20]="";strings[step(i)+0x40]="";seed<uint8_t>(step(i)+0x18,0);}
    seed(step(93),image+0x17206c0);strings[step(93)+0x40]="startingpoint";return n;
}
startingpoint::Row project(){startingpoint::Row r;r.project_present=r.purchased=true;return r;}
void require(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
void pass(const char* name){++tests;std::cout<<"PASS "<<name<<'\n';}
uint8_t completed(int32_t index){uint8_t value=0;std::string error;require(at(step(index),0x18,value,error),"missing completion readback");return value;}
}
int main(){try{
    {setup();tech_start(reinterpret_cast<void*>(step(93)));require(originals==1&&fault.empty()&&!completed(93),"foreign CompleteTech was swallowed");pass("outside scoped adoption CompleteTech calls original once");}
    {auto n=setup();Cursor target=n.value;target.next=96;std::string error;require(interval(n,target,project(),error),"coalesced valid Startingpoint interval rejected");Scope scope(sequence,false,step(93));tech_start(reinterpret_cast<void*>(step(93)));require(!originals&&fault.empty()&&completed(93),"admitted Step93 called duplicate original Deploy");pass("actual coalesced interval admits only fixed Step93 and completes it without original Deploy");}
    {setup();{Scope scope(sequence,false,step(93));tech_start(reinterpret_cast<void*>(step(94)));}require(!originals&&fault=="unadmitted scoped tutorial CompleteTech"&&!completed(94),"foreign scoped technology got authority");pass("other scoped CompleteTech is blocked visibly without completion");}
    {setup();seed(array+94*8,step(93));Scope scope(sequence,false,step(93));tech_start(reinterpret_cast<void*>(step(93)));require(!originals&&fault=="tutorial scoped step is not uniquely owned"&&!completed(93),"duplicated target ownership was accepted");pass("duplicate step membership rejects before effect");}
    {setup();unreadable=sequence+0x20;Scope scope(sequence,false,step(93));tech_start(reinterpret_cast<void*>(step(93)));require(!originals&&fault=="original scoped memory read failed","read failure cause lost");pass("scoped owner read failure retains original cause");}
    {setup();unwritable=step(93)+0x18;Scope scope(sequence,false,step(93));tech_start(reinterpret_cast<void*>(step(93)));require(!originals&&fault=="original completion store failed"&&!completed(93),"completion store failure was acknowledged");pass("completion store failure remains visible with no original replay");}
    {setup();{Scope outer(sequence,false,step(93));{Scope restore(sequence,true);tech_start(reinterpret_cast<void*>(step(94)));}require(scoped_sequence==sequence&&!restoring&&scoped_startingpoint_step==step(93),"nested PostLoad scope lost outer ownership");}require(!scoped_sequence&&!restoring&&!scoped_startingpoint_step&&completed(94)&&!originals&&fault.empty(),"PostLoad suppression or scope restoration changed");pass("PostLoad keeps existing suppression and restores exact outer scope");}
    {auto n=setup();Cursor target=n.value;target.next=96;std::string error;require(!interval(n,target,std::nullopt,error)&&error.find("requires supported authoritative Startingpoint")!=std::string::npos,"coalesced Step93 lacked authoritative Row");pass("coalesced interval without a Host technology Row is rejected");}
    {auto n=setup();strings[step(93)+0x40]="inn";Cursor target=n.value;target.next=96;std::string error;require(!interval(n,target,project(),error),"foreign tech key inherited Step93 admission");pass("coalesced foreign technology key is rejected");}
    std::cout<<tests<<" actual native scope scenarios passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
