#include "vendor/nlohmann/json.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
std::map<uintptr_t,std::vector<unsigned char>> memory;
std::map<std::pair<uintptr_t,size_t>,std::string> strings;
uintptr_t image=0x10000000;
size_t checks=0;
void check(bool ok,const char* cause){++checks;if(!ok)throw std::runtime_error(cause);}
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
template<class T>void put(uintptr_t p,size_t off,const T& value){auto& bytes=memory[p+off];bytes.resize(sizeof(value));std::memcpy(bytes.data(),&value,sizeof(value));}
template<class T>bool at(uintptr_t p,size_t off,T& value,std::string& e){auto found=memory.find(p+off);if(found==memory.end()||found->second.size()!=sizeof(T))return fail(e,"original checked memory read failed");std::memcpy(&value,found->second.data(),sizeof(T));return true;}
bool type(uintptr_t p,uintptr_t rva,std::string& e){uintptr_t value=0;return at(p,0,value,e)&&(value==image+rva||fail(e,"original class guard"));}
bool str(uintptr_t p,size_t off,std::string& value,std::string& e){const auto found=strings.find({p,off});if(found==strings.end())return fail(e,"original string read failed");value=found->second;return true;}
#include "tutorial_replication/research_release.inc"
void array(uintptr_t p,size_t off,uintptr_t data,const std::vector<uintptr_t>& rows){
    put(p,off+8,data);put(p,off+16,int32_t(rows.size()));put(p,off+20,int32_t(rows.size()+4));
    for(size_t i=0;i<rows.size();++i)put(data,i*8,rows[i]);
}
void node(uintptr_t p,uintptr_t owner,bool feature,const std::vector<uintptr_t>& children={}){
    put(p,0,image+0x1786d10);put(p,0x70,owner);strings[{p,0x18}]="tech_release";
    put(p,0xe8,int32_t(1));put(p,0xec,uint8_t(feature));array(p,0xb8,p+0x10000,children);
}
void setup(){
    memory.clear();strings.clear();put(0x1000,0,image+0x16c12b0);
    array(0x1000,0x148,0x2000,{0x3000,0x4000});array(0x1000,0x178,0x2100,{0x5000});
    node(0x3000,0xa000,true,{0x6000});node(0x4000,0xb000,true);node(0x5000,0xc000,false,{0x7000});node(0x6000,0xd000,false);node(0x7000,0xe000,false);
}
}
int main(){try{
    std::string e;research_release::Graph before,after;nlohmann::json receipt;
    setup();check(research_release::read(0x1000,before,e),"read actual feature/minor/child graph");
    check(before.changes.size()==5&&before.features.size()==2&&before.minor.size()==1,"complete membership");
    check(research_release::admissible(before,0xf000,e),"unreferenced Tech");
    check(!research_release::admissible(before,0xd000,e),"child Tech reference rejects before effect");
    node(0x8000,0xf000,true);array(0x1000,0x148,0x2200,{0x4000,0x8000,0x3000});
    check(research_release::read(0x1000,after,e),"read sorted addition");
    check(research_release::preserve(before,after,0xf000,receipt,e),"original sorting preserves old members");
    check(receipt.at("preserved")==true&&receipt.at("members_before")==5&&receipt.at("members_after")==6,"actual diagnostic counts");
    const auto good=after;
    after.minor.clear();check(!research_release::preserve(before,after,0xf000,receipt,e),"minor removal rejects");after=good;
    after.changes[0].children.clear();check(!research_release::preserve(before,after,0xf000,receipt,e),"child removal rejects");after=good;
    after.changes[0].owner++;check(!research_release::preserve(before,after,0xf000,receipt,e),"old owner change rejects");after=good;
    after.changes[0].key="changed";check(!research_release::preserve(before,after,0xf000,receipt,e),"old scalar change rejects");after=good;
    after.changes.back().owner++;check(!research_release::preserve(before,after,0xf000,receipt,e),"new wrong Tech rejects");after=good;
    after.changes.back().children={0x9000};check(!research_release::preserve(before,after,0xf000,receipt,e),"new unexpected children reject");
    setup();memory.erase(0x3000+0x70);check(!research_release::read(0x1000,after,e)&&e=="original checked memory read failed","read cause preserved");
    setup();put(0x1000,0x15c,int32_t(1));check(!research_release::read(0x1000,after,e),"count exceeds capacity");
    setup();put(0x1000,0x15c,int32_t(4097));check(!research_release::read(0x1000,after,e),"existing4096 bound");
    setup();put(0x2000,8,uintptr_t(0x3000));check(!research_release::read(0x1000,after,e),"duplicate root rejects");
    setup();put(0x5000+0x10000,0,uintptr_t(0x6000));check(!research_release::read(0x1000,after,e),"duplicate child ownership rejects");
    setup();put(0x7000,0,uintptr_t(0));check(!research_release::read(0x1000,after,e)&&e=="original class guard","child class guard");
    setup();put(0x3000,0xec,uint8_t(2));check(!research_release::read(0x1000,after,e),"Boolean guard");
    std::cout<<checks<<" actual Release graph checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<"Research Release failure: "<<x.what()<<'\n';return 1;}}
