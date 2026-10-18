#include "vendor/nlohmann/json.hpp"
#include "monster_canonical.h"
#include <iostream>
#include <stdexcept>
namespace c=mt2mp::game::monster_canonical;
namespace d=mt2mp::game::direct_monster_identity;
namespace z=mt2mp::game::monster_zone_read;
using json=nlohmann::json;
void need(bool v,const std::string& e){if(!v)throw std::runtime_error(e);}
constexpr const char* id="11111111-1111-4111-8111-111111111111";
constexpr const char* other_action="33333333-3333-4333-8333-333333333333";
d::Scope owner(){return {"22222222-2222-4222-8222-222222222222",1,2,0x1000,0x2000,0x3000};}
json normalize(bool client){d::registry={};auto scope=owner();if(client){scope.root+=0x10000;scope.region+=0x10000;scope.manager+=0x10000;}
 const uintptr_t base=client?0x10000:0;z::State s;z::Zone zone;zone.value.pointer=base+0x4000;zone.value.uid=client?0x100000005ull:5;zone.monsters={client?0x100000006ull:6};s.zones.push_back(zone);
 z::Monster member;member.pointer=base+0x5000;member.uid=zone.monsters[0];member.zone=zone.value.pointer;s.monsters.push_back(member);
 z::Monster direct;direct.pointer=base+0x6000;direct.uid=client?0x200000007ull:7;s.monsters.push_back(direct);
 json content={{"zones",json::array({{{"local_uid",zone.value.uid},{"monster_local_uids",zone.monsters}}})},
 {"monsters",json::array({{{"local_uid",member.uid},{"zone_local_uid",zone.value.uid}},{{"local_uid",direct.uid},{"zone_local_uid",0}}})}};
 const auto original=content;std::string e;unsigned verified=0,named=0;
 auto name=[&](uintptr_t p,std::string& out,std::string&){need(p==direct.pointer,"name called for Zone member");out="Deathblood";++named;return true;};
 auto verify=[&](const z::Monster& row,std::string&){++verified;return row.pointer==member.pointer||row.pointer==direct.pointer;};
 need(!c::normalize(s,{{zone.value.uid,"saved:zone:1"}},scope,content,name,verify,e)&&e.find("saved direct admission unsupported")!=std::string::npos,"unbound saved direct silently admitted");
 need(d::registry.stage(id,scope,direct.pointer,direct.uid,e),e);std::string key;
 need(!d::registry.resolve(scope,direct.pointer,direct.uid,key,e),"unconfirmed birth admitted");
 need(!d::registry.resolve(scope,direct.pointer,direct.uid,key,e,other_action),"other_action pending UUID admitted");
 content=original;need(c::normalize(s,{{zone.value.uid,"saved:zone:1"}},scope,content,name,verify,e,id),e);
 need(content["monsters"][0]["key"]=="saved:zone:1:monster:0"&&content["monsters"][0]["zone"]=="saved:zone:1","Zone member key changed");
 need(content["monsters"][1]["key"]==std::string("action:")+id&&content["monsters"][1]["zone"].is_null()&&named==1,"direct stable UUID/name missing");
 need(d::registry.confirm(id,e),e);need(!d::registry.confirm(id,e),"identity confirmation replayed");
 need(!d::registry.stage(id,scope,direct.pointer+1,direct.uid+1,e),"UUID rebound");
 need(!d::registry.stage(other_action,scope,direct.pointer,direct.uid+1,e),"pointer reused");
 need(!d::registry.stage(other_action,scope,direct.pointer+1,direct.uid,e),"full UID reused");
 need(!d::registry.resolve(scope,direct.pointer,direct.uid+(1ull<<32),key,e),"low32 UID alias accepted");
 for(unsigned field=0;field<6;++field){auto stale=scope;switch(field){case 0:stale.snapshot="stale";break;case 1:++stale.epoch;break;case 2:++stale.generation;break;case 3:++stale.root;break;case 4:++stale.region;break;default:++stale.manager;}
  need(!d::registry.resolve(stale,direct.pointer,direct.uid,key,e),"stale world/manager identity admitted");}
 auto bad=original;auto reject=[](const z::Monster&,std::string& cause){cause="full UID registry mismatch";return false;};
 need(!c::normalize(s,{{zone.value.uid,"saved:zone:1"}},scope,bad,name,reject,e)&&e=="full UID registry mismatch","full UID failure ignored");
 need(verified>=4,"full UID verifier did not cover both families");
 bad=original;need(c::normalize(s,{{zone.value.uid,"saved:zone:1"}},scope,bad,name,verify,e),e);need(bad==content,"confirmation changed digest");
 return content;
}
int main(){try{auto host=normalize(false),client=normalize(true);need(host==client,"native pointer/full UID leaked into mixed canonical digest");
 std::string e;d::Registry r;need(!r.stage(std::string(36,'x'),owner(),1,2,e),"malformed UUID admitted");
 std::cout<<"Mixed Zone/direct actual normalizer and full scoped identity cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
