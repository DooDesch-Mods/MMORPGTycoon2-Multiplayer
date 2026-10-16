#include "tutorial_development.h"
#include "initial_release/startingpoint_read.h"
#include "tutorial_replication/admission_read.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <set>
#include <map>
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
class Reader final:public initial_release::Memory {
    Port& port_;std::vector<Witness> reads_;size_t count_=0;
public:
    explicit Reader(Port& port):port_(port){}
    bool copy(uintptr_t p,void* out,size_t count,std::string& error)override{return bytes(p,out,count,error);}
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
bool technology(Reader& r,uintptr_t image,uintptr_t software,uintptr_t& tech,nlohmann::json& out,std::string& error){
    tech=0;uintptr_t tree=0,record=0,linked=0;if(!r.at(image,0x13df250,tree,error)||!r.type(tree,image+0x16dc400,"TechTree",error))return false;
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
bool source(Reader& r,uintptr_t image,uintptr_t address,ProjectSource& out,std::string& error){
    out.address=address;
    if(!r.type(address,image+0x16e48c0,"catalogue DevProject",error)||!r.at(address,0x18,out.kind,error)||
       !r.string(address,0x20,out.key,error)||!r.string(address,0x40,out.art,error)||!r.at(address,0x60,out.statistic,error))return false;
    return out.kind>=0&&out.kind<=3||fail(error,"development catalogue project kind outside0..3");
}
bool research_rows(Reader& r,Port& port,uintptr_t image,uintptr_t state,uintptr_t lounge,uintptr_t software,nlohmann::json& out,std::string& error){
    uintptr_t tree=0,business=0,cash=0,current=0;int32_t major=0,balance=0;uint8_t sandbox=0;
    if(!r.at(image,0x13df250,tree,error)||!r.type(tree,image+0x16dc400,"research TechTree",error)||
       !owner(r,image,state,0x1c0,0x13df220,0x16d97e0,"research Business",business,error)||
       !r.at(business,0xf0,sandbox,error)||!r.at(business,0x100,cash,error)||!r.at(cash,0x20,balance,error)||
       !r.at(software,0x88,major,error)||!r.at(lounge,0xa8,current,error))return false;
    if(sandbox>1||major<0)return fail(error,"development research version/sandbox flag invalid");
    std::vector<uintptr_t> projects,techs,records;
    if(!r.array(lounge,0xd8,projects,error)||!r.array(tree,0x10,techs,error)||!r.array(software,0x280,records,error))return false;
    std::map<std::string,uintptr_t> by_key,by_record;
    for(auto address:techs){std::string key;if(!r.string(address,0x20,key,error))return false;
        if(key.empty()||!by_key.emplace(key,address).second)return fail(error,"development research Tech key empty/duplicated");}
    for(auto address:records){std::string key;if(!r.type(address,image+0x16e79d0,"research TechRecord",error)||!r.string(address,0x38,key,error))return false;
        if(key.empty()||!by_record.emplace(key,address).second)return fail(error,"development research record key empty/duplicated");}
    nlohmann::json rows=nlohmann::json::array();std::set<std::string> seen;
    for(auto address:projects){ProjectSource entry;if(!source(r,image,address,entry,error))return false;
        if(entry.kind!=0)continue;
        if(!seen.insert(entry.key).second)return fail(error,"development research project key duplicated");
        auto ti=by_key.find(entry.key),ri=by_record.find(entry.key);
        if(ti==by_key.end()||ri==by_record.end())return fail(error,"development research project has no owned Tech/record");
        const auto tech=ti->second,record=ri->second;uintptr_t linked=0,back=0,prerequisite=0;int32_t required=0,cost=0;
        std::array<uint8_t,3> flags{};
        if(!r.at(tech,0x1e8,linked,error)||!r.at(record,0x60,back,error)||!r.bytes(record+0x58,flags.data(),flags.size(),error)||
           !r.at(tech,0x1e0,prerequisite,error)||!r.at(tech,0xa8,required,error)||!r.at(tech,0xac,cost,error))return false;
        if(linked!=record||back!=tech)return fail(error,"development research Tech/record reciprocal owner differs");
        for(auto flag:flags)if(flag>1)return fail(error,"development research record flag invalid");
        if(required<0||cost<0)return fail(error,"development research Tech limits invalid");
        std::string prerequisite_key;
        if(prerequisite){
            if(std::find(techs.begin(),techs.end(),prerequisite)==techs.end())return fail(error,"development research prerequisite outside owned TechTree");
            uintptr_t prerecord=0,preback=0;std::array<uint8_t,3> preflags{};
            if(!r.string(prerequisite,0x20,prerequisite_key,error)||!r.at(prerequisite,0x1e8,prerecord,error))return false;
            auto pi=by_record.find(prerequisite_key);
            if(pi==by_record.end()||pi->second!=prerecord)return fail(error,"development research prerequisite record outside SoftwareStatus");
            if(!r.at(prerecord,0x60,preback,error)||!r.bytes(prerecord+0x58,preflags.data(),preflags.size(),error))return false;
            if(preback!=prerequisite)return fail(error,"development research prerequisite reciprocal owner differs");
            for(auto flag:preflags)if(flag>1)return fail(error,"development research prerequisite flag invalid");
        }
        std::array<bool,4> native{};
        if(!port.research_flags(image,address,native,error))return false;
        rows.push_back({{"key",entry.key},{"kind",0},{"purchased",flags[0]!=0},{"deployed",flags[1]!=0},{"custom",flags[2]!=0},
            {"visible",native[0]},{"owned",native[1]},{"can_choose_now",native[2]},{"can_afford",native[3]},
            {"cost",cost},{"required_major",required},{"prerequisite_key",prerequisite_key}});
    }
    out={{"kind",0},{"project_present",current!=0},{"major_version",major},{"cash",balance},{"sandbox",sandbox!=0},{"rows",std::move(rows)}};
    return true;
}
bool creation_owners(Reader& r,StartingpointSnapshot& snapshot,std::string& error){
    if(!snapshot.record)return fail(error,"startingpoint creation native TechRecord absent from owned SoftwareStatus");
    CreationOwners out;const auto lounge=snapshot.lounge,image=snapshot.image;
    std::vector<uintptr_t> catalogue,developers;
    if(!r.array(lounge,0xd8,catalogue,error)||!r.at(lounge,0xd8,out.catalogue_data,error)||!r.at(lounge,0xe4,out.catalogue_capacity,error)||
       !r.array(lounge,0x70,developers,error)||!r.at(lounge,0x70,out.developer_data,error)||!r.at(lounge,0x7c,out.developer_capacity,error)||
       !r.at(lounge,0x57,out.automatic_workers,error)||!r.at(lounge,0x310,out.total_workers,error)||!r.at(snapshot.tech,0xb4,out.max_workers,error))return false;
    if(!owner(r,image,reinterpret_cast<uintptr_t>(snapshot.origin.state),0x190,0x13df4f0,0x1714ed0,"ReleaseManager",out.release_manager,error)||
       !r.at(out.release_manager,0x38,out.release_owner,error)||!r.type(out.release_owner,image+0x16c12b0,"Release",error)||
       !r.array(out.release_owner,0x150,out.release_changes,error)||!r.at(out.release_owner,0x150,out.release_data,error)||
       !r.at(out.release_owner,0x15c,out.release_capacity,error))return false;
    if(out.automatic_workers>1||(out.automatic_workers&&(out.total_workers<0||out.max_workers<0)))
        return fail(error,"startingpoint creation native automatic worker policy invalid");
    for(auto address:catalogue){ProjectSource entry;if(!source(r,image,address,entry,error))return false;
        if(entry.kind==0&&entry.key=="startingpoint"){
            if(out.source)return fail(error,"startingpoint creation catalogue key duplicated");out.source=address;
        }
        out.catalogue.push_back(std::move(entry));
    }
    if(!out.source)return fail(error,"startingpoint creation catalogue project absent");
    for(auto address:developers){DeveloperState entry;entry.address=address;
        if(!r.type(address,image+0x17422e0,"Dev",error)||!r.at(address,0x4c4,entry.job,error))return false;
        out.developers.push_back(entry);
    }
    if(snapshot.project){
        if(std::find(catalogue.begin(),catalogue.end(),snapshot.project)!=catalogue.end())return fail(error,"startingpoint current project aliases owned catalogue member");
        ProjectSource current;if(!source(r,image,snapshot.project,current,error))return false;out.current=std::move(current);
    }
    snapshot.creation=std::move(out);return true;
}
}
bool observe(Port& port,nlohmann::json& result,std::string& error,StartingpointSnapshot* snapshot,bool creation,bool research){
    result=nullptr;if(snapshot)*snapshot={};
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
        nlohmann::json dev,tech;int32_t item_count=0;uintptr_t startingpoint=0;
        initial_release::StartingpointRelease release;
        if(!project(r,image,lounge,dev,error)||!technology(r,image,software,startingpoint,tech,error)||
           !inventory(r,image,player,item_count,error)||!initial_release::read_startingpoint_release(r,image,state,startingpoint,release,error))return false;
        if(snapshot){
            snapshot->origin=before;snapshot->image=image;snapshot->lounge=lounge;snapshot->tech=startingpoint;snapshot->player=player;
            if(!r.at(lounge,0xa8,snapshot->project,error)||!r.at(startingpoint,0x1e8,snapshot->record,error)||
               !r.at(player,0x1d0,snapshot->inventory,error)||!r.at(snapshot->inventory,8,snapshot->slots,error)||
               !r.bytes(snapshot->slots,snapshot->items.data(),sizeof(snapshot->items),error))return false;
            const bool fixed_project=dev.at("present")==true&&dev.at("kind")==0&&dev.at("tech_key")=="startingpoint";
            const bool retired=!fixed_project&&tech.at("deployed")==true;
            const bool baseline=!snapshot->project&&tech.at("deployed")==false&&tech.at("custom")==false;
            if(tech.at("record_present")==true&&(fixed_project||retired||baseline)){
                tutorial_replication::startingpoint::Row row;
                row.project_present=fixed_project;
                // This row describes only the current Startingpoint project.
                // Later unrelated projects remain in the ordinary diagnostic.
                if(fixed_project||baseline){row.work=dev.at("work").get<float>();row.lines=dev.at("lines").get<int32_t>();
                    row.desired_workers=dev.at("desired_workers").get<int32_t>();row.bugs_total=dev.at("bugs_total").get<int32_t>();row.bugs_known=dev.at("bugs_known").get<int32_t>();}
                row.purchased=tech.at("purchased").get<bool>();row.deployed=tech.at("deployed").get<bool>();row.custom=tech.at("custom").get<bool>();
                row.inventory_count=item_count;row.release_change_count=release.change_count;row.release_feature_count=release.feature_count;row.release_counter=release.mutation_counter;
                snapshot->row=row;
            }
            if(creation&&!creation_owners(r,*snapshot,error))return false;
            startingpoint_adoption::Admission admission;
            snapshot->admitted=startingpoint_adoption::read_admission(r,image,state,startingpoint,admission,snapshot->admission_cause);
            snapshot->release=admission.release;
            nlohmann::json listeners=nlohmann::json::array();
            auto rva=[&](uintptr_t address)->nlohmann::json{return address>=image&&address-image<0x1b00000?nlohmann::json(address-image):nlohmann::json(nullptr);};
            for(const auto& listener:admission.listeners)listeners.push_back({{"class_rva",rva(listener.address_point)},{"handler_rva",rva(listener.handler)}});
            snapshot->admission={{"accepted",snapshot->admitted},{"cause",snapshot->admission_cause},{"event38_listener_count",admission.listeners.size()},
                {"events_enabled",admission.events_enabled},{"repeatable",admission.repeatable},{"modifiers",admission.modifiers},
                {"art_pack_bytes",admission.art_pack_bytes},{"item_feature",admission.item_feature},{"listeners",listeners}};
        }
        nlohmann::json research_result;
        if(research&&!research_rows(r,port,image,state,lounge,software,research_result,error))return false;
        if(!r.stable(error))return false;
        Origin after;if(!port.current(after,error))return false;if(!same(before,after))return fail(error,"development current world origin changed during read");
        tech["inventory_count"]=item_count;tech["inventory_present"]=item_count!=0;
        result={{"schema",1},{"mainthread_confirmed",true},{"lifetime_confirmed",true},{"snapshot_uuid",before.uuid},
            {"world_epoch",before.world_epoch},{"world_generation",before.generation},{"project",std::move(dev)},{"startingpoint",std::move(tech)},
            {"startingpoint_release",{{"change_count",release.change_count},{"mutation_counter",release.mutation_counter},
                {"feature_count",release.feature_count},{"feature_present",release.feature_count!=0}}}};
        if(snapshot)result["startingpoint_adoption"]=snapshot->admission;
        if(research)result["research"]=std::move(research_result);
        error.clear();return true;
    }catch(const std::exception& x){result=nullptr;return fail(error,std::string("development read exception: ")+x.what());}
}
}
