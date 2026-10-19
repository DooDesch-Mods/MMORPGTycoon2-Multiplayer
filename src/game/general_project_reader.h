#pragma once
#include "general_project_state.h"
#include "general_project_effect_reader.h"
#include <map>
namespace mt2mp::game::general_project {
struct Local {
    uintptr_t lounge=0,software=0,tree=0,library=0,store=0,player=0,inventory=0,slots=0,current=0,random=0;
    std::vector<uintptr_t> catalogue,techs,records,packs,item_types;
    std::array<uintptr_t,200> items{};
    std::map<std::string,uintptr_t> sources,pack_keys,tech_keys;
    EffectOwners effects;
    bool operator==(const Local&)const=default;
};
template<class Read>bool array(Read& read,uintptr_t p,size_t offset,std::vector<uintptr_t>& out,std::string& e){
    uintptr_t data=0;int32_t count=0,capacity=0;
    if(!read(p,offset,data,e)||!read(p,offset+8,count,e)||!read(p,offset+12,capacity,e))return false;
    if(count<0||count>4096||capacity<count||capacity>65536||(!data&&capacity)||data>UINTPTR_MAX-size_t(capacity)*8){e="General project owned array extent invalid";return false;}
    out.clear();std::set<uintptr_t> seen;for(int32_t i=0;i<count;++i){uintptr_t value=0;if(!read(data,size_t(i)*8,value,e))return false;if(!value||!seen.insert(value).second){e="General project owned array duplicate/null member";return false;}out.push_back(value);}return true;
}
template<class Read,class Text>
bool source(Read& read,Text& text,uintptr_t image,uintptr_t object,Source& out,std::string& e){
    uintptr_t ap=0;if(!read(object,0,ap,e)||ap!=image+0x16e48c0||!read(object,0x18,out.kind,e)||!text(object+0x20,out.tech,e)||!text(object+0x40,out.art,e)||!read(object,0x60,out.statistic,e)){
        if(e.empty())e="General project actual source class/fields differ";return false;}
    if(!valid(out)){e="General project source kind/key/statistic invalid";return false;}return true;
}
template<class Read,class Text,class Flags,class PackFlags>
bool read(Read& read,Text& text,Flags& flags,PackFlags& pack_flags,uintptr_t image,Local& pin,State& out,std::string& e){
    out={};Local local;uintptr_t ap=0;int32_t count=0;
    const auto owner=[&](uintptr_t global,uintptr_t expected,uintptr_t& result){return read(image,global,result,e)&&read(result,0,ap,e)&&(ap==image+expected||(e="General project actual global owner class differs",false));};
    if(!owner(0x13df290,0x16de6c0,local.lounge)||!owner(0x13df500,0x1715450,local.software)||!owner(0x13df250,0x16dc400,local.tree)||
       !owner(0x13df210,0x16d9300,local.store)||!owner(0x13df9f0,0x17873b0,local.player)||!read(image,0x13df2d0,local.library,e)||!local.library||
       !read(image,0x164b250,local.random,e)||!local.random||!read(local.random,0,out.random,e)||
       !array(read,local.lounge,0xd8,local.catalogue,e)||!array(read,local.tree,0x10,local.techs,e)||!array(read,local.software,0x280,local.records,e)||!array(read,local.library,8,local.packs,e))return false;
    std::map<uintptr_t,std::string> tech_names;
    for(auto p:local.techs){std::string name;
        if(!read(p,0,ap,e)||(ap!=image+0x1744c40&&ap!=image+0x16dc330&&ap!=image+0x16e0ac0&&ap!=image+0x16fdb10)||!text(p+0x20,name,e)||name.empty()||!local.tech_keys.emplace(name,p).second){if(e.empty())e="General project complete TechTree key/class invalid";return false;}tech_names.emplace(p,std::move(name));}
    for(auto p:local.records){std::string name;uintptr_t tech=0,back=0;State::Tech row;
        if(!read(p,0,ap,e)||ap!=image+0x16e79d0||!text(p+0x38,name,e)||!read(p,0x60,tech,e)||!tech_names.contains(tech)||tech_names.at(tech)!=name||!read(tech,0x1e8,back,e)||back!=p||!read(p,0x58,row.flags,e)){
            if(e.empty())e="General project current Software TechRecord reciprocity differs";return false;}
        row.key=name;for(auto flag:row.flags)if(flag>1){e="General project TechRecord flag not bool";return false;}out.techs.push_back(std::move(row));
    }
    if(local.records.size()!=local.techs.size()){e="General project Software records do not completely cover TechTree";return false;}
    std::set<std::string> record_keys;for(const auto& row:out.techs)if(!record_keys.insert(row.key).second){e="General project duplicate Software TechRecord";return false;}
    if(!effects(read,text,image,local.tech_keys,out,local.effects,e))return false;
    for(auto p:local.packs){State::Pack row;
        if(!read(p,0,ap,e)||ap!=image+0x16c0140||!text(p+0x28,row.key,e)||row.key.empty()||!local.pack_keys.emplace(row.key,p).second||!pack_flags(local.store,p,row.purchased,row.deployed,e)){
            if(e.empty())e="General project complete ArtLibrary/ArtStore ownership invalid";return false;}out.packs.push_back(std::move(row));
    }
    for(auto p:local.catalogue){Catalogue row;if(!source(read,text,image,p,row.source,e)||!local.sources.emplace(key(row.source),p).second||!flags(p,row,e))return false;
        if(row.source.kind==0){const auto found=std::find_if(out.techs.begin(),out.techs.end(),[&](const auto& t){return t.key==row.source.tech;});if(found==out.techs.end()){e="General project catalogue Tech lacks current owned record";return false;}row.tech_flags=found->flags;}
        if(row.source.kind==1){const auto found=std::find_if(out.packs.begin(),out.packs.end(),[&](const auto& t){return t.key==row.source.art;});if(found==out.packs.end()){e="General project catalogue ArtPack lacks current library member";return false;}row.art_purchased=found->purchased;row.art_deployed=found->deployed;}
        out.catalogue.push_back(std::move(row));
    }
    if(!read(local.lounge,0xa8,local.current,e))return false;
    if(local.current){Source value;if(std::find(local.catalogue.begin(),local.catalogue.end(),local.current)!=local.catalogue.end()||!source(read,text,image,local.current,value,e)||!local.sources.contains(key(value))){if(e.empty())e="General project current clone aliases/lacks catalogue source";return false;}out.progress.current=std::move(value);}
    if(!read(local.lounge,0xb0,out.progress.work,e)||!read(local.lounge,0xb4,out.progress.lines,e)||!read(local.lounge,0xb8,out.progress.bugs_total,e)||!read(local.lounge,0xbc,out.progress.bugs_known,e)||!read(local.lounge,0xc4,out.progress.workers,e)||
       !read(local.software,0x31c,out.statistics,e)||!read(local.software,0x350,out.spent,e)||!read(local.software,0x344,out.purchased,e)||!read(local.software,0x37c,out.free_points,e)||!read(local.software,0x378,out.purchased_points,e))return false;
    if(!read(local.player,0x1d0,local.inventory,e)||!read(local.inventory,0,ap,e)||ap!=image+0x16df0f0||!read(local.inventory,8,local.slots,e)||!read(local.inventory,0x10,count,e)||count!=200||!read(local.slots,0,local.items,e))return false;
    uintptr_t head=0,tail=0,previous=0;if(!read(image,0x13e2050,head,e)||!read(image,0x13e2040,tail,e))return false;
    std::set<uintptr_t> types;
    for(auto node=head;node;){uintptr_t back=0,next=0,table=0,getter=0;
        if(types.size()>=4096||!types.insert(node).second||!read(node,0x28,back,e)||back!=previous||!read(node,0x30,next,e)||!read(node,0,table,e)||!read(table,0xa8,getter,e)||getter!=image+0xd31590){if(e.empty())e="General project complete ItemType catalogue differs";return false;}
        local.item_types.push_back(node);previous=node;node=next;
    }
    if(previous!=tail){e="General project ItemType catalogue tail differs";return false;}
    std::set<uintptr_t> item_owners;
    for(size_t i=0;i<local.items.size();++i){auto p=local.items[i];if(!p)continue;uintptr_t type=0,proxy=0,target=0,back=0,table=0,getter=0;
        if(!item_owners.insert(p).second||!read(p,0,ap,e)||ap!=image+0x17437f0||!read(p,0x68,type,e)||!types.contains(type)||!read(type,0,table,e)||!read(table,0xa8,getter,e)||getter!=image+0xd31590||
           !text(type+0x48,out.inventory[i],e)||!read(p,0x70,proxy,e)||!proxy||!read(proxy,0,target,e)||target!=type+0x38||!read(type,0x40,back,e)||back!=proxy){if(e.empty())e="General project actual Item/ItemType weak owner differs";return false;}
    }
    if(!valid(out)){e="General project actual complete state invalid";return false;}pin=std::move(local);e.clear();return true;
}
}
