#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace mt2mp::game::character_definition_identity {
struct Array {uintptr_t object=0,data=0;int32_t count=0,capacity=0;std::vector<uintptr_t> members;};
struct Definition {
    std::string key;int32_t kind=-1,id=-1,variant_slot=-1;
    uintptr_t object=0,parent=0,owning_collection=0,variants=0,record=0;
    uint64_t uid=0,parent_uid=0;bool deleted=false,edited=false,released=false;
};
struct Collection {uintptr_t object=0,owner=0;std::array<Array,5> arrays;std::vector<std::array<int32_t,2>> ids;};
struct Census {uintptr_t state=0,manager=0;std::array<Array,5> arrays;std::vector<Definition> definitions;std::vector<Collection> collections;};
inline bool fail(std::string& error,const char* cause){error=cause;return false;}
inline std::string key(int32_t kind,int32_t id){return "character:"+std::to_string(kind)+":"+std::to_string(id);}
template<class Read>bool array(Read& at,uintptr_t object,Array& out,std::string& error){
    out={};out.object=object;
    if(!at(object,8,out.data,error)||!at(object,0x10,out.count,error)||!at(object,0x14,out.capacity,error))return false;
    if(out.count<0||out.capacity<out.count||(out.count&&(!out.data||size_t(out.count)>(UINTPTR_MAX-out.data)/8)))return fail(error,"Character definition array count/capacity/span differs");
    for(int32_t i=0;i<out.count;++i){uintptr_t member=0;if(!at(out.data,size_t(i)*8,member,error))return false;if(!member)return fail(error,"Character definition array contains a null member");out.members.push_back(member);}return true;
}
template<class Read>bool unchanged(Read& at,const Array& before,std::string& error){Array after;if(!array(at,before.object,after,error))return false;
    return (after.data==before.data&&after.count==before.count&&after.capacity==before.capacity&&after.members==before.members)||fail(error,"Character definition current array/header/member order changed");}

