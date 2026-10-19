#pragma once
#include "npc_quest_targets.h"
#include "snapshot_district_key.h"

namespace mt2mp::game::district_saved_identity {
namespace read=npc_quest_targets;
using read::at;using read::typed;using read::bounds;using read::unchanged;using read::fail;using read::Bounds;
inline constexpr uintptr_t save_property_rva=0xddcee0,load_property_rva=0xddc9c0;
inline constexpr uintptr_t load_children_return_rva=0xddca04;
inline constexpr uintptr_t load_link_return_rva=0xddcd5a,load_inline_return_rva=0xddce18;
inline constexpr size_t maximum_districts=100000;
struct Record {
    uintptr_t pointer=0;uint64_t uid=0;int32_t child=-1,link_id=0;
    bool operator==(const Record&)const=default;
};
struct Pool {uintptr_t manager=0;std::vector<Record> records;bool operator==(const Pool&)const=default;};
struct SavedPool {int32_t region=-1;Pool pool;};
struct Current {std::string key;int32_t saved_region=-1;Record row;};
inline std::string key(const std::string& sha,int32_t region,const Record& record){
    return sha+":world:district:"+(record.link_id>0?std::to_string(record.link_id):snapshot_district_key::root_suffix(region,record.child));
}
template<class Memory>
bool property(Memory& memory,uintptr_t image,uintptr_t object,uintptr_t manager,std::string& error){
    uint64_t offset=0;
    if(!typed(memory,image,object,0x1710190,error)||!typed(memory,image,manager,0x1719080,error)||
       !at(memory,object,0x38,offset,error))return false;
    return (object==image+0x13bef60&&offset==0x18)||fail(error,"District saved property layout differs");
}
template<class Memory,class LinkId>
bool observe(Memory& memory,uintptr_t image,uintptr_t manager,LinkId&& links,Pool& out,std::string& error){
    Pool result;result.manager=manager;Bounds initial;
    if(!typed(memory,image,manager,0x1719080,error)||!typed(memory,image,manager+0x18,0x172d8e0,error)||
       !bounds(memory,manager+0x18,maximum_districts,initial,error))return false;
    std::set<uintptr_t> pointers;std::set<uint64_t> uids;std::set<int32_t> ids;
    for(int32_t n=0;n<initial.count;++n){
        Record row;row.child=n;
        if(!at(memory,initial.data,size_t(n)*8,row.pointer,error)||!typed(memory,image,row.pointer,0x16da000,error)||
           !at(memory,row.pointer,0x18,row.uid,error)||!links(row.pointer,row.link_id,error))return false;
        if(!row.uid||row.link_id<0||!pointers.insert(row.pointer).second||!uids.insert(row.uid).second||
           (row.link_id&&!ids.insert(row.link_id).second))return fail(error,"District saved pointer/full UID/link duplicate or invalid");
        result.records.push_back(row);
    }
    if(!unchanged(memory,manager+0x18,maximum_districts,initial,error))return false;
    out=std::move(result);error.clear();return true;
}
struct SaveFrame {uintptr_t property_object=0,manager=0,writer=0,context=0;Pool before;};
template<class Memory,class LinkId>
bool begin_save(Memory& memory,uintptr_t image,uintptr_t prop,uintptr_t manager,uintptr_t writer,uintptr_t context,
                LinkId&& links,SaveFrame& frame,std::string& error){
    frame={};if(!writer||!context)return fail(error,"District save writer/context absent");
    if(!property(memory,image,prop,manager,error)||!observe(memory,image,manager,links,frame.before,error))return false;
    frame.property_object=prop;frame.manager=manager;frame.writer=writer;frame.context=context;return true;
}
template<class Memory,class LinkId>
bool finish_save(Memory& memory,uintptr_t image,const SaveFrame& frame,uintptr_t prop,uintptr_t manager,
                 uintptr_t writer,uintptr_t context,bool result,LinkId&& links,Pool& out,std::string& error){
    if(!result||prop!=frame.property_object||manager!=frame.manager||writer!=frame.writer||context!=frame.context)
        return fail(error,"District original save result/receivers differ");
    if(!property(memory,image,prop,manager,error)||!observe(memory,image,manager,links,out,error))return false;
    return out==frame.before||fail(error,"District order/full identity/link changed during SaveStream");
}
struct ChildHeader {std::string label;int32_t count=-1;};
template<class Memory>
bool child_header(Memory& memory,uintptr_t reader,ChildHeader& out,std::string& error){
    out={};uint8_t valid=0,binary=0;int32_t token=-1;uintptr_t data=0;uint64_t size=0;
    if(!at(memory,reader,0x18,valid,error)||!at(memory,reader,0x90,binary,error)||!at(memory,reader,0x8c,out.count,error)||
       !at(memory,reader,0x20,token,error)||!at(memory,reader,0x28,data,error)||!at(memory,reader,0x30,size,error))return false;
    if(valid!=1||binary!=1||token!=1||out.count<0||size!=8||!data||data>UINTPTR_MAX-size)
        return fail(error,"District original child header invalid");
    out.label.resize(size_t(size));if(!memory.read(data,out.label.data(),out.label.size(),error))return false;
    return out.label=="district"||fail(error,"District original child label differs");
}
struct LoadFrame {
    uintptr_t manager=0,reader=0,context=0;int32_t declared=-1;std::vector<std::pair<uintptr_t,int32_t>> registrations;
    bool children(uintptr_t caller,uintptr_t actual_reader,const std::string& label,int32_t before_count,int32_t count,std::string& error){
        if(caller!=load_children_return_rva||actual_reader!=reader||label!="district"||declared!=-1||count!=before_count||
           count<0||size_t(count)>maximum_districts)return fail(error,"District original child count/label/reader differs");
        declared=count;return true;
    }
    bool registered(uintptr_t caller,uintptr_t ctx,int32_t id,uintptr_t object,std::string& error){
        const bool linked=caller==load_link_return_rva;
        if((!linked&&caller!=load_inline_return_rva)||ctx!=context||!object||declared<0||
           (linked?id<=0:id!=0)||registrations.size()>=size_t(declared))return fail(error,"District original registration receiver/link/count differs");
        for(const auto& old:registrations)if(old.first==object||(id&&old.second==id))return fail(error,"District original registration duplicate");
        registrations.emplace_back(object,id);return true;
    }
    template<class Memory>
    bool finish(Memory& memory,uintptr_t image,Pool& out,std::string& error)const{
        if(declared<0||registrations.size()!=size_t(declared))return fail(error,"District original load skipped child/registration");
        auto links=[&](uintptr_t object,int32_t& id,std::string& cause){
            for(const auto& entry:registrations)if(entry.first==object){id=entry.second;return true;}
            return fail(cause,"District array member lacks original registration");
        };
        if(!observe(memory,image,manager,links,out,error))return false;
        if(out.records.size()!=registrations.size())return fail(error,"District loaded array has extra/missing members");
        for(size_t i=0;i<registrations.size();++i)if(out.records[i].pointer!=registrations[i].first)
            return fail(error,"District loaded array differs from original record order");
        return true;
    }
};
template<class Memory>
bool begin_load(Memory& memory,uintptr_t image,uintptr_t prop,uintptr_t manager,uintptr_t reader,uintptr_t context,
                LoadFrame& frame,std::string& error){
    frame={};if(!reader||!context)return fail(error,"District load reader/context absent");
    Bounds initial;
    if(!property(memory,image,prop,manager,error)||!typed(memory,image,manager+0x18,0x172d8e0,error)||
       !bounds(memory,manager+0x18,maximum_districts,initial,error))return false;
    if(initial.count)return fail(error,"District native load requires initially empty array");
    frame.manager=manager;frame.reader=reader;frame.context=context;return true;
}
template<class Memory>
bool registered(Memory& memory,uintptr_t image,LoadFrame& frame,uintptr_t caller,uintptr_t context,
                int32_t id,uintptr_t object,std::string& error){
    uintptr_t actual_context=0;
    if(!typed(memory,image,object,0x16da000,error)||!at(memory,object,8,actual_context,error))return false;
    if(context!=actual_context)return fail(error,"District registered object context differs");
    return frame.registered(caller,context,id,object,error);
}
// Resolve the retained full identity against current members, never a mutable slot.
template<class Lookup>
bool current_rows(const std::string& sha,const SavedPool& saved,const Pool& current,Lookup&& lookup,
                  std::vector<Current>& out,std::string& error){
    if(current.manager!=saved.pool.manager)return fail(error,"District current owner manager differs from snapshot");
    std::vector<Current> result;
    for(const auto& old:saved.pool.records)for(const auto& row:current.records){
        if(row.pointer!=old.pointer&&row.uid!=old.uid)continue;
        if(row.pointer!=old.pointer||row.uid!=old.uid)return fail(error,"District saved pointer/full UID was replaced");
        uintptr_t found=0;if(!lookup(old.uid,found,error))return false;
        if(found!=row.pointer)return fail(error,"District current full UID lookup differs from manager member");
        auto canonical=key(sha,saved.region,old);
        if(!snapshot_district_key::valid(canonical))return fail(error,"District snapshot key invalid");
        result.push_back({std::move(canonical),saved.region,row});
    }
    out=std::move(result);error.clear();return true;
}
}
