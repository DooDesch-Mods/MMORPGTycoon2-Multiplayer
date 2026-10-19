#pragma once
#include "general_project_reader.h"

namespace mt2mp::game::general_project {
struct ReferenceOwners {
    uintptr_t software=0,tree=0,library=0;
    std::vector<uintptr_t> techs,records,packs;
    std::map<std::string,uintptr_t> tech_keys,pack_keys;
    bool operator==(const ReferenceOwners&)const=default;
};
// External Record references need current canonical owners. Project eligibility,
// prices, inventory and implementation effects do not identify these objects.
template<class Read,class Text>
bool reference_owners(Read& read,Text& text,uintptr_t image,ReferenceOwners& out,std::string& error){
    out={};ReferenceOwners next;uintptr_t ap=0;
    const auto owner=[&](uintptr_t global,uintptr_t expected,uintptr_t& result){
        return read(image,global,result,error)&&read(result,0,ap,error)&&
            (ap==image+expected||(error="General project actual global owner class differs",false));
    };
    if(!owner(0x13df500,0x1715450,next.software)||!owner(0x13df250,0x16dc400,next.tree)||
       !read(image,0x13df2d0,next.library,error)||!next.library){
        if(error.empty())error="General project current ArtLibrary owner absent";return false;
    }
    if(!array(read,next.tree,0x10,next.techs,error)||!array(read,next.software,0x280,next.records,error)||
       !array(read,next.library,8,next.packs,error))return false;
    std::map<uintptr_t,std::string> tech_names;
    for(const auto object:next.techs){std::string name;
        if(!read(object,0,ap,error)||(ap!=image+0x1744c40&&ap!=image+0x16dc330&&ap!=image+0x16e0ac0&&ap!=image+0x16fdb10)||
           !text(object+0x20,name,error)||name.empty()||!next.tech_keys.emplace(name,object).second){
            if(error.empty())error="General project complete TechTree key/class invalid";return false;
        }
        tech_names.emplace(object,std::move(name));
    }
    std::set<std::string> record_keys;
    for(const auto object:next.records){std::string name;uintptr_t tech=0,back=0;
        if(!read(object,0,ap,error)||ap!=image+0x16e79d0||!text(object+0x38,name,error)||
           !read(object,0x60,tech,error)||!tech_names.contains(tech)||tech_names.at(tech)!=name||
           !read(tech,0x1e8,back,error)||back!=object){
            if(error.empty())error="General project current Software TechRecord reciprocity differs";return false;
        }
        if(!record_keys.insert(name).second){error="General project duplicate Software TechRecord";return false;}
    }
    if(next.records.size()!=next.techs.size()){error="General project Software records do not completely cover TechTree";return false;}
    for(const auto object:next.packs){std::string name;
        if(!read(object,0,ap,error)||ap!=image+0x16c0140||!text(object+0x28,name,error)||name.empty()||
           !next.pack_keys.emplace(name,object).second){
            if(error.empty())error="General project complete ArtLibrary/ArtStore ownership invalid";return false;
        }
    }
    out=std::move(next);error.clear();return true;
}
}
