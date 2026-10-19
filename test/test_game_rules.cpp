#include "game_rules_native.h"
#include <map>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace gr=mt2mp::game::game_rules;
namespace {
unsigned checks=0,notifications=0;std::vector<int32_t> calls,events;std::string property;
void need(bool value,const std::string& error){++checks;if(!value)throw std::runtime_error(error);}
void notify(void*,const void* text){struct String{const char* data;size_t length;};const auto& value=*static_cast<const String*>(text);property.assign(value.data,value.length);++notifications;}
void feature(void* receiver,int32_t i,bool enabled){auto* flags=static_cast<uint8_t*>(receiver)+0x18;calls.push_back(i);if(!flags[i])events.push_back(i);flags[i]=uint8_t(enabled);}
gr::State state(){gr::State out;out.xp[0]=0xaabbccdd;out.xp[1]=10;out.respawn_seconds=60;out.available={1,1};return out;}
gr::Intent intent(gr::Command command){gr::Intent out{command,state(),state()};switch(command){case gr::Command::ApplyXP:out.after.xp[2]=3;break;case gr::Command::RespawnSeconds:out.after.respawn_seconds=90;break;case gr::Command::RespawnDelay:out.after.respawn_delay=true;break;case gr::Command::Features:out.after.features[0]=1;break;}return out;}
void domain(){auto value=state();need(gr::valid(value),"complete native rules invalid");for(size_t i=1;i<14;++i){auto bad=value;bad.xp[i]=UINT32_MAX;need(!gr::valid(bad),"negative reflected XP integer accepted");}
    for(auto command:{gr::Command::ApplyXP,gr::Command::RespawnSeconds,gr::Command::RespawnDelay,gr::Command::Features}){auto request=intent(command);need(gr::valid(request),"original command effect rejected");gr::Proof before{request.before,std::string(64,'a')},after{request.after,std::string(64,'a')};need(gr::effect(request,before,after),"exact complete rule effect rejected");after.state.xp[0]^=1;need(!gr::effect(request,before,after),"unrelated native XP word drift accepted");}
    auto request=intent(gr::Command::ApplyXP);request.after.xp[0]^=1;need(!gr::valid(request),"uneditable native XP word overwritten");request=intent(gr::Command::ApplyXP);request.after.xp[1]=request.after.xp[2]=0;need(!gr::valid(request),"native disabled zero-XP Apply accepted");
    request=intent(gr::Command::Features);request.before.available[0]=request.after.available[0]=0;need(!gr::valid(request),"unowned feature enabled");request=intent(gr::Command::Features);request.after=request.before;need(gr::valid(request)&&gr::feature_events(request.before)==2&&gr::feature_calls(request.before)==3,"unchanged false Features command lost both original events");
    request.before.available=request.after.available={0,0};need(!gr::valid(request),"Features without any native owned entry accepted");
    auto wire=gr::encode(value);need(gr::codec::state(wire)==value,"full rules codec");for(const auto& malformed:{nlohmann::json(true),nlohmann::json(-1),nlohmann::json(1.5),nlohmann::json(uint64_t(UINT32_MAX)+1)}){auto bad=wire;bad["xp"][1]=malformed;bool rejected=false;try{(void)gr::codec::state(bad);}catch(const std::exception&){rejected=true;}need(rejected,"coerced/overflowing XP scalar accepted");}
    for(const auto& malformed:{nlohmann::json::array(),nlohmann::json::array({0})}){auto bad=wire;bad["xp"]=malformed;bool rejected=false;try{(void)gr::codec::state(bad);}catch(const std::exception&){rejected=true;}need(rejected,"incomplete XP block accepted");}
}
struct Memory {std::map<uintptr_t,uint8_t> bytes;std::map<uintptr_t,std::string> names;std::map<std::string,uintptr_t> techs;unsigned copies=0;
    template<class T>void put(uintptr_t p,const T& value){const auto* data=reinterpret_cast<const uint8_t*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[p+i]=data[i];}
    bool copy(uintptr_t p,void* out,size_t n,std::string& error){++copies;auto* target=static_cast<uint8_t*>(out);for(size_t i=0;i<n;++i){const auto it=bytes.find(p+i);if(it==bytes.end()){error="missing native byte";return false;}target[i]=it->second;}return true;}
    template<class T>bool read(uintptr_t p,size_t offset,T& out,std::string& error){return copy(p+offset,&out,sizeof out,error);}
};
void reader(){constexpr uintptr_t image=0x10000000,software=0x20000000,design=software+0x98,features=0x30000000,tree=0x40000000,tech_data=0x50000000,record_data=0x60000000;
    const std::array<uintptr_t,2> techs{0x70000000,0x71000000},records{0x72000000,0x73000000};Memory m;
    for(auto [slot,object,ap]:{std::array<uintptr_t,3>{0x13df500,software,0x1715450},{0x13df9e0,design,0x1786ef0},{0x13df3c0,features,0x16ffdd0},{0x13df250,tree,0x16dc400}}){m.put(image+slot,object);m.put(object,image+ap);}
    const auto initial=state();m.put(design+0x170,image+0x16d9f40);m.put(design+0x1c0,image+0x173cd30);m.put(design+0x188,initial.xp);m.put(design+0x1d8,initial.respawn_seconds);m.put(design+0x1dc,uint8_t(0));m.put(features+0x18,initial.features);
    m.put(tree+0x10,tech_data);m.put(tree+0x18,int32_t(2));m.put(tree+0x1c,int32_t(2));m.put(tech_data,techs);m.put(software+0x280,record_data);m.put(software+0x288,int32_t(2));m.put(software+0x28c,int32_t(2));m.put(record_data,records);
    for(size_t i=0;i<2;++i){const std::string key=i?"parties":"pvpduels";m.techs[key]=techs[i];m.names[techs[i]+0x20]=m.names[records[i]+0x38]=key;m.put(techs[i],image+0x1744c40);m.put(techs[i]+0x1e8,records[i]);m.put(records[i],image+0x16e79d0);m.put(records[i]+0x60,techs[i]);m.put(records[i]+0x58,std::array<uint8_t,3>{0,uint8_t(i==0),uint8_t(i==1)});}
    auto read=[&](uintptr_t p,size_t o,auto& out,std::string& e){return m.read(p,o,out,e);};auto copy=[&](uintptr_t p,void* out,size_t n,std::string& e){return m.copy(p,out,n,e);};auto text=[&](uintptr_t p,std::string& out,std::string&){if(!m.names.contains(p))return false;out=m.names.at(p);return true;};auto find=[&](uintptr_t owner,const std::string& key,uintptr_t& out,std::string&){need(owner==tree,"foreign TechTree finder");out=m.techs[key];return true;};
    gr::Pin pin;gr::State actual;std::string error;need(gr::read(read,copy,text,find,image,pin,actual,error)&&actual==initial,error);need(pin.design==software+0x98&&pin.tech==techs&&pin.records==records,"complete local owners missing");
    m.put(image+0x13df9e0,design+8);need(!gr::read(read,copy,text,find,image,pin,actual,error),"stale Design singleton accepted");m.put(image+0x13df9e0,design);
    m.put(records[0]+0x60,techs[1]);need(!gr::read(read,copy,text,find,image,pin,actual,error),"foreign Software TechRecord backlink accepted");m.put(records[0]+0x60,techs[0]);
    m.put(tech_data+8,techs[0]);need(!gr::read(read,copy,text,find,image,pin,actual,error),"duplicate keyed TechTree owner accepted");m.put(tech_data+8,techs[1]);
    m.put(software+0x288,int32_t(3));need(!gr::read(read,copy,text,find,image,pin,actual,error),"unowned record array overflow accepted");m.put(software+0x288,int32_t(2));
    m.put(records[0]+0x58,std::array<uint8_t,3>{0,2,0});need(!gr::read(read,copy,text,find,image,pin,actual,error),"non-Boolean owned technology accepted");
}
void native(){alignas(8)std::array<uint8_t,512> design{};alignas(8)std::array<uint8_t,64> flags{};gr::Pin pin;pin.design=uintptr_t(design.data());pin.features=uintptr_t(flags.data());
    auto request=intent(gr::Command::ApplyXP);std::memcpy(design.data()+0x188,request.before.xp.data(),56);auto result=gr::native::change(0,pin,request);std::array<uint32_t,14> actual{};std::memcpy(actual.data(),design.data()+0x188,56);
    need(result.returned&&!result.exception&&result.stores==14&&actual==request.after.xp,"actual whole native Apply block incomplete");result=gr::native::change(0,pin,request);need(result.exception&&!result.returned,"stale XP before block replayed");
    request=intent(gr::Command::RespawnSeconds);std::memcpy(design.data()+0x1d8,&request.before.respawn_seconds,4);auto count=notifications;result=gr::native::change(uintptr_t(&notify)-0xb63030,pin,request);need(result.returned&&!result.exception&&notifications==count+1&&property=="seconds","actual Respawn seconds store/Notify not once");
    request=intent(gr::Command::RespawnDelay);count=notifications;result=gr::native::change(uintptr_t(&notify)-0xb63030,pin,request);need(result.returned&&!result.exception&&notifications==count+1&&property=="delayForToons"&&design[0x1dc]==1,"actual Respawn Boolean store/Notify not once");
    request=intent(gr::Command::Features);calls.clear();events.clear();result=gr::native::change(uintptr_t(&feature)-0x46fb70,pin,request);
    need(result.returned&&!result.exception&&result.feature_calls==3&&calls==std::vector<int32_t>{0,1},"original complete Features call order lost");need(result.feature_events==2&&events==std::vector<int32_t>{0,1}&&flags[0x18]==1&&flags[0x19]==0,"unchanged false entry lost its native FeatureEnabled event");
    request.before.features=request.after.features={1,0};calls.clear();events.clear();result=gr::native::change(uintptr_t(&feature)-0x46fb70,pin,request);need(result.returned&&!result.exception&&result.feature_events==1&&calls==std::vector<int32_t>{0,1}&&events==std::vector<int32_t>{1},"no-state-change Features command omitted original false event");
}
}
int main()try{domain();reader();native();std::cout<<checks<<" Design rules/current native owner/original Apply and Features event assertions passed\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
