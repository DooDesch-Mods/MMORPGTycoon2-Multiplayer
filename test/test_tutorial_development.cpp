#include "tutorial_development.h"
#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>
using namespace mt2mp::game::tutorial_development;
using nlohmann::json;
constexpr uintptr_t base=0x140000000,state=0x200000,mode=0x201000,world=0x202000,player=0x203000,lounge=0x204000,software=0x205000;
constexpr uintptr_t project=0x206000,tree=0x207000,tech=0x208000,record=0x209000,type=0x20a000,inventory=0x20b000,item=0x20c000,proxy=0x20d000;
constexpr uintptr_t tech_array=0x210000,record_array=0x211000,slots=0x212000;
constexpr uintptr_t release_manager=0x220000,release=0x221000,change=0x222000,change2=0x223000,change_array=0x224000;
struct Fake final:detail::Port {
    std::map<uintptr_t,unsigned char> memory;
    std::map<uintptr_t,int> reads;
    detail::Origin origin;
    std::function<void(detail::Origin&)> change_origin;
    uintptr_t unreadable=0,unstable=0;
    int research_calls=0;bool getter_failure=false;
    std::array<bool,4> getter_flags{true,false,true,true};
    bool research_flags(uintptr_t image,uintptr_t receiver,std::array<bool,4>& out,std::string& e)override{
        if(image!=base||receiver!=project){e="fixture getter receiver differs";return false;}
        ++research_calls;if(getter_failure){e="fixture original research getter failure";return false;}
        out=getter_flags;return true;
    }
    void research_setup(){
        constexpr uintptr_t business=0x230000,cash=0x231000,projects=0x232000;
        owned(state,0x1c0,0x13df220,business,0x16d97e0);put<uint8_t>(business+0xf0,0);
        put<uintptr_t>(business+0x100,cash);put<int32_t>(cash+0x20,88160);put<int32_t>(software+0x88,1);
        array(lounge+0xd8,projects,{project});str(project+0x40,"");put<int32_t>(project+0x60,0);
        put<uintptr_t>(tech+0x1e0,0);put<int32_t>(tech+0xa8,1);put<int32_t>(tech+0xac,10000);
    }
    int current_calls=0,fail_current=0;bool fail_image=false;
    Fake(){
        origin.game=reinterpret_cast<void*>(0x100000);origin.mode=reinterpret_cast<void*>(mode);origin.state=reinterpret_cast<void*>(state);
        origin.world=reinterpret_cast<void*>(world);origin.map=reinterpret_cast<void*>(0x202800);origin.generation=8;origin.world_epoch=5;origin.bytes=75843;origin.uuid="local-native-origin";origin.sha256="fixture-hash";
        fill_guard(0x206df0,"56534883ec284863da4889ce85db7805");fill_guard(0x52e610,"0fb6415984c075040fb6415ac3906690");
        fill_guard(0xd31590,"488d4148c39090909090909090909090");fill_guard(0x52b7d0,"56534883ec284889ce4889d34885d274");
        put<uintptr_t>(base+0x13df2a0,state);put<uintptr_t>(mode+0x58,state);put<uintptr_t>(state+0x160,world);vt(state,0x16de920);
        owned(mode,0x150,0x13df9f0,player,0x17873b0);owned(state,0x180,0x13df290,lounge,0x16de6c0);owned(state,0x1c8,0x13df500,software,0x1715450);
        put<uintptr_t>(lounge+0xa8,project);vt(project,0x16e48c0);put<int32_t>(project+0x18,0);str(project+0x20,"startingpoint");
        put<float>(lounge+0xb0,1.25f);put<int32_t>(lounge+0xb4,9);put<int32_t>(lounge+0xb8,3);put<int32_t>(lounge+0xbc,1);put<int32_t>(lounge+0xc4,1);
        put<uintptr_t>(base+0x13df250,tree);vt(tree,0x16dc400);array(tree+0x10,tech_array,{tech});vt(tech,0x16dc330);str(tech+0x20,"startingpoint");str(tech+0x1f0,"@StartingPoints");put<uintptr_t>(tech+0x1e8,record);
        array(software+0x280,record_array,{record});vt(record,0x16e79d0);str(record+0x18,"Starting Point");str(record+0x38,"startingpoint");put<uintptr_t>(record+0x60,tech);
        fill_guard(0x4df220,"4154555756534883ec2031db4889d78b");
        fill_guard(0x71e90,"5756534883ec20488951704889ce4889");
        fill_guard(0x4e34a0,"4157415641554154555756534881ecc8");
        owned(state,0x190,0x13df4f0,release_manager,0x1714ed0);
        put<uintptr_t>(release_manager+0x38,release);vt(release,0x16c12b0);
        array(release+0x150,change_array,{});put<uint32_t>(release+0x1c4,7);
        vt(change,0x1786d10);put<uintptr_t>(change+0x70,tech);str(change+0x18,"tech_release");
        vt(change2,0x1786d10);put<uintptr_t>(change2+0x70,tech);str(change2+0x18,"tech_release");
        flags(1,0,0);put<uintptr_t>(base+0x13e2050,type);put<uintptr_t>(base+0x13e2040,type);vt(type,0x173b7b0);put<uintptr_t>(base+0x173b7b0+0xa8,base+0xd31590);
        put<uintptr_t>(type+0x28,0);put<uintptr_t>(type+0x30,0);str(type+0x48,"@StartingPoints");put<uintptr_t>(type+0x40,proxy);
        put<uintptr_t>(player+0x1d0,inventory);vt(inventory,0x16df0f0);put<uintptr_t>(inventory+8,slots);put<int32_t>(inventory+0x10,200);
        for(size_t i=0;i<200;++i)put<uintptr_t>(slots+i*8,0);
        vt(item,0x17437f0);put<uintptr_t>(item+0x68,type);put<uintptr_t>(item+0x70,proxy);put<uintptr_t>(proxy,type+0x38);
    }
    void fill_guard(uintptr_t rva,const std::string& hex){for(size_t i=0;i<hex.size();i+=2)memory[base+rva+i/2]=static_cast<unsigned char>(std::stoul(hex.substr(i,2),nullptr,16));}
    template<class T>void put(uintptr_t p,const T& value){std::array<unsigned char,sizeof(T)> bytes{};std::memcpy(bytes.data(),&value,sizeof(T));for(size_t i=0;i<bytes.size();++i)memory[p+i]=bytes[i];}
    void vt(uintptr_t p,uintptr_t rva){put<uintptr_t>(p,base+rva);}
    void str(uintptr_t p,const std::string& s){const auto data=p+0x1000000;put<uintptr_t>(p,data);put<uint64_t>(p+8,s.size());for(size_t i=0;i<=s.size();++i)memory[data+i]=i==s.size()?0:static_cast<unsigned char>(s[i]);}
    void owned(uintptr_t p,size_t off,uintptr_t singleton,uintptr_t value,uintptr_t vtable){put<uintptr_t>(p+off,value);put<uintptr_t>(base+singleton,value);vt(value,vtable);}
    void array(uintptr_t p,uintptr_t data,const std::vector<uintptr_t>& entries){put<uintptr_t>(p,data);put<int32_t>(p+8,static_cast<int32_t>(entries.size()));put<int32_t>(p+12,4);for(size_t i=0;i<entries.size();++i)put<uintptr_t>(data+i*8,entries[i]);}
    void flags(uint8_t a,uint8_t b,uint8_t c){put<uint8_t>(record+0x58,a);put<uint8_t>(record+0x59,b);put<uint8_t>(record+0x5a,c);}
    bool current(detail::Origin& out,std::string& e)override{++current_calls;if(current_calls==fail_current){e="fixture NativeOpen original cause";return false;}out=origin;if(current_calls==2&&change_origin)change_origin(out);return true;}
    bool image(uintptr_t& out,std::string& e)override{if(fail_image){e="fixture module original cause";return false;}out=::base;return true;}
    bool copy(uintptr_t p,void* out,size_t count,std::string& e)override{
        if(p==unreadable){e="fixture memory original cause";return false;}++reads[p];
        if(p==unstable&&reads[p]>1)memory[p]^=1;
        auto* target=static_cast<unsigned char*>(out);for(size_t i=0;i<count;++i){if(!memory.contains(p+i)){e="fixture missing byte at "+std::to_string(p+i);return false;}target[i]=memory.at(p+i);}return true;
    }
};
int tests=0;
void require(bool v,const std::string& message){if(!v)throw std::runtime_error(message);}
json success(Fake& f){json out;std::string e="old error";const auto before=f.memory;const bool ok=detail::observe(f,out,e);require(ok,"unexpected rejection: "+e);require(e.empty(),"success retained error");require(f.memory==before,"reader mutated memory");++tests;return out;}
void rejects(Fake& f,const std::string& part){json out={{"stale",true}};std::string e;require(!detail::observe(f,out,e),"expected rejection: "+part);require(out.is_null(),"failure exported partial result");require(e.find(part)!=std::string::npos,"wrong failure cause: "+e+" expected "+part);++tests;}
int main(){try{
    {Fake f;const auto row=success(f);require(row["project"]["tech_key"]=="startingpoint"&&row["project"]["bugs_unknown"]==2,"project fields differ");require(row["startingpoint"]["purchased"]==true&&row["startingpoint"]["owned"]==false&&row["startingpoint"]["inventory_count"]==0,"purchased incorrectly implies owned/item");}
    {Fake f;f.flags(1,1,0);f.put<uintptr_t>(slots,item);auto row=success(f);require(row["startingpoint"]["owned"]==true&&row["startingpoint"]["inventory_count"]==1,"deployed item receipt differs");}
    {Fake f;f.flags(0,0,1);auto row=success(f);require(row["startingpoint"]["owned"]==true&&row["startingpoint"]["inventory_count"]==0,"reader inferred inventory from ownership");}
    {Fake f;f.put<uintptr_t>(slots,item);auto row=success(f);require(row["startingpoint"]["owned"]==false&&row["startingpoint"]["inventory_count"]==1,"reader hid inventory/flag mismatch");}
    {Fake f;f.put<uintptr_t>(tech+0x1e8,0);f.array(software+0x280,record_array,{});f.put<uintptr_t>(lounge+0xa8,0);auto row=success(f);require(row["project"]["present"]==false&&row["startingpoint"]["record_present"]==false&&row["startingpoint"]["owned"].is_null(),"absence not explicit");}
    {Fake f;f.put<int32_t>(lounge+0xb8,std::numeric_limits<int32_t>::min());f.put<int32_t>(lounge+0xbc,std::numeric_limits<int32_t>::max());auto row=success(f);require(row["project"]["bugs_unknown"]==-4294967295LL,"raw bug difference overflowed or hidden");}
    {Fake f;f.fail_current=1;rejects(f,"fixture NativeOpen original cause");}
    {Fake f;f.fail_current=2;rejects(f,"fixture NativeOpen original cause");}
    {Fake f;f.fail_image=true;rejects(f,"fixture module original cause");}
    {Fake f;f.unreadable=software+0x280;rejects(f,"fixture memory original cause");}
    {Fake f;f.memory[::base+0x52e610]^=1;rejects(f,"native code guard differs");}
    {Fake f;f.put<uintptr_t>(mode+0x58,0xdead);rejects(f,"GameState/Mode/World current owner differs");}
    {Fake f;f.put<uintptr_t>(::base+0x13df9f0,0xdead);rejects(f,"current owner differs: Player");}
    {Fake f;f.put<uintptr_t>(state+0x180,0xdead);rejects(f,"current owner differs: DevLounge");}
    {Fake f;f.put<uintptr_t>(::base+0x13df500,0xdead);rejects(f,"current owner differs: SoftwareStatus");}
    {Fake f;f.vt(project,0x16e79d0);rejects(f,"vtable differs: DevProject");}
    {Fake f;f.put<float>(lounge+0xb0,std::numeric_limits<float>::quiet_NaN());rejects(f,"work is not finite");}
    {Fake f;f.put<int32_t>(software+0x28c,0);rejects(f,"array bounds invalid");}
    {Fake f;f.array(tree+0x10,tech_array,{tech,tech});rejects(f,"owned array has null/duplicate member");}
    {Fake f;f.str(record+0x18,"startingpoint");f.str(record+0x38,"different_key");rejects(f,"record is not owned by current SoftwareStatus");}
    {Fake f;f.put<uintptr_t>(record+0x60,0xdead);rejects(f,"reciprocal link differs");}
    {Fake f;f.flags(0,2,0);rejects(f,"record flag invalid");}
    {Fake f;f.str(tech+0x1f0,"other");rejects(f,"native itemTypeName differs");}
    {Fake f;f.put<uintptr_t>(type+0x30,type);rejects(f,"catalogue cycle/count");}
    {Fake f;f.put<uintptr_t>(type+0x28,0xdead);rejects(f,"catalogue previous link differs");}
    {Fake f;f.put<uintptr_t>(::base+0x13e2040,0);rejects(f,"catalogue tail differs");}
    {Fake f;f.put<uintptr_t>(::base+0x173b7b0+0xa8,::base+0xd31591);rejects(f,"name getter unsupported");}
    {Fake f;f.put<int32_t>(inventory+0x10,201);rejects(f,"slot count=201 required=200");}
    {Fake f;f.put<uintptr_t>(slots,item);f.put<uintptr_t>(slots+8,item);rejects(f,"item owner duplicated");}
    {Fake f;f.put<uintptr_t>(slots,item);f.put<uintptr_t>(item+0x68,0xdead);rejects(f,"outside current catalogue");}
    {Fake f;f.put<uintptr_t>(slots,item);f.put<uintptr_t>(proxy,0);rejects(f,"weak target/owner differs");}
    {Fake f;f.put<uintptr_t>(slots,item);f.put<uintptr_t>(type+0x40,proxy+8);rejects(f,"weak target/owner differs");}
    {Fake f;f.put<uint64_t>(tech+0x28,201);rejects(f,"string bytes=201 required<=200");}
    {Fake f;f.unstable=record+0x58;rejects(f,"changed during read");}
    for(int field=0;field<10;++field){Fake f;f.change_origin=[field](auto& o){switch(field){case 0:o.game=nullptr;break;case 1:o.mode=nullptr;break;case 2:o.state=nullptr;break;case 3:o.world=nullptr;break;case 4:o.map=nullptr;break;case 5:++o.generation;break;case 6:++o.world_epoch;break;case 7:++o.bytes;break;case 8:o.uuid="changed";break;case 9:o.sha256="changed";break;}};rejects(f,"current world origin changed");}
    {Fake f;auto row=success(f);require(row["startingpoint_release"]==json{{"change_count",0},{"mutation_counter",7},{"feature_count",0},{"feature_present",false}},"absent release feature was inferred from purchase");}
    {Fake f;f.array(release+0x150,change_array,{change});auto row=success(f);require(row["startingpoint_release"]["feature_count"]==1&&row["startingpoint_release"]["feature_present"]==true,"owned exact-key feature absent");}
    {Fake f;f.array(release+0x150,change_array,{change});f.put<uintptr_t>(change+0x70,tech+8);auto row=success(f);require(row["startingpoint_release"]["feature_count"]==0,"foreign Tech owner counted");}
    {Fake f;f.array(release+0x150,change_array,{change});f.str(change+0x18,"tech_purchase");auto row=success(f);require(row["startingpoint_release"]["feature_count"]==0,"another feature key counted");}
    {Fake f;f.array(release+0x150,change_array,{change,change2});auto row=success(f);require(row["startingpoint_release"]["feature_count"]==2&&row["startingpoint_release"]["change_count"]==2,"distinct duplicate-feature rows hidden");}
    {Fake f;f.put<uint32_t>(release+0x1c4,0xffffffffu);auto row=success(f);require(row["startingpoint_release"]["mutation_counter"]==4294967295ULL,"raw counter bits narrowed or discarded");}
    {Fake f;f.put<uintptr_t>(state+0x190,0xdead);rejects(f,"ReleaseManager current owner differs");}
    {Fake f;f.put<uintptr_t>(::base+0x13df4f0,0xdead);rejects(f,"ReleaseManager current owner differs");}
    {Fake f;f.vt(release_manager,0x16c12b0);rejects(f,"vtable differs: ReleaseManager");}
    {Fake f;f.vt(release,0x1714ed0);rejects(f,"vtable differs: Release");}
    {Fake f;f.array(release+0x150,change_array,{change,change});rejects(f,"change array has null/duplicate member");}
    {Fake f;f.array(release+0x150,change_array,{0});rejects(f,"change array has null/duplicate member");}
    {Fake f;f.array(release+0x150,change_array,{change});f.vt(change,0x16dc330);rejects(f,"vtable differs: Change");}
    {Fake f;f.put<int32_t>(release+0x158,-1);rejects(f,"change bounds invalid");}
    {Fake f;f.put<int32_t>(release+0x158,5);rejects(f,"change bounds invalid");}
    {Fake f;f.put<int32_t>(release+0x15c,4097);rejects(f,"change bounds invalid");}
    {Fake f;f.array(release+0x150,0,{change});rejects(f,"change bounds invalid");}
    {Fake f;f.array(release+0x150,change_array,{change});f.put<uint64_t>(change+0x20,201);rejects(f,"key bytes=201 required<=200");}
    {Fake f;f.array(release+0x150,change_array,{change});f.memory[change+0x18+0x1000000+12]=1;rejects(f,"key terminator differs");}
    {Fake f;f.unreadable=release+0x1c4;rejects(f,"fixture memory original cause");}
    {Fake f;f.memory[::base+0x71e90]^=1;rejects(f,"startingpoint release code guard differs");}
    {Fake f;f.unstable=release+0x1c4;rejects(f,"changed during read");}
    {Fake f;f.unstable=release_manager+0x38;rejects(f,"changed during read");}
    {Fake f;f.array(release+0x150,change_array,{change});f.unstable=change+0x70;rejects(f,"changed during read");}
    {Fake f;f.array(release+0x150,change_array,{change});f.unstable=change_array;rejects(f,"changed during read");}
    auto research=[](Fake& f){json out;std::string e;const auto before=f.memory;
        require(detail::observe(f,out,e,nullptr,false,true),"research failed: "+e);
        require(e.empty()&&f.memory==before,"research modified memory/error");++tests;return out.at("research");};
    auto research_reject=[](Fake& f,const std::string& part){json out={{"stale",true}};std::string e;
        require(!detail::observe(f,out,e,nullptr,false,true),"research expected reject");
        require(out.is_null()&&e.find(part)!=std::string::npos,"research wrong cause: "+e);++tests;};
    {Fake f;success(f);require(f.research_calls==0,"legacy replication queried research");}
    {Fake f;f.research_setup();auto result=research(f);require(result["project_present"]==true&&result["cash"]==88160&&result["major_version"]==1,"research owner fields");
        const auto& row=result["rows"][0];require(row["key"]=="startingpoint"&&row["cost"]==10000&&row["required_major"]==1&&row["visible"]==true&&row["owned"]==false&&row["can_choose_now"]==true&&row["can_afford"]==true&&f.research_calls==1,"original getter results/receiver");}
    {Fake f;f.research_setup();f.put<uintptr_t>(lounge+0xa8,0);f.getter_flags={false,true,false,false};auto result=research(f);
        require(result["project_present"]==false&&result["rows"][0]["visible"]==false&&result["rows"][0]["owned"]==true&&result["rows"][0]["can_choose_now"]==false&&result["rows"][0]["can_afford"]==false,"getter results were inferred or hidden");}
    {Fake f;f.research_setup();f.getter_failure=true;research_reject(f,"original research getter failure");require(f.research_calls==1,"getter replay");}
    {Fake f;f.research_setup();f.put<uintptr_t>(base+0x13df220,0x230008);research_reject(f,"current owner differs: research Business");require(f.research_calls==0,"getter before owner authorization");}
    {Fake f;f.research_setup();f.put<uintptr_t>(record+0x60,tech+8);research_reject(f,"record/tech reciprocal link differs");require(f.research_calls==0,"getter on foreign record");}
    {Fake f;f.research_setup();f.put<uintptr_t>(tech+0x1e0,tech+8);research_reject(f,"prerequisite outside owned TechTree");require(f.research_calls==0,"getter on foreign prerequisite");}
    {Fake f;f.research_setup();f.unreadable=tech+0xa8;research_reject(f,"fixture memory original cause");require(f.research_calls==0,"getter after memory failure");}
    {Fake f;f.research_setup();f.unstable=software+0x88;research_reject(f,"changed during read");}
    {Fake f;f.research_setup();f.put<uint8_t>(0x230000+0xf0,2);research_reject(f,"version/sandbox flag invalid");}
    {Fake f;f.research_setup();f.put<int32_t>(project+0x18,2);auto result=research(f);require(result["rows"].empty()&&f.research_calls==0,"non-tech project passed getter");}
    {Fake f;f.research_setup();f.array(lounge+0xd8,0x232000,{project,project});research_reject(f,"null/duplicate member");}
    std::cout<<"PASS "<<tests<<" actual reader fixtures; no game writes or native calls\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
