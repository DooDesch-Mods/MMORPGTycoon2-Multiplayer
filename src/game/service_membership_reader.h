#pragma once
#include "typed_action/service_membership_action.h"
#include <algorithm>
#include <set>
namespace mt2mp::service_membership_reader {
namespace a=action_session;
struct LocalRow {
    uintptr_t object=0,destination=0,character=0,prototype=0,config=0;
    uint64_t uid=0;
    std::array<uintptr_t,2> history{};
    std::array<std::array<int32_t,8>,2> daily{};
    bool operator==(const LocalRow&)const=default;
};
struct Local {
    uintptr_t data=0,manager=0,character_data=0,config_manager=0,config_data=0;
    int32_t capacity=0,character_capacity=0,config_capacity=0;
    std::vector<uintptr_t> characters,configs;
    std::vector<uint64_t> character_uids;
    std::vector<LocalRow> rows;
    bool operator==(const Local&)const=default;
};
template<class Read>
bool array(Read& read,uintptr_t object,size_t offset,size_t maximum,uintptr_t& data,int32_t& capacity,std::vector<uintptr_t>& rows,std::string& e){
    int32_t count=0;
    if(!read(object,offset+8,data,e)||!read(object,offset+0x10,count,e)||!read(object,offset+0x14,capacity,e))return false;
    if(count<0||size_t(count)>maximum||capacity<count||capacity>1000000||(!data&&capacity)||data>UINTPTR_MAX-size_t(capacity)*8){e="Service membership array bounds invalid";return false;}
    rows.clear();std::set<uintptr_t> seen;
    for(int32_t i=0;i<count;++i){uintptr_t value=0;if(!read(data,size_t(i)*8,value,e))return false;
        if(!value||!seen.insert(value).second){e="Service membership duplicate/null owner";return false;}rows.push_back(value);}
    return true;
}
template<class Read,class Text,class Destination>
bool sample(Read& read,Text& text,Destination& destination,uintptr_t image,uintptr_t building,a::ServiceMembershipState& out,Local& local,std::string& e){
    out={};local={};uintptr_t ap=0;
    if(!text(building+0x768,out.building_definition,e)||!text(image+0x1a30080,out.spawn_type,e)||out.spawn_type.empty()||!read(image,0x13df7a0,local.manager,e)||!local.manager||
       !read(local.manager,0,ap,e)||ap!=image+0x1732430||
       !array(read,local.manager,0x80,10000,local.character_data,local.character_capacity,local.characters,e)){
        if(e.empty())e="Service membership Character manager differs";return false;}
    for(size_t i=0;i<local.characters.size();++i){const auto object=local.characters[i];a::ServiceCharacter value;int32_t kind=-1;uint64_t uid=0;uint8_t deleted=2;
        if(!read(object,0,ap,e)||ap!=image+0x1705990||!read(object,0x18,uid,e)||!uid||!read(object,0x2c,kind,e)||kind!=0||
           !read(object,0x30,value.id,e)||value.id!=int32_t(i)||!read(object,0x490,deleted,e)||deleted>1||!text(object+0xb0,value.name,e)){
            if(e.empty())e="Service membership Character kind/id/class differs";return false;}
        value.deleted=deleted!=0;out.characters.push_back(std::move(value));local.character_uids.push_back(uid);
    }
    if(!read(image,0x13df840,local.config_manager,e))return false;out.config_present=local.config_manager!=0;
    if(local.config_manager){
        if(!read(local.config_manager,0,ap,e)||ap!=image+0x173fba0||!array(read,local.config_manager,0x18,4096,local.config_data,local.config_capacity,local.configs,e)){
            if(e.empty())e="Service membership Config manager differs";return false;}
        for(auto object:local.configs){a::ServiceConfigValue row;
            if(!read(object,0,ap,e)||ap!=image+0x1736660||!text(object+0x18,row.key,e)||!read(object,0x38,ap,e)||ap!=image+0x174ab50||!read(object,0x50,row.price,e)){
                if(e.empty())e="Service membership Config/Price class differs";return false;}out.configs.push_back(std::move(row));}
    }
    std::vector<uintptr_t> rows;if(!array(read,building,0x9d0,1024,local.data,local.capacity,rows,e))return false;
    std::set<uint64_t> uids;
    for(auto object:rows){LocalRow pin;pin.object=object;a::ServiceMember row;uintptr_t owner=0;std::array<uint8_t,3> flags{};int32_t satisfaction=-1;
        if(!read(object,0,ap,e)||ap!=image+0x1718480||!read(object,0x18,pin.uid,e)||!pin.uid||!uids.insert(pin.uid).second||
           !read(object,0x148,owner,e)||owner!=building||!read(object,0x20,row.type,e)||row.type<0||row.type>16||
           !read(object,0x28,ap,e)||ap!=image+0x174ab50||!read(object,0x40,row.price,e)||!read(object,0x48,flags,e)||
           !read(object,0x130,pin.destination,e)||!read(object,0x138,pin.character,e)||!read(object,0x140,satisfaction,e)||
           !read(object,0x150,pin.config,e)||!read(object,0x178,pin.prototype,e)||!text(object+0x158,row.config_key,e)){
            if(e.empty())e="Service membership Service class/full UID/Building owner differs";return false;}
        // +0x140 is the local BuildingInfoWindow satisfaction cache, not a row index.
        // Keep the legacy wire slot at -1; the owning array defines Service order.
        for(size_t i=0;i<3;++i){if(flags[i]>1){e="Service membership flag invalid";return false;}row.flags[i]=flags[i]!=0;}
        if(pin.destination&&!destination(pin.destination,row.destination,e))return false;
        if(pin.character){const auto found=std::find(local.characters.begin(),local.characters.end(),pin.character);
            if(found==local.characters.end()){e="Service membership spawn Character outside current kind0 manager";return false;}row.character=int32_t(found-local.characters.begin());}
        if(pin.prototype&&(!read(pin.prototype,0,ap,e)||ap!=image+0x173be20)){if(e.empty())e="Service membership prototype class differs";return false;}
        row.prototype=pin.prototype!=0;row.config=pin.config!=0;
        if(pin.config){const auto found=std::find(local.configs.begin(),local.configs.end(),pin.config);
            if(found==local.configs.end()||out.configs[size_t(found-local.configs.begin())].key!=row.config_key){e="Service membership Config membership/key differs";return false;}}
        for(size_t i=0;i<2;++i){const auto offset=0x50+i*0x70;auto& words=pin.daily[i];auto& count=row.counts[i];uint8_t warning=2;
            if(!read(object,offset,ap,e)||ap!=image+0x16e4710||!read(object,offset+0x38,ap,e)||ap!=image+0x1749370||
               !read(object,offset+0x40,pin.history[i],e)||!read(object,offset+0x48,words,e)||!read(object,offset+0x68,warning,e)){
                if(e.empty())e="Service membership DailyCount class differs";return false;}
            if(words[0]<0||words[0]>10||words[1]<words[0]||words[1]>100000||words[5]!=10||warning>1||(!pin.history[i]&&words[1])||pin.history[i]>UINTPTR_MAX-size_t(words[1])*4){e="Service membership DailyCount bounds differ";return false;}
            count.today=words[2];count.total=words[3];count.highest=words[6];count.warning=warning!=0;
            for(int32_t j=0;j<words[0];++j){int32_t value=0;if(!read(pin.history[i],size_t(j)*4,value,e))return false;count.history.push_back(value);}
        }
        out.rows.push_back(std::move(row));local.rows.push_back(std::move(pin));
    }
    uintptr_t after=0;int32_t capacity=0;std::vector<uintptr_t> check;
    if(!array(read,building,0x9d0,1024,after,capacity,check,e)||after!=local.data||capacity!=local.capacity||check!=rows){if(e.empty())e="Service membership array changed during read";return false;}
    return true;
}
inline bool transition(const a::ServiceMembershipIntent& intent,const Local& before,const Local& after){
    auto expected=before;
    if(intent.kind==a::ServiceMembershipKind::Remove){
        if(size_t(intent.ordinal)>=expected.rows.size())return false;expected.rows.erase(expected.rows.begin()+intent.ordinal);
    }else return false;
    return expected==after;
}
inline bool transition(const a::ServiceMembershipIntent& intent,const a::ServiceMembershipState& state,const Local& before,const Local& after){
    if(intent.kind==a::ServiceMembershipKind::Remove)return transition(intent,before,after);
    if(state.rows.size()!=before.rows.size()||intent.character<-1||(intent.character>=0&&size_t(intent.character)>=before.characters.size()))return false;auto expected=before;expected.rows.clear();
    for(size_t i=0;i<state.rows.size();++i){const auto& row=state.rows[i];if(!(row.type==2&&row.destination.empty()&&row.character==intent.character))expected.rows.push_back(before.rows[i]);}
    if(after.rows.size()!=expected.rows.size()+1)return false;const auto& added=after.rows.back();
    if(!added.object||added.destination||added.prototype||added.config||added.character!=(intent.character<0?0:before.characters[size_t(intent.character)]))return false;
    for(const auto& old:before.rows)if(old.object==added.object||old.uid==added.uid)return false;
    for(size_t i=0;i<2;++i)if(!added.history[i]||added.daily[i][0]||added.daily[i][1]!=10||added.daily[i][2]||added.daily[i][3]||added.daily[i][5]!=10||added.daily[i][6]||added.daily[i][7])return false;
    if(expected.rows.size()>=size_t(before.capacity)){if(after.capacity!=std::max(4,before.capacity*2)||!after.data)return false;expected.data=after.data;expected.capacity=after.capacity;}
    expected.rows.push_back(added);return expected==after;
}
}
