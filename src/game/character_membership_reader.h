#pragma once
#include "character_definition_identity.h"
#include <algorithm>
namespace mt2mp::game::character_membership_reader {
namespace definitions=character_definition_identity;
enum class Family:uint8_t {Boss,Monster,Npc,Toon};
struct User {uintptr_t object=0,addresspoint=0;uint64_t uid=0;Family family=Family::Boss;std::string definition;bool operator==(const User&)const=default;};
struct Users {std::vector<User> rows;std::map<std::string,std::vector<User>> by_definition;bool operator==(const Users&)const=default;};
inline constexpr std::array<uintptr_t,4> callbacks{0x3976f0,0x3b9e20,0x3c3170,0x3e9e00};
template<class Read,class Find>
bool users(Read& at,Find& find,uintptr_t image,const definitions::Census& census,Users& out,std::string& error){
    out={};Users next;
    for(const auto& definition:census.definitions){
        if(definition.deleted)continue;uintptr_t ap=0;definitions::Array array;
        if(!at(definition.object,0x4c0,ap,error))return false;
        if(ap!=image+0x1747ae0)return definitions::fail(error,"Character current complete native users array class differs");
        if(!definitions::array(at,definition.object+0x4c0,array,error))return false;
        std::set<uintptr_t> pointers;std::set<uint64_t> uids;auto& rows=next.by_definition[definition.key];
        for(const auto pointer:array.members){User row;uintptr_t callback=0,registered=0;row.object=pointer;row.definition=definition.key;
            if(!pointers.insert(pointer).second)return definitions::fail(error,"Character current native users array repeats a process pointer");
            if(!at(pointer,0,row.addresspoint,error)||!at(pointer,0x18,row.uid,error)||!at(row.addresspoint,0x438,callback,error)||!find(row.uid,registered,error))return false;
            if(!row.uid||!uids.insert(row.uid).second||registered!=pointer)return definitions::fail(error,"Character current user is repeated or lacks its full native UID registration");
            const auto kind=std::find(callbacks.begin(),callbacks.end(),callback-image);
            if(kind==callbacks.end())return definitions::fail(error,"Character current user has an unknown native destruction callback");
            row.family=Family(kind-callbacks.begin());rows.push_back(row);next.rows.push_back(std::move(row));
        }
        if(!definitions::unchanged(at,array,error))return false;
    }
    out=std::move(next);error.clear();return true;
}
template<class Read>
bool collection_member(Read& at,uintptr_t image,const definitions::Definition& selected,uintptr_t& record,std::string& error){
    record=0;uintptr_t ap=0;definitions::Array rows;
    if(selected.kind<0||selected.kind>4||!selected.owning_collection||selected.deleted)return definitions::fail(error,"Character destruction lacks a live owning Collection/kind");
    if(!at(selected.owning_collection,0,ap,error))return false;
    if(ap!=image+0x173a6f0)return definitions::fail(error,"Character destruction owning Collection class differs");
    if(!at(selected.owning_collection,0x40+size_t(selected.kind)*0x30,ap,error))return false;
    if(ap!=image+0x16eef80)return definitions::fail(error,"Character destruction owning Collection array class differs");
    if(!definitions::array(at,selected.owning_collection+0x40+size_t(selected.kind)*0x30,rows,error))return false;
    for(const auto pointer:rows.members){int32_t kind=-1,id=-1;
        if(!at(pointer,0,ap,error)||!at(pointer,0x18,kind,error)||!at(pointer,0x1c,id,error))return false;
        if(ap!=image+0x1740fa0)return definitions::fail(error,"Character owning Collection member Record class differs");
        if(kind==selected.kind&&id==selected.id){if(record)return definitions::fail(error,"Character owning Collection repeats its target global ID");record=pointer;}
    }
    return definitions::unchanged(at,rows,error)&&(record||definitions::fail(error,"Character global ID is absent from its owning Collection"));
}
inline std::vector<std::string> affected(const definitions::Census& census,const definitions::Definition& selected){
    std::vector<std::string> out;for(const auto& row:census.definitions)if(!row.deleted&&(row.object==selected.object||(!selected.parent&&row.parent==selected.object)))out.push_back(row.key);return out;
}
}
