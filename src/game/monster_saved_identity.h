#pragma once
#include "npc_quest_targets.h"
#include "snapshot_monster_key.h"

namespace mt2mp::game::monster_saved_identity {
namespace read=npc_quest_targets;
using read::at;using read::typed;using read::bounds;using read::unchanged;using read::fail;using read::Bounds;
inline constexpr uintptr_t save_property_rva=0xda7a10,load_property_rva=0xda79e0;
inline constexpr uintptr_t load_children_return_rva=0xb67853,load_link_return_rva=0xb67ba5,load_inline_return_rva=0xb67d05;
inline constexpr size_t maximum_monsters=100000;
struct Record {
    uintptr_t pointer=0;uint64_t uid=0;int32_t child=-1,link_id=0;
    bool operator==(const Record&)const=default;
};
struct Pool {uintptr_t manager=0;std::vector<Record> records;bool operator==(const Pool&)const=default;};
struct SavedPool {int32_t region=-1;Pool pool;};
inline std::string key(const std::string& sha,int32_t region,const Record& record){
    return sha+":world:monster:"+(record.link_id>0?std::to_string(record.link_id):snapshot_monster_key::root_suffix(region,record.child));
}
template<class Memory>bool property(Memory& m,uintptr_t image,uintptr_t object,uintptr_t manager,std::string& e){
    uint64_t offset=0;
    if(!typed(m,image,object,0x170bf10,e)||!typed(m,image,manager,0x1714100,e)||!at(m,object,0x38,offset,e))return false;
    return (object==image+0x13bf500&&offset==0x38)||fail(e,"Monster saved property layout differs");
}
template<class Memory,class LinkId>bool observe(Memory& m,uintptr_t image,uintptr_t manager,LinkId&& links,Pool& out,std::string& e){
    Pool result;result.manager=manager;Bounds initial;
    if(!typed(m,image,manager,0x1714100,e)||!typed(m,image,manager+0x38,0x16e8b60,e)||
       !bounds(m,manager+0x38,maximum_monsters,initial,e))return false;
    std::set<uintptr_t> pointers;std::set<uint64_t> uids;std::set<int32_t> ids;
    for(int32_t n=0;n<initial.count;++n){Record row;row.child=n;
        if(!at(m,initial.data,size_t(n)*8,row.pointer,e)||!typed(m,image,row.pointer,0x16c0770,e)||
           !at(m,row.pointer,0x18,row.uid,e)||!links(row.pointer,row.link_id,e))return false;
        if(!row.uid||row.link_id<0||!pointers.insert(row.pointer).second||!uids.insert(row.uid).second||
           (row.link_id&&!ids.insert(row.link_id).second))return fail(e,"Monster saved pointer/full UID/link duplicate or invalid");
        result.records.push_back(row);
    }
    if(!unchanged(m,manager+0x38,maximum_monsters,initial,e))return false;
    out=std::move(result);e.clear();return true;
}
struct SaveFrame {uintptr_t property_object=0,manager=0,writer=0,context=0;Pool before;};
template<class Memory,class LinkId>bool begin_save(Memory& m,uintptr_t image,uintptr_t prop,uintptr_t manager,
        uintptr_t writer,uintptr_t context,LinkId&& links,SaveFrame& frame,std::string& e){
    frame={};if(!writer||!context)return fail(e,"Monster save writer/context absent");
    if(!property(m,image,prop,manager,e)||!observe(m,image,manager,links,frame.before,e))return false;
    frame.property_object=prop;frame.manager=manager;frame.writer=writer;frame.context=context;return true;
}
template<class Memory,class LinkId>bool finish_save(Memory& m,uintptr_t image,const SaveFrame& frame,uintptr_t prop,
        uintptr_t manager,uintptr_t writer,uintptr_t context,bool result,LinkId&& links,Pool& out,std::string& e){
    if(!result||prop!=frame.property_object||manager!=frame.manager||writer!=frame.writer||context!=frame.context)
        return fail(e,"Monster original save result/receivers differ");
    if(!property(m,image,prop,manager,e)||!observe(m,image,manager,links,out,e))return false;
    return out==frame.before||fail(e,"Monster snapshot order/identity/link changed during SaveStream");
}
struct ChildHeader {std::string label;int32_t count=-1;};
template<class Memory>bool child_header(Memory& m,uintptr_t reader,ChildHeader& out,std::string& e){
    out={};uint8_t valid=0,binary=0;int32_t token=-1;uintptr_t data=0;uint64_t size=0;
    if(!at(m,reader,0x18,valid,e)||!at(m,reader,0x90,binary,e)||!at(m,reader,0x8c,out.count,e)||
       !at(m,reader,0x20,token,e)||!at(m,reader,0x28,data,e)||!at(m,reader,0x30,size,e))return false;
    if(valid!=1||binary!=1||token!=1||out.count<0||!data||size!=7||data>UINTPTR_MAX-size)
        return fail(e,"Monster original child header invalid");
    out.label.resize(size_t(size));if(!m.read(data,out.label.data(),out.label.size(),e))return false;
    return out.label=="monster"||fail(e,"Monster original child label differs");
}
struct LoadFrame {
    uintptr_t manager=0,reader=0,context=0;int32_t declared=-1;
    std::vector<std::pair<uintptr_t,int32_t>> registrations;
    bool children(uintptr_t caller,uintptr_t actual_reader,const std::string& label,int32_t before_count,int32_t count,std::string& e){
        if(caller!=load_children_return_rva||actual_reader!=reader||label!="monster"||declared!=-1||
           count!=before_count||count<0||size_t(count)>maximum_monsters)return fail(e,"Monster original child count/reader/label differs");
        declared=count;return true;
    }
    bool registered(uintptr_t caller,uintptr_t ctx,int32_t id,uintptr_t object,std::string& e){
        const bool linked=caller==load_link_return_rva;
        if((!linked&&caller!=load_inline_return_rva)||ctx!=context||!object||declared<0||
           (linked?id<=0:id!=0)||registrations.size()>=size_t(declared))return fail(e,"Monster original registration receiver/link/count differs");
        for(const auto& old:registrations)if(old.first==object||(id&&old.second==id))return fail(e,"Monster original registration duplicate");
        registrations.emplace_back(object,id);return true;
    }
    template<class Memory>bool finish(Memory& m,uintptr_t image,Pool& out,std::string& e)const{
        if(declared<0||registrations.size()!=size_t(declared))return fail(e,"Monster original load skipped children/registration");
        auto links=[&](uintptr_t object,int32_t& id,std::string& cause){for(const auto& row:registrations)if(row.first==object){id=row.second;return true;}
            return fail(cause,"Monster array member lacks original registration");};
        if(!observe(m,image,manager,links,out,e))return false;
        if(out.records.size()!=registrations.size())return fail(e,"Monster loaded array has extra or missing members");
        for(size_t i=0;i<registrations.size();++i)if(out.records[i].pointer!=registrations[i].first)
            return fail(e,"Monster loaded array differs from original record order");
        return true;
    }
};
template<class Memory>bool begin_load(Memory& m,uintptr_t image,uintptr_t prop,uintptr_t manager,
        uintptr_t reader,uintptr_t context,LoadFrame& frame,std::string& e){
    frame={};if(!reader||!context)return fail(e,"Monster load reader/context absent");
    Bounds initial;if(!property(m,image,prop,manager,e)||!typed(m,image,manager+0x38,0x16e8b60,e)||
       !bounds(m,manager+0x38,maximum_monsters,initial,e))return false;
    if(initial.count)return fail(e,"Monster native load requires initially empty array");
    frame.manager=manager;frame.reader=reader;frame.context=context;return true;
}
template<class Memory>bool registered(Memory& m,uintptr_t image,LoadFrame& frame,uintptr_t caller,
        uintptr_t context,int32_t id,uintptr_t object,std::string& e){
    uintptr_t actual=0;if(!typed(m,image,object,0x16c0770,e)||!at(m,object,8,actual,e))return false;
    if(actual!=context)return fail(e,"Monster registered object context differs");
    return frame.registered(caller,context,id,object,e);
}
}