// The caller supplies a current NativeOpen state and pinned image/Mainthread.
// IDs are original global CharacterTypeIDs. Variant collection order is a local
// witness because original DestroySlot shifts those records with memmove.
template<class Read>bool read(Read& at,uintptr_t image,uintptr_t native_state,uintptr_t manager,Census& out,std::string& error){
    out={};Census next;next.state=native_state;next.manager=manager;uintptr_t actual=0,owned=0,ap=0;
    if(!at(image,0x13df7a0,actual,error)||!at(native_state,0x168,owned,error)||!at(manager,0,ap,error))return false;
    if(!manager||actual!=manager||owned!=manager||ap!=image+0x1732430)return fail(error,"Character definition actual State/singleton/manager class differs");
    std::map<uintptr_t,size_t> pointers;std::set<uint64_t> uids;
    for(int32_t kind=0;kind<5;++kind){auto& span=next.arrays[size_t(kind)];if(!array(at,manager+0x80+size_t(kind)*0x50,span,error))return false;
        for(int32_t id=0;id<span.count;++id){Definition item;item.object=span.members[size_t(id)];item.kind=kind;item.id=id;item.key=key(kind,id);
            int32_t actual_kind=-1,actual_id=-1;uint8_t deleted=2,edited=2,released=2;
            if(!at(item.object,0,ap,error)||!at(item.object,0x18,item.uid,error)||!at(item.object,0x2c,actual_kind,error)||!at(item.object,0x30,actual_id,error)||
               !at(item.object,0x498,item.owning_collection,error)||!at(item.object,0x4a0,item.parent,error)||!at(item.object,0x2c8,item.variants,error)||
               !at(item.object,0x490,deleted,error)||!at(item.object,0x491,edited,error)||!at(item.object,0x492,released,error))return false;
            if(ap!=image+0x1705990||(!item.uid&&!deleted)||actual_kind!=kind||actual_id!=id||deleted>1||edited>1||released>1||
               !pointers.emplace(item.object,next.definitions.size()).second||(item.uid&&!uids.insert(item.uid).second))return fail(error,"Character definition global type/AP/full UID/ID/flags duplicated or foreign");
            item.deleted=deleted!=0;item.edited=edited!=0;item.released=released!=0;next.definitions.push_back(std::move(item));
        }
    }
    std::set<uintptr_t> collections,records;std::set<size_t> matched;
    for(const auto& base:next.definitions){if(base.parent||!base.variants)continue;Collection collection;collection.object=base.variants;collection.owner=base.object;
        if(!collections.insert(collection.object).second)return fail(error,"Character variants collection is shared by multiple base definitions");
        if(!at(collection.object,0,ap,error))return false;
        if(ap!=image+0x173a6f0)return fail(error,"Character variants private collection class differs");
        for(int32_t kind=0;kind<5;++kind){auto& span=collection.arrays[size_t(kind)];if(!at(collection.object,0x40+size_t(kind)*0x30,ap,error)||!array(at,collection.object+0x40+size_t(kind)*0x30,span,error))return false;
            if(ap!=image+0x16eef80)return fail(error,"Character variants native object-array class differs");
            for(int32_t slot=0;slot<span.count;++slot){const auto record=span.members[size_t(slot)];int32_t type_kind=-1,type_id=-1;
                if(!at(record,0,ap,error)||!at(record,0x18,type_kind,error)||!at(record,0x1c,type_id,error))return false;
                if(ap!=image+0x1740fa0||type_kind!=kind||kind!=base.kind||type_id<0||type_id>=next.arrays[size_t(kind)].count||!records.insert(record).second)return fail(error,"Character variants original record class/kind/ID/membership differs");
                const auto object=next.arrays[size_t(kind)].members[size_t(type_id)];auto& variant=next.definitions[pointers.at(object)];
                if(variant.parent!=base.object||variant.owning_collection!=collection.object||variant.variants||!matched.insert(pointers.at(object)).second)return fail(error,"Character variant parent/private collection/global membership is not reciprocal");
                variant.parent_uid=base.uid;variant.variant_slot=slot;variant.record=record;collection.ids.push_back({type_kind,type_id});
            }
        }next.collections.push_back(std::move(collection));
    }
    for(size_t index=0;index<next.definitions.size();++index){auto& item=next.definitions[index];
        // AddCharacterType can destroy and reuse a deleted global slot. Other
        // deleted variants retain opaque parent/Collection addresses from that
        // former lifetime; neither address names a current owner after reuse.
        if(item.deleted&&!item.uid){if(item.variants)return fail(error,"Destroyed Character tombstone retains a private variant Collection");continue;}
        if(!item.parent)continue;
        const auto found=pointers.find(item.parent);if(found==pointers.end())return fail(error,"Character variant parent is absent from actual global manager");const auto& base=next.definitions[found->second];
        if(base.parent||base.kind!=item.kind||item.variants||item.parent==item.object)
            return fail(error,"Character variant parent/private Collection is nested, self or foreign");
        // Destroy clears the UID and releases a base's whole private Collection.
        // Retained global tombstones keep their parent and former Collection
        // pointer; that freed Collection must never be dereferenced.
        if(base.deleted&&!base.variants){if(!item.deleted)return fail(error,"Live Character variant has a destroyed base/private Collection");item.parent_uid=base.uid;continue;}
        if(!base.variants||item.owning_collection!=base.variants)return fail(error,"Character variant retained private Collection differs from its live base");
        if(!item.deleted&&!matched.contains(index))return fail(error,"Live Character variant has no exact owning Collection record");item.parent_uid=base.uid;
    }
    for(const auto& span:next.arrays)if(!unchanged(at,span,error))return false;
    for(const auto& collection:next.collections){size_t index=0;for(const auto& span:collection.arrays){if(!unchanged(at,span,error))return false;
        for(const auto record:span.members){int32_t kind=-1,id=-1;if(!at(record,0x18,kind,error)||!at(record,0x1c,id,error))return false;
            if(collection.ids[index++]!=std::array<int32_t,2>{kind,id})return fail(error,"Character variants original Collection record changed during read");}}}
    for(const auto& item:next.definitions){uint64_t uid=0;uintptr_t parent=0,collection=0,variants=0;int32_t kind=-1,id=-1;uint8_t deleted=2,edited=2,released=2;
        if(!at(item.object,0x18,uid,error)||!at(item.object,0x2c,kind,error)||!at(item.object,0x30,id,error)||!at(item.object,0x4a0,parent,error)||!at(item.object,0x498,collection,error)||!at(item.object,0x2c8,variants,error)||
           !at(item.object,0x490,deleted,error)||!at(item.object,0x491,edited,error)||!at(item.object,0x492,released,error))return false;
        if(uid!=item.uid||kind!=item.kind||id!=item.id||parent!=item.parent||collection!=item.owning_collection||variants!=item.variants||deleted!=uint8_t(item.deleted)||edited!=uint8_t(item.edited)||released!=uint8_t(item.released))return fail(error,"Character definition full UID/owner/ID/flags changed during census");}
    if(!at(image,0x13df7a0,actual,error)||!at(native_state,0x168,owned,error))return false;
    if(actual!=manager||owned!=manager)return fail(error,"Character definition State/singleton manager changed during census");out=std::move(next);error.clear();return true;
}
inline bool find(const Census& census,uintptr_t object,uint64_t uid,const Definition*& out,std::string& error){out=nullptr;
    for(const auto& row:census.definitions)if(row.object==object){if(row.uid!=uid)return fail(error,"Character definition current full UID differs");out=&row;return true;}
    return fail(error,"Character definition is absent from current complete global pool");
}
}
