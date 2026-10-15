#include "tutorial_development.h"
#include <array>
#include <cmath>
#include <cstring>
#include <set>
#include <vector>

namespace mt2mp::game::tutorial_development::detail {
namespace {
constexpr size_t byte_limit=1024*1024;
constexpr int32_t catalogue_limit=4096,inventory_slots=200;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
bool same(const Origin& a,const Origin& b){
    return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&
        a.generation==b.generation&&a.world_epoch==b.world_epoch&&a.bytes==b.bytes&&a.uuid==b.uuid&&a.sha256==b.sha256;
}
struct Witness {uintptr_t address;std::vector<unsigned char> bytes;};
class Reader {
    Port& port_;std::vector<Witness> reads_;size_t count_=0;
public:
    explicit Reader(Port& port):port_(port){}
    bool bytes(uintptr_t p,void* out,size_t count,std::string& error){
        if(!p||!count||count>byte_limit-count_||p>UINTPTR_MAX-count)return fail(error,"development memory range/byte budget invalid required<=1048576");
        Witness w{p,std::vector<unsigned char>(count)};if(!port_.copy(p,w.bytes.data(),count,error))return false;
        std::memcpy(out,w.bytes.data(),count);count_+=count;reads_.push_back(std::move(w));return true;
    }
    template<class T>bool at(uintptr_t p,size_t offset,T& out,std::string& error){
        if(!p||p>UINTPTR_MAX-offset)return fail(error,"development owner/offset invalid");return bytes(p+offset,&out,sizeof(out),error);
    }
    bool type(uintptr_t p,uintptr_t expected,const char* label,std::string& error){
        uintptr_t actual=0;if(!at(p,0,actual,error))return false;
        return actual==expected||fail(error,std::string("development vtable differs: ")+label);
    }
    bool string(uintptr_t p,size_t offset,std::string& out,std::string& error){
        uintptr_t data=0;uint64_t size=0;if(!at(p,offset,data,error)||!at(p,offset+8,size,error))return false;
        if(size>200)return fail(error,"development string bytes="+std::to_string(size)+" required<=200");
        std::array<char,201> buffer{};if(!bytes(data,buffer.data(),size_t(size)+1,error))return false;
        if(buffer[size]||std::memchr(buffer.data(),0,size_t(size)))return fail(error,"development string terminator differs");
        out.assign(buffer.data(),size_t(size));for(unsigned char c:out)if(c<32||c==127)return fail(error,"development string contains control byte");
        try{static_cast<void>(nlohmann::json(out).dump());}catch(const nlohmann::json::exception&){return fail(error,"development string is not valid UTF-8");}return true;
    }
    bool array(uintptr_t p,size_t offset,std::vector<uintptr_t>& out,std::string& error){
        uintptr_t data=0;int32_t count=0,capacity=0;
        if(!at(p,offset,data,error)||!at(p,offset+8,count,error)||!at(p,offset+12,capacity,error))return false;
        if(count<0||capacity<count||capacity>catalogue_limit||(!data&&count))return fail(error,"development array bounds invalid count="+std::to_string(count)+" capacity="+std::to_string(capacity)+" required 0<=count<=capacity<=4096");
        out.resize(size_t(count));if(count&&!bytes(data,out.data(),out.size()*8,error))return false;
        std::set<uintptr_t> seen;for(auto item:out)if(!item||!seen.insert(item).second)return fail(error,"development owned array has null/duplicate member");return true;
    }
    bool stable(std::string& error){
        for(const auto& w:reads_){std::vector<unsigned char> after(w.bytes.size());if(!port_.copy(w.address,after.data(),after.size(),error))return false;
            if(after!=w.bytes)return fail(error,"development owner/catalogue/record/inventory changed during read");}return true;
    }
};
bool guard(Reader& r,uintptr_t image,std::string& error){
    struct Entry{uintptr_t rva;std::array<unsigned char,16> bytes;};
    constexpr Entry entries[]={
        {0x206df0,{0x56,0x53,0x48,0x83,0xec,0x28,0x48,0x63,0xda,0x48,0x89,0xce,0x85,0xdb,0x78,0x05}},
        {0x52e610,{0x0f,0xb6,0x41,0x59,0x84,0xc0,0x75,0x04,0x0f,0xb6,0x41,0x5a,0xc3,0x90,0x66,0x90}},
        {0xd31590,{0x48,0x8d,0x41,0x48,0xc3,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90}},
        {0x52b7d0,{0x56,0x53,0x48,0x83,0xec,0x28,0x48,0x89,0xce,0x48,0x89,0xd3,0x48,0x85,0xd2,0x74}}};
    for(const auto& entry:entries){std::array<unsigned char,16> bytes{};if(!r.bytes(image+entry.rva,bytes.data(),bytes.size(),error))return false;
        if(bytes!=entry.bytes)return fail(error,"development native code guard differs RVA="+std::to_string(entry.rva));}return true;
}
bool owner(Reader& r,uintptr_t image,uintptr_t parent,size_t offset,uintptr_t singleton,uintptr_t vtable,const char* label,uintptr_t& out,std::string& error){
    uintptr_t global=0;if(!r.at(parent,offset,out,error)||!r.at(image,singleton,global,error))return false;
    if(!out||out!=global)return fail(error,std::string("development current owner differs: ")+label);
    return r.type(out,image+vtable,label,error);
}
bool project(Reader& r,uintptr_t image,uintptr_t lounge,nlohmann::json& out,std::string& error){
    uintptr_t current=0;float work=0;int32_t lines=0,total=0,known=0,workers=0;
    if(!r.at(lounge,0xa8,current,error)||!r.at(lounge,0xb0,work,error)||!r.at(lounge,0xb4,lines,error)||
       !r.at(lounge,0xb8,total,error)||!r.at(lounge,0xbc,known,error)||!r.at(lounge,0xc4,workers,error))return false;
    if(!std::isfinite(work))return fail(error,"development project work is not finite");
    out={{"present",current!=0},{"kind",nullptr},{"tech_key",nullptr},{"work",work},{"lines",lines},
         {"bugs_total",total},{"bugs_known",known},{"bugs_unknown",int64_t(total)-int64_t(known)},{"desired_workers",workers}};
    if(current){int32_t kind=0;std::string key;if(!r.type(current,image+0x16e48c0,"DevProject",error)||!r.at(current,0x18,kind,error)||!r.string(current,0x20,key,error))return false;
        if(kind<0||kind>3)return fail(error,"development project kind outside0..3");out["kind"]=kind;if(kind==0)out["tech_key"]=key;}return true;
}
bool technology(Reader& r,uintptr_t image,uintptr_t software,nlohmann::json& out,std::string& error){
    uintptr_t tree=0,tech=0,record=0,linked=0;if(!r.at(image,0x13df250,tree,error)||!r.type(tree,image+0x16dc400,"TechTree",error))return false;
    std::vector<uintptr_t> techs,records;if(!r.array(tree,0x10,techs,error)||!r.array(software,0x280,records,error))return false;
    for(auto candidate:techs){std::string key;if(!r.string(candidate,0x20,key,error))return false;if(key=="startingpoint"){
        if(tech)return fail(error,"development startingpoint TechTree key duplicated");tech=candidate;}}
    if(!tech)return fail(error,"development startingpoint TechTree member absent");
    std::string item_name;if(!r.type(tech,image+0x16dc330,"startingpoint TechItem",error)||!r.string(tech,0x1f0,item_name,error)||!r.at(tech,0x1e8,linked,error))return false;
    if(item_name!="@StartingPoints")return fail(error,"development startingpoint native itemTypeName differs");
    for(auto candidate:records){std::string key;if(!r.type(candidate,image+0x16e79d0,"TechRecord",error)||!r.string(candidate,0x38,key,error))return false;
        if(key=="startingpoint"){if(record)return fail(error,"development startingpoint SoftwareStatus record duplicated");record=candidate;}}
    if(linked!=record)return fail(error,"development startingpoint record is not owned by current SoftwareStatus");
    out={{"key","startingpoint"},{"item_type_name",item_name},{"record_present",record!=0},{"purchased",nullptr},{"deployed",nullptr},{"custom",nullptr},{"owned",nullptr}};
    if(record){uintptr_t back=0;std::array<uint8_t,3> flags{};if(!r.at(record,0x60,back,error)||!r.bytes(record+0x58,flags.data(),flags.size(),error))return false;
        if(back!=tech)return fail(error,"development startingpoint record/tech reciprocal link differs");
        for(auto value:flags)if(value>1)return fail(error,"development startingpoint record flag invalid");
        out["purchased"]=flags[0]!=0;out["deployed"]=flags[1]!=0;out["custom"]=flags[2]!=0;out["owned"]=flags[1]!=0||flags[2]!=0;}return true;
}
bool inventory(Reader& r,uintptr_t image,uintptr_t player,int32_t& matches,std::string& error){
    uintptr_t head=0,tail=0,target=0,previous=0;
    if(!r.at(image,0x13e2050,head,error)||!r.at(image,0x13e2040,tail,error))return false;
    std::set<uintptr_t> types;
    for(auto node=head;node;){uintptr_t back=0,next=0,table=0,getter=0;std::string name;
        if(types.size()>=size_t(catalogue_limit)||!types.insert(node).second)return fail(error,"development ItemType catalogue cycle/count exceeds4096");
        if(!r.at(node,0x28,back,error)||!r.at(node,0x30,next,error)||!r.at(node,0,table,error)||!r.at(table,0xa8,getter,error))return false;
        if(back!=previous)return fail(error,"development ItemType catalogue previous link differs");
        if(getter!=image+0xd31590)return fail(error,"development ItemType name getter unsupported");
        if(!r.string(node,0x48,name,error))return false;
        if(name=="@StartingPoints"){if(target)return fail(error,"development StartingPoints ItemType catalogue key duplicated");target=node;}
        previous=node;node=next;
    }
    if(previous!=tail)return fail(error,"development ItemType catalogue tail differs");
    if(!target)return fail(error,"development StartingPoints live ItemType catalogue member absent");
    if(!r.type(target,image+0x173b7b0,"StartingPoints CursorBehaviour ItemType",error))return false;
    uintptr_t owned=0,slots=0;int32_t count=0;
    if(!r.at(player,0x1d0,owned,error)||!r.type(owned,image+0x16df0f0,"Inventory",error)||!r.at(owned,8,slots,error)||!r.at(owned,0x10,count,error))return false;
    if(count!=inventory_slots)return fail(error,"development Player inventory slot count="+std::to_string(count)+" required=200");
    std::array<uintptr_t,inventory_slots> items{};if(!r.bytes(slots,items.data(),sizeof(items),error))return false;
    std::set<uintptr_t> seen;matches=0;
    for(auto item:items){if(!item)continue;if(!seen.insert(item).second)return fail(error,"development Inventory item owner duplicated");
        uintptr_t type=0,proxy=0,proxy_target=0,type_proxy=0;
        if(!r.type(item,image+0x17437f0,"Item",error)||!r.at(item,0x68,type,error)||!r.at(item,0x70,proxy,error))return false;
        if(!types.contains(type))return fail(error,"development Inventory ItemType is outside current catalogue");
        if(!proxy)return fail(error,"development Inventory Item weak proxy absent");
        if(!r.at(proxy,0,proxy_target,error)||!r.at(type,0x40,type_proxy,error))return false;
        if(type>UINTPTR_MAX-0x38||proxy_target!=type+0x38||type_proxy!=proxy)return fail(error,"development Inventory Item weak target/owner differs");
        if(type==target)++matches;
    }return true;
}
}
bool observe(Port& port,nlohmann::json& result,std::string& error){
    result=nullptr;
    try{
        Origin before;if(!port.current(before,error))return false;
        if(!before.game||!before.mode||!before.state||!before.world||!before.map||!before.generation||!before.world_epoch||!before.bytes||before.uuid.empty()||before.sha256.empty())return fail(error,"development current world origin incomplete");
        uintptr_t image=0;if(!port.image(image,error))return false;
        if(!image||image>UINTPTR_MAX-0x1800000)return fail(error,"development native image range invalid");
        Reader r(port);if(!guard(r,image,error))return false;
        const auto state=reinterpret_cast<uintptr_t>(before.state),mode=reinterpret_cast<uintptr_t>(before.mode);
        uintptr_t global_state=0,mode_state=0,owned_world=0,player=0,lounge=0,software=0;
        if(!r.at(image,0x13df2a0,global_state,error)||!r.at(mode,0x58,mode_state,error)||!r.at(state,0x160,owned_world,error))return false;
        if(global_state!=state||mode_state!=state||owned_world!=reinterpret_cast<uintptr_t>(before.world))return fail(error,"development GameState/Mode/World current owner differs");
        if(!r.type(state,image+0x16de920,"GameState",error)||
           !owner(r,image,mode,0x150,0x13df9f0,0x17873b0,"Player",player,error)||
           !owner(r,image,state,0x180,0x13df290,0x16de6c0,"DevLounge",lounge,error)||
           !owner(r,image,state,0x1c8,0x13df500,0x1715450,"SoftwareStatus",software,error))return false;
        nlohmann::json dev,tech;int32_t item_count=0;
        if(!project(r,image,lounge,dev,error)||!technology(r,image,software,tech,error)||!inventory(r,image,player,item_count,error)||!r.stable(error))return false;
        Origin after;if(!port.current(after,error))return false;if(!same(before,after))return fail(error,"development current world origin changed during read");
        tech["inventory_count"]=item_count;tech["inventory_present"]=item_count!=0;
        result={{"schema",1},{"mainthread_confirmed",true},{"lifetime_confirmed",true},{"snapshot_uuid",before.uuid},
            {"world_epoch",before.world_epoch},{"world_generation",before.generation},{"project",std::move(dev)},{"startingpoint",std::move(tech)}};
        error.clear();return true;
    }catch(const std::exception& x){result=nullptr;return fail(error,std::string("development read exception: ")+x.what());}
}
}
