#include "visual.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <set>
namespace mt2mp::game::toon_weapon_visual {
namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
template<class T> bool get(placement_ui_probe::Memory& m,uintptr_t p,size_t o,T& x,std::string& e){
    if(!p||p>UINTPTR_MAX-o||p+o>UINTPTR_MAX-sizeof x)return fail(e,"Weapon visual read address overflow");
    return m.copy(p+o,&x,sizeof x,e);
}
bool text(placement_ui_probe::Memory& m,uintptr_t p,std::string& s,std::string& e){
    uintptr_t data=0;uint64_t size=0;
    if(!get(m,p,0,data,e)||!get(m,p,8,size,e))return false;
    if(size>4096||!data||data>UINTPTR_MAX-size-1)return fail(e,"Weapon visual catalogue string bounds differ");
    s.resize(static_cast<size_t>(size));if(size&&!m.copy(data,s.data(),s.size(),e))return false;
    char end=0;if(!get(m,data,static_cast<size_t>(size),end,e))return false;
    return (!end&&s.find('\0')==std::string::npos)||fail(e,"Weapon visual catalogue string is not exact terminated text");
}
bool array(placement_ui_probe::Memory& m,uintptr_t p,size_t offset,uintptr_t& data,int32_t& count,std::string& e){
    int32_t cap=0;
    if(!get(m,p,offset,data,e)||!get(m,p,offset+8,count,e)||!get(m,p,offset+12,cap,e))return false;
    return (count>=0&&cap>=count&&cap<=1048576&&(!count||data))||fail(e,"Weapon visual owned array bounds differ");
}
bool allowed_category(placement_ui_probe::Memory& m,const Plan& p,std::string& e){
    uintptr_t buckets=0;int32_t count=0,shift=0;
    if(!get(m,p.type,0xf8,buckets,e)||!get(m,p.type,0x100,count,e)||!get(m,p.type,0x104,shift,e))return false;
    if(!buckets||count<=0||count>65536||!std::has_single_bit(static_cast<uint32_t>(count))||shift!=32-static_cast<int32_t>(std::bit_width(static_cast<uint32_t>(count))-1))
        return fail(e,"Weapon CharacterType category bucket owner/count/shift differs");
    size_t matches=0;std::set<uintptr_t> seen;
    for(int32_t i=0;i<count;++i){uintptr_t node=0;if(!get(m,buckets,static_cast<size_t>(i)*0x30+0x28,node,e))return false;
        while(node){if(!seen.insert(node).second||seen.size()>65536)return fail(e,"Weapon CharacterType category chain is cyclic or oversized");
            std::string key;if(!text(m,node,key,e))return false;if(key==p.category)++matches;
            if(!get(m,node,0x28,node,e))return false;}}
    return matches==1||fail(e,"Weapon category is absent or ambiguous in actual CharacterType");
}
bool model_for(placement_ui_probe::Memory& m,Plan& p,std::string& e){
    if(!get(m,p.image,0x13df6e0,p.manager,e)||!get(m,p.image,0x13df4d0,p.rez_manager,e))return false;
    uintptr_t cats=0;int32_t n=0;if(!array(m,p.manager,0x60,cats,n,e))return false;
    uintptr_t category=0;for(int32_t i=0;i<n;++i){std::string key;const auto at=cats+static_cast<size_t>(i)*0x78;
        if(!text(m,at,key,e))return false;if(key==p.category){if(category)return fail(e,"Weapon catalogue category is ambiguous");category=at;}}
    if(!category)return fail(e,"Weapon catalogue category is absent");
    uintptr_t tiers=0;int32_t tiers_n=0;if(!array(m,category,0x68,tiers,tiers_n,e))return false;
    if(!tiers_n)return fail(e,"Weapon catalogue has no ilvl tier");
    int32_t selected=0,previous=INT32_MIN;
    for(int32_t i=0;i<tiers_n;++i){int32_t threshold=0;if(!get(m,tiers,static_cast<size_t>(i)*0x20,threshold,e))return false;
        if(threshold<previous)return fail(e,"Weapon catalogue ilvl tiers are unordered");previous=threshold;if(p.ilvl>=threshold)selected=i;}
    uintptr_t weapons=0;int32_t weapons_n=0;if(!array(m,tiers+static_cast<size_t>(selected)*0x20,0x10,weapons,weapons_n,e))return false;
    int matches=0;for(int32_t i=0;i<weapons_n;++i){int32_t variant=0;if(!get(m,weapons,static_cast<size_t>(i)*8+4,variant,e))return false;
        if(variant==p.variant){++matches;if(!get(m,weapons,static_cast<size_t>(i)*8,p.model_id,e))return false;}}
    if(matches!=1)return fail(e,"Weapon selected variant is absent or ambiguous in exact ilvl tier");
    uintptr_t models=0;int32_t model_n=0;if(!array(m,p.rez_manager,0x68,models,model_n,e))return false;
    if(p.model_id<0||p.model_id>=model_n)return fail(e,"Weapon selected model ID outside owned ModelRez catalogue");
    if(!get(m,models,static_cast<size_t>(p.model_id)*0x10,p.model,e))return false;
    return p.model!=0||fail(e,"Weapon selected catalogue model is null");
}
}
bool read_plan(placement_ui_probe::Memory& m,uintptr_t image,uintptr_t toon,Plan& p,std::string& e){
    p={};p.image=image;p.toon=toon;
    if(!get(m,toon,0x498,p.type,e)||!get(m,toon,0x6f0,p.actor,e)||!get(m,toon,0x170,p.shard,e)||
       !get(m,toon,0x4b0,p.ilvl,e)||!get(m,toon,0x4d8,p.variant,e)||!get(m,toon,0x4dc,p.prior_attack,e)||!text(m,toon+0x4b8,p.category,e)||
       !get(m,p.type,0xf0,p.costume,e))return false;
    std::string costume_type;if(!text(m,p.costume+0x38,costume_type,e))return false;p.humanoid=costume_type=="humanoid";
    if(p.actor&&(!get(m,p.actor,0,p.actor_object,e)||!get(m,p.actor_object,0x40,p.skeleton,e)))return false;
    if(p.actor&&!p.skeleton)return fail(e,"Weapon current actor skeleton is null");
    if(p.ilvl<=0||!p.humanoid)return true;
    if(!allowed_category(m,p,e)||!model_for(m,p,e))return false;
    if(!p.actor)return true;
    uintptr_t descriptors=0;int32_t parts=0;if(!array(m,p.costume,0x60,descriptors,parts,e))return false;
    uintptr_t definition=0,bones=0;int32_t bone_n=0;
    if(!get(m,p.skeleton,0,definition,e)||!get(m,definition,0x68,bones,e)||!get(m,p.skeleton,0xb0,bone_n,e))return false;
    if(bone_n<0||bone_n>65536||(!bones&&bone_n))return fail(e,"Weapon skeleton bone owner/count differs");
    for(int32_t i=0;i<parts;++i){Part part;uintptr_t attachments=0;int32_t attachment_n=0;
        if(!get(m,descriptors,static_cast<size_t>(i)*8,part.descriptor,e)||!array(m,part.descriptor,0xd0,attachments,attachment_n,e))return false;
        if(!attachment_n)continue;
        if(!get(m,attachments,0,part.attachment,e))return false;
        std::string kind;if(!text(m,part.attachment+0x18,kind,e))return false;if(kind!="hand")continue;
        part.name=part.descriptor+0x18;part.transform=part.attachment+0x38;
        if(!get(m,part.transform,0,part.trs,e))return false;
        if(!std::all_of(part.trs.begin(),part.trs.end(),[](float v){return std::isfinite(v);}))return fail(e,"Weapon attachment transform is nonfinite");
        std::string name;if(!text(m,part.name,name,e))return false;
        for(int32_t j=0;j<bone_n;++j){uintptr_t bone=0;std::string key;if(!get(m,bones,static_cast<size_t>(j)*8,bone,e)||!text(m,bone,key,e))return false;
            if(key==name){if(part.bone!=-1)return fail(e,"Weapon attachment bone is ambiguous");part.bone=j;}}
        if(part.bone<0)return fail(e,"Weapon attachment bone missing from current skeleton");
        p.parts.push_back(part);
    }
    if(!p.parts.empty()){
        if(p.category=="guns"||p.category=="crossbows"||p.category=="rayguns")p.attack=5;
        else if(p.category=="bows")p.attack=6;
    }
    return true;
}
bool read_attachment(placement_ui_probe::Memory& m,const Plan& p,bool& matching,std::string& e){
    matching=false;int32_t attack=0;if(!get(m,p.toon,0x4dc,attack,e))return false;
    if(!p.actor){matching=attack==p.attack;return true;}
    uintptr_t instance=0,transform=0;if(!get(m,p.skeleton,0xe8,instance,e)||!get(m,p.skeleton,0xf0,transform,e))return false;
    if(p.parts.empty()){matching=!instance&&!transform&&attack==p.attack;return true;}
    if(!instance||!transform)return true;
    const auto& part=p.parts.back();int32_t bone=-1;uintptr_t group=0,model=0;
    std::array<float,10> trs{};uint8_t visible=0,required_visible=0;std::array<float,4> color{},required_color{};
    if(!get(m,p.skeleton,0xf8,bone,e)||!get(m,instance,0x50,group,e)||!get(m,group,0x58,model,e)||!get(m,transform,0,trs,e)||
       !get(m,p.skeleton,0xe4,required_visible,e)||!get(m,instance,0x70,visible,e)||!get(m,p.skeleton,0xa0,required_color,e)||!get(m,instance,0x40,color,e))return false;
    if(required_visible>1||visible>1)return fail(e,"Weapon attachment visible value is not boolean");
    uintptr_t groups=0;int32_t count=0;if(!array(m,p.rez_manager,0x98,groups,count,e))return false;
    int owners=0;bool selected=false;for(int32_t i=0;i<count;++i){uintptr_t record=0,actual_group=0,scene=0;int32_t model_id=-1;
        if(!get(m,groups,static_cast<size_t>(i)*8,record,e)||!get(m,record,8,actual_group,e))return false;
        if(actual_group!=group)continue;
        if(!get(m,record,0x70,scene,e)||!get(m,record,0,model_id,e))return false;
        uintptr_t models=0,catalogue_model=0;int32_t model_n=0;
        if(!array(m,p.rez_manager,0x68,models,model_n,e))return false;
        if(model_id<0||model_id>=model_n)return fail(e,"Weapon existing attachment model ID outside owned catalogue");
        if(!get(m,models,static_cast<size_t>(model_id)*0x10,catalogue_model,e))return false;
        if(scene!=p.scene||model!=catalogue_model)return fail(e,"Weapon attachment group has different scene/model owner");
        selected=model_id==p.model_id;++owners;}
    if(owners!=1)return fail(e,"Weapon attachment group absent or ambiguous in ModelRez owner");
    matching=selected&&bone==part.bone&&model==p.model&&trs==part.trs&&visible==required_visible&&color==required_color&&attack==p.attack;
    return true;
}
}
