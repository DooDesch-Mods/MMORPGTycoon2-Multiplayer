#include "reference_map.h"
#include <bit>
#include <set>
#include "snapshot_building_key.h"
#include "npc_target_key.h"

namespace mt2mp::subscriber_lifecycle::first_append {
namespace {
bool fail(std::string& e, const char* s) { e = s; return false; }
template<class T> bool at(Memory& m, uintptr_t p, size_t o, T& v, std::string& e) {
    if (!p || o > UINTPTR_MAX - p) return fail(e, "reference field address overflow");
    return m.read(p + o, &v, sizeof v, e);
}
bool ap(Memory& m, const ReferenceWorld& w, uintptr_t p, uintptr_t rva, std::string& e) {
    uintptr_t v = 0;
    return at(m, p, 0, v, e) && (v == w.image + rva || fail(e, "reference native addresspoint differs"));
}
bool array(Memory& m, uintptr_t p, uintptr_t& data, int32_t& count, std::string& e) {
    int32_t capacity = -1;
    if (!at(m,p,8,data,e) || !at(m,p,0x10,count,e) || !at(m,p,0x14,capacity,e)) return false;
    return (count >= 0 && capacity >= count && (!count || data) &&
        static_cast<size_t>(count) <= (UINTPTR_MAX-data)/8) || fail(e,"reference native array extent invalid");
}
bool roots(Memory& m,const ReferenceWorld& w,uintptr_t& shard,std::string& e) {
    if (!w.image || w.image > UINTPTR_MAX-0x1787930 || !w.state || !w.map || w.map > UINTPTR_MAX-0x1c0)
        return fail(e,"reference world address invalid");
    if (!ap(m,w,w.state,0x16de920,e) || !ap(m,w,w.map,0x1742860,e)) return false;
    shard=w.map+0x1c0; int32_t type=-1;
    if (!ap(m,w,shard,0x174ad20,e) || !at(m,shard,0x48,type,e)) return false;
    return type==0 || fail(e,"reference map root Shard type differs");
}
bool regions(Memory& m,const ReferenceWorld& w,uintptr_t shard,
             uintptr_t wanted,int32_t wanted_id,uintptr_t& found,int32_t& found_id,std::string& e) {
    uintptr_t data=0;int32_t count=0;
    if (!array(m,w.map+0x350,data,count,e)) return false;
    std::set<uintptr_t> pointers;std::set<int32_t> ids;found=0;
    for(int32_t i=0;i<count;++i){
        uintptr_t p=0,owner=0;int32_t id=-1;
        if(!at(m,data,size_t(i)*8,p,e)||!ap(m,w,p,0x1787930,e)||!at(m,p,0x70,owner,e)||!at(m,p,0x834,id,e))return false;
        if(owner!=shard || id<0 || !pointers.insert(p).second || !ids.insert(id).second)
            return fail(e,"reference root Region ownership/unique ID differs");
        if((wanted&&p==wanted)||(!wanted&&id==wanted_id)){found=p;found_id=id;}
    }
    return found!=0 || fail(e,"reference Region is not a unique current root Map member");
}
bool abilities(Memory& m,const ReferenceWorld& w,int32_t kind,int32_t id,
               uintptr_t wanted,int32_t wanted_index,uintptr_t& found,int32_t& found_index,std::string& e){
    if(kind<0||kind>=5||id<0)return fail(e,"reference CharacterType key invalid");
    uintptr_t manager=0,singleton=0,data=0,type=0;int32_t count=0;
    if(!at(m,w.state,0x168,manager,e)||!at(m,w.image,0x13df7a0,singleton,e)||manager!=singleton)
        return fail(e,e.empty()?"reference CharacterTypeManager world/singleton differs":e.c_str());
    if(!ap(m,w,manager,0x1732430,e)||!array(m,manager+0x80+size_t(kind)*0x50,data,count,e))return false;
    if(id>=count)return fail(e,"reference CharacterType ID outside native array");
    if(!at(m,data,size_t(id)*8,type,e)||!ap(m,w,type,0x1705990,e))return false;
    int32_t actual_kind=-1,actual_id=-1;uint8_t destroyed=0;
    if(!at(m,type,0x2c,actual_kind,e)||!at(m,type,0x30,actual_id,e)||!at(m,type,0x490,destroyed,e))return false;
    if(actual_kind!=kind||actual_id!=id||destroyed)return fail(e,"reference CharacterType identity/destroyed guard differs");
    if(!array(m,type+0x128,data,count,e))return false;
    std::set<uintptr_t> pointers;found=0;
    for(int32_t i=0;i<count;++i){
        uintptr_t p=0;
        if(!at(m,data,size_t(i)*8,p,e)||!ap(m,w,p,0x171c930,e))return false;
        if(!pointers.insert(p).second)return fail(e,"reference Ability owner array contains duplicate objects");
        if((wanted&&p==wanted)||(!wanted&&i==wanted_index)){found=p;found_index=i;}
    }
    return found!=0 || fail(e,"reference Ability is not a member of captured Toon CharacterType");
}
bool subscriber_toon(Memory& m,const ReferenceWorld& w,uintptr_t wanted,int32_t wanted_id,
                     uintptr_t& found,int32_t& found_id,std::string& e){
    found=0;found_id=-1;e.clear();
    std::string detail="Subscriber Toon reference subscriber_id="+std::to_string(wanted_id);
    const auto reject=[&](const char* condition){e=detail+" condition="+condition+(e.empty()?"":" original="+e);return false;};
    uintptr_t manager=0,singleton=0,data=0,owner=0;int32_t count=0,next=-1,capacity=-1;
    if(wanted){
        uint64_t uid=0;
        if(!ap(m,w,wanted,0x1744e90,e)||!at(m,wanted,0x18,uid,e)||!at(m,wanted,0x940,owner,e))return reject("Toon fields unreadable");
        detail+=" toon_uid="+std::to_string(uid)+" subscriber_owner_present="+std::to_string(owner!=0);
        if(!uid||!owner)return reject("nonzero UID and Subscriber owner required");
        if(!ap(m,w,owner,0x16e75f0,e)||!at(m,owner,0x20,wanted_id,e))return reject("Subscriber owner identity unreadable");
        detail+=" actual_subscriber_id="+std::to_string(wanted_id);
    }
    if(!at(m,w.state,0x1b8,manager,e)||!at(m,w.image,0x13df690,singleton,e))return reject("Manager owner unreadable");
    if(manager!=singleton||!ap(m,w,manager,0x1725f40,e))return reject("Manager world/singleton differs");
    if(!array(m,manager+0x38,data,count,e)||!at(m,manager,0x4c,capacity,e)||!at(m,manager,0x68,next,e))return reject("Manager census unreadable");
    detail+=" manager_count="+std::to_string(count)+" next_id="+std::to_string(next);
    if(count>1000000||wanted_id<0||wanted_id>=next)return reject("Subscriber ID/census outside bounds");
    uintptr_t selected=0;int32_t selected_index=-1;std::set<uintptr_t> objects;std::set<int32_t> ids;
    for(int32_t i=0;i<count;++i){
        uintptr_t subscriber=0;int32_t actual_id=-1;
        if(!at(m,data,size_t(i)*8,subscriber,e)||!ap(m,w,subscriber,0x16e75f0,e)||!at(m,subscriber,0x20,actual_id,e))return reject("Manager member unreadable");
        if(actual_id<0||actual_id>=next||!objects.insert(subscriber).second||!ids.insert(actual_id).second)return reject("Manager member identity not unique");
        if(actual_id==wanted_id){selected=subscriber;selected_index=i;}
    }
    if(!selected||(wanted&&selected!=owner))return reject("Subscriber absent from unique current Manager membership");
    uintptr_t toon=0,brain=0,reverse=0,brain_owner=0;uint64_t uid=0;uint8_t online=0;
    if(!at(m,selected,0x80,toon,e)||!at(m,selected,0x88,brain,e)||!ap(m,w,toon,0x1744e90,e)||
       !at(m,toon,0x18,uid,e)||!at(m,toon,0x940,reverse,e)||!at(m,toon,0x825,online,e)||
       (brain&&(!ap(m,w,brain,0x16fdd60,e)||!at(m,brain,0x28,brain_owner,e))))return reject("owned Toon/Brain fields unreadable");
    detail+=" selected_toon_uid="+std::to_string(uid)+" reciprocal_toon="+std::to_string(reverse==selected)+" reciprocal_brain="+std::to_string(brain_owner==selected);
    if(!uid||reverse!=selected||(brain&&brain_owner!=selected)||online>1||(wanted&&wanted!=toon))return reject("Subscriber Toon/Brain reciprocal ownership differs");
    uintptr_t after_member=0,after_manager=0,after_singleton=0,after_data=0,after_toon=0,after_brain=0,after_reverse=0,after_brain_owner=0;
    int32_t after_count=0,after_capacity=-1,after_next=-1,after_id=-1;uint64_t after_uid=0;uint8_t after_online=0;
    if(!at(m,w.state,0x1b8,after_manager,e)||!at(m,w.image,0x13df690,after_singleton,e)||
       !array(m,manager+0x38,after_data,after_count,e)||!at(m,manager,0x4c,after_capacity,e)||!at(m,manager,0x68,after_next,e)||
       !at(m,data,size_t(selected_index)*8,after_member,e)||!at(m,selected,0x20,after_id,e)||!at(m,selected,0x80,after_toon,e)||!at(m,selected,0x88,after_brain,e)||
       !at(m,toon,0x18,after_uid,e)||!at(m,toon,0x940,after_reverse,e)||!at(m,toon,0x825,after_online,e)||(brain&&!at(m,brain,0x28,after_brain_owner,e)))return reject("owner final readback unreadable");
    if(after_member!=selected||after_manager!=manager||after_singleton!=manager||after_data!=data||after_count!=count||after_capacity!=capacity||after_next!=next||
       after_id!=wanted_id||after_toon!=toon||after_brain!=brain||after_uid!=uid||after_reverse!=selected||(brain&&after_brain_owner!=selected)||after_online!=online)
        return reject("owner/census final readback changed");
    found=toon;found_id=wanted_id;e.clear();return true;
}
}
bool valid_npc_key(const std::string& key) {return game::npc_target_key::valid(key);}
bool valid_building_key(const std::string& key) {
    if (game::snapshot_building_key::valid(key)) return true;
    if (key.size()!=43 || !key.starts_with("action:")) return false;
    for (size_t i=7;i<key.size();++i) {
        const size_t n=i-7; const char c=key[i];
        if (n==8||n==13||n==18||n==23) { if(c!='-') return false; }
        else if (!((c>='0'&&c<='9')||(c>='a'&&c<='f'))) return false;
    }
    return true;
}
bool record_character_type(const std::vector<Record>& rows,int32_t& kind,int32_t& id,std::string& e){
    kind=id=-1;size_t toon=SIZE_MAX;int matches=0;
    for(size_t i=0;i<rows.size();++i){const auto& r=rows[i];
        if(r.label.type==1&&r.label.text=="mmoToon"&&r.parent<rows.size()&&rows[r.parent].label.text=="main"&&rows[r.parent].parent==0){toon=i;++matches;}}
    if(matches!=1)return fail(e,"captured Subscriber must contain exactly one main Toon");
    int kinds=0,ids=0;
    for(const auto& r:rows)if(r.parent==toon&&(r.label.text=="typeType"||r.label.text=="typeId")){
        if(r.tokens.size()!=1||r.tokens[0].type!=4)return fail(e,"captured Toon CharacterType is not a native integer");
        if(r.label.text=="typeType"){kind=std::bit_cast<int32_t>(r.tokens[0].bits);++kinds;}
        else{id=std::bit_cast<int32_t>(r.tokens[0].bits);++ids;}
    }
    return (kinds==1&&ids==1&&kind>=0&&kind<5&&id>=0)||fail(e,"captured Toon CharacterType fields absent/duplicate/invalid");
}
bool identify_reference(Memory& m,const ReferenceWorld& w,uintptr_t object,int32_t kind,int32_t id,ReferenceKey& out,std::string& e){
    out={};uintptr_t shard=0,vptr=0,found=0;int32_t index=-1;
    if(!roots(m,w,shard,e)||!at(m,object,0,vptr,e))return false;
    if(object==shard){out.kind=ReferenceKind::RootShard;return true;}
    if(vptr==w.image+0x1787930){
        if(!regions(m,w,shard,object,-1,found,index,e))return false;
        out.kind=ReferenceKind::RootRegion;out.region_id=index;return true;
    }
    if(vptr==w.image+0x171c930){
        if(!abilities(m,w,kind,id,object,-1,found,index,e))return false;
        out.kind=ReferenceKind::CharacterAbility;out.character_kind=kind;out.character_id=id;out.ability_index=index;return true;
    }
    if(vptr==w.image+0x16d93c0){
        if(!w.buildings)return fail(e,"Building reference has no pinned shared bindings");
        if(!w.buildings->identify(object,out.building_key,e))return false;
        if(!valid_building_key(out.building_key))return fail(e,"Building reference shared key invalid");
        out.kind=ReferenceKind::SharedBuilding;return true;
    }
    if(vptr==w.image+0x1742980){
        if(!w.npcs)return fail(e,"NPC reference has no pinned shared bindings");
        if(!w.npcs->identify(object,out.npc_key,e))return false;
        if(!valid_npc_key(out.npc_key))return fail(e,"NPC reference shared key invalid");
        out.kind=ReferenceKind::SharedNPC;return true;
    }
    if(vptr==w.image+0x174ac10){
        if(!w.npcs)return fail(e,"Quest reference has no pinned shared bindings");
        if(!w.npcs->identify_quest(object,out.npc_key,out.sequence_id,e))return false;
        if(!valid_npc_key(out.npc_key)||out.sequence_id<0)return fail(e,"Quest reference shared key invalid");
        out.kind=ReferenceKind::SharedQuest;return true;
    }
    if(vptr==w.image+0x1744e90){
        if(!subscriber_toon(m,w,object,-1,found,index,e))return false;
        out.kind=ReferenceKind::SubscriberToon;out.subscriber_id=index;return true;
    }
    return fail(e,"external reference has no supported portable owner key");
}
bool resolve_reference(Memory& m,const ReferenceWorld& w,const ReferenceKey& key,uintptr_t& out,std::string& e){
    out=0;uintptr_t shard=0;int32_t index=-1;
    if(!roots(m,w,shard,e))return false;
    if(key.kind!=ReferenceKind::SharedNPC&&key.kind!=ReferenceKind::SharedQuest&&!key.npc_key.empty())return fail(e,"non-NPC reference has NPC key");
    if(key.kind!=ReferenceKind::SharedBuilding&&!key.building_key.empty())return fail(e,"non-Building reference has Building key");
    if(key.kind!=ReferenceKind::SubscriberToon&&key.subscriber_id!=-1)return fail(e,"non-Toon reference has Subscriber ID");
    if(key.kind!=ReferenceKind::SharedQuest&&key.sequence_id!=-1)return fail(e,"non-Quest reference has Quest sequence");
    switch(key.kind){
    case ReferenceKind::RootShard:
        if(key.region_id!=-1||key.character_kind!=-1||key.character_id!=-1||key.ability_index!=-1)return fail(e,"RootShard key has foreign fields");
        out=shard;return true;
    case ReferenceKind::RootRegion:
        if(key.region_id<0||key.character_kind!=-1||key.character_id!=-1||key.ability_index!=-1)return fail(e,"RootRegion key has foreign fields");
        return regions(m,w,shard,0,key.region_id,out,index,e);
    case ReferenceKind::SubscriberToon:
        if(key.region_id!=-1||key.character_kind!=-1||key.character_id!=-1||key.ability_index!=-1||key.subscriber_id<0)return fail(e,"Subscriber Toon key has invalid/foreign fields");
        return subscriber_toon(m,w,0,key.subscriber_id,out,index,e);
    case ReferenceKind::SharedQuest:
        if(key.region_id!=-1||key.character_kind!=-1||key.character_id!=-1||key.ability_index!=-1||
           !valid_npc_key(key.npc_key)||key.sequence_id<0)return fail(e,"Quest reference key has invalid/foreign fields");
        if(!w.npcs)return fail(e,"Quest reference has no pinned shared bindings");
        if(!w.npcs->resolve_quest(key.npc_key,key.sequence_id,out,e))return false;
        return ap(m,w,out,0x174ac10,e);
    case ReferenceKind::SharedNPC:
        if(key.region_id!=-1||key.character_kind!=-1||key.character_id!=-1||key.ability_index!=-1||!valid_npc_key(key.npc_key))return fail(e,"NPC reference key has invalid/foreign fields");
        if(!w.npcs)return fail(e,"NPC reference has no pinned shared bindings");
        if(!w.npcs->resolve(key.npc_key,out,e))return false;
        return ap(m,w,out,0x1742980,e);
    case ReferenceKind::SharedBuilding:
        if(key.region_id!=-1||key.character_kind!=-1||key.character_id!=-1||key.ability_index!=-1||!valid_building_key(key.building_key))return fail(e,"Building reference key has invalid/foreign fields");
        if(!w.buildings)return fail(e,"Building reference has no pinned shared bindings");
        if(!w.buildings->resolve(key.building_key,out,e))return false;
        return ap(m,w,out,0x16d93c0,e);
    case ReferenceKind::CharacterAbility:
        if(key.region_id!=-1||key.ability_index<0)return fail(e,"CharacterAbility key has foreign fields");
        return abilities(m,w,key.character_kind,key.character_id,0,key.ability_index,out,index,e);
    }
    return fail(e,"external reference kind invalid");
}
}
