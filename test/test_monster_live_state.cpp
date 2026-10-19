#include "live_host_state/state.h"
#include "live_host_state/monster_apply.h"
#include "direct_monster_identity.h"
#include <bit>
#include <iostream>
#include <stdexcept>
namespace l=mt2mp::game::live_host_state;
namespace m=l::monsters;
namespace identity=mt2mp::game::direct_monster_identity;
namespace {
size_t checks=0;
void need(bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);}
std::string id(char digit){return std::string(8,digit)+"-1111-4111-8111-111111111111";}
m::Row row(char digit='1'){m::Row r;r.birth=id(digit);r.name="Gnashgore";r.rank=1;r.slot=1;r.type=0;r.level=1;r.tag=4;
    r.pose.region=63;r.pose.orientation={0,0,0,1};r.pose.position={8020,70,8550};r.pose.scale={1,1,1};r.spawn=r.pose.position;r.on_map=true;
    for(auto& length:r.animation_length)length=std::bit_cast<uint32_t>(2.0f);r.animation[0]=std::bit_cast<uint32_t>(1.0f);return r;}
m::State state(){return {{row()},{}};}
l::Frame frame(){l::Frame f;f.session=41;f.epoch=7;f.source_generation=9;f.revision=3;f.snapshot=id('3');f.selected=1;f.pause=1;
    f.values.calendar={1,1,3,2,11819,7};f.values.cash=88400;f.values.record={2,88400,0,0,0};f.monsters=state();return f;}
struct Calls {
    m::State actual=state(),before,after;bool witnessed=false;int read_count=0,set_count=0,fail_read=0,mode=0;
    void observed(const m::State& first,const m::State& final){before=first;after=final;witnessed=true;}
    bool read(m::State& out,std::string& e){++read_count;if(read_count==fail_read){e="retained read failure";return false;}out=actual;return true;}
    bool set(const m::Row&,const m::Row& target,bool& attempted,std::string& e){++set_count;if(mode==1){e="before native";return false;}
        attempted=true;if(mode==2){e="native failed";return false;}if(mode==3)throw std::runtime_error("native threw");
        auto& r=actual.rows.at(size_t(set_count-1));const auto local_tag=r.tag;r=target;r.tag=local_tag;
        if(mode==4)r.spawn[0]+=1;if(mode==5)actual.rows.at(1).pose.position[0]+=1;return true;}
};
struct Spatial {
    std::array<uint8_t,4> bytes{7,1,8,9};int updates=0,cas=0,mode=0;
    bool read_byte(uintptr_t,uint8_t& value,std::string& e){if(mode==1){e="read failed";return false;}value=bytes[1];return true;}
    bool update_spatial(uintptr_t,std::string& e){++updates;if(mode==2){e="quadtree failed";return false;}if(mode==3)bytes[1]=0;return true;}
    bool compare_byte(uintptr_t,uint8_t expected,uint8_t target,std::string& e){++cas;if(bytes[1]!=expected){e="CAS failed";return false;}bytes[1]=target;return true;}
};
}
int main(){try{
    std::string e;auto s=state();auto encoded=m::encode(s);m::State decoded;
    need(m::decode(encoded,decoded,e)&&decoded==s,"component exact roundtrip: "+e);
    for(int field=0;field<17;++field){auto bad=encoded;auto& r=bad["rows"][0];switch(field){
        case 0:r["birth"]="foreign";break;case 1:r["name"]="";break;case 2:r["tag"]=int64_t(INT32_MIN)-1;break;case 3:r["rank"]=uint64_t(UINT32_MAX);break;
        case 4:r["level"]=1.5;break;case 5:r["on_map"]=1;break;case 6:r["animation"][0]=-1;break;case 7:r["animation_length"][0]=0;break;
        case 8:r["animation"][0]=std::bit_cast<uint32_t>(NAN);break;case 9:r["host_attribute_values"][0]=std::bit_cast<uint32_t>(INFINITY);break;
        case 10:r["host_target_state"]="mapped";r["host_target_key"]="subscriber:01";break;case 11:r["extra"]=1;break;
        case 12:bad["rows"].push_back(r);break;case 13:bad["retired"].push_back(id('1'));break;case 14:r["spawn"]={1,2};break;
        case 15:r["rotation"]={0,0,0,0};break;case 16:r["animation_length"][0]=nullptr;break;}
        need(!m::decode(bad,decoded,e)&&decoded.rows.empty(),"malformed component accepted "+std::to_string(field));}
    s.rows[0].host_combat.target={"mapped","action:"+id('2')};s.rows[0].host_combat.aggro={"unmapped",""};
    need(m::valid(s,e),"explicit Host observations rejected: "+e);
    auto f=frame();std::vector<uint8_t> bytes;l::Frame out;
    need(l::encode(f,bytes,e)&&nlohmann::json::parse(bytes).at("schema")==18&&l::decode(bytes.data(),bytes.size(),out,e)&&out==f,"real HostFrame18 roundtrip: "+e);
    f.monsters.reset();need(l::encode(f,bytes,e)&&nlohmann::json::parse(bytes).at("schema")==12&&l::decode(bytes.data(),bytes.size(),out,e)&&out==f,"prior Frame12 changed: "+e);
    f=frame();f.marketing=mt2mp::game::marketing_authority::State{{{"awards",nlohmann::json::array()},{"campaigns",nlohmann::json::array()},{"positiveBuzz",0},{"negativeBuzz",0}}};
    need(l::encode(f,bytes,e)&&l::decode(bytes.data(),bytes.size(),out,e)&&out==f,"Monster+Marketing Frame18: "+e);
    auto target=state();target.rows[0].pose.position[0]+=3;target.rows[0].animation[0]=std::bit_cast<uint32_t>(1.25f);
    need(m::preflight(state(),target,e),"motion with immutable spawn rejected: "+e);
    for(int field=0;field<8;++field){auto bad=target;switch(field){case 0:bad.rows[0].birth=id('2');break;case 1:bad.rows[0].spawn[0]+=1;break;case 2:bad.rows[0].pose.region++;break;
        case 3:bad.rows.clear();break;case 4:bad.retired.push_back(id('2'));break;case 5:bad.rows[0].host_combat.maxima[0]=std::bit_cast<uint32_t>(20.0f);break;case 6:bad.rows[0].lifecycle.action=49;break;case 7:bad.rows[0].animation_length[0]=std::bit_cast<uint32_t>(3.0f);break;}
        need(!m::preflight(state(),bad,e),"unsupported lifecycle/identity silently adopted "+std::to_string(field));}
    auto observed=target;observed.rows[0].host_combat.values[0]=std::bit_cast<uint32_t>(123.0f);
    need(!m::adopted(target,observed,e),"changed health skipped final adoption");
    Calls good;auto receipt=m::apply_motion(good,target);need(receipt.confirmed&&receipt.attempted&&!receipt.unknown&&good.set_count==1&&good.witnessed&&good.before==state()&&good.after==target,"actual motion apply/readback failed: "+receipt.cause);
    receipt=m::apply_motion(good,target);need(receipt.confirmed&&!receipt.attempted&&good.set_count==1,"same phase replayed native work");
    for(int mode=1;mode<5;++mode){Calls c;c.mode=mode;receipt=m::apply_motion(c,target);need(!receipt.confirmed&&receipt.unknown==(mode>1)&&receipt.attempted==(mode>1)&&c.set_count==1,"failed native attempt lost no-replay state "+std::to_string(mode));}
    Calls changed;changed.actual.rows.push_back(row('2'));auto two=changed.actual;two.rows[0].pose.position[0]+=1;two.rows[1].pose.position[0]+=1;changed.mode=5;
    receipt=m::apply_motion(changed,two);need(receipt.unknown&&changed.set_count==1,"later current row changed without rejection");
    Calls serials;serials.actual.rows[0].tag=-11752269;
    receipt=m::apply_motion(serials,target);need(receipt.confirmed&&serials.actual.rows[0].tag==-11752269&&m::adopted(serials.after,target,e),"motion copied or compared remote local serial");
    receipt=m::apply_motion(serials,target);need(receipt.confirmed&&!receipt.attempted&&serials.set_count==1,"local serial difference replayed native apply");
    Calls missing;missing.fail_read=2;receipt=m::apply_motion(missing,target);need(!receipt.attempted&&!receipt.confirmed&&receipt.cause=="retained read failure","pre-call failure looked attempted");
    Spatial spatial;bool attempted=false;need(m::clear_spatial_dirty(spatial,100,attempted,e)&&attempted&&spatial.updates==1&&spatial.cas==1&&spatial.bytes==std::array<uint8_t,4>{7,0,8,9},"byte CAS damaged adjacent flags");
    attempted=false;need(m::clear_spatial_dirty(spatial,100,attempted,e)&&!attempted&&spatial.updates==1,"clean spatial state replayed update");
    for(int mode=1;mode<4;++mode){Spatial failed;failed.mode=mode;attempted=false;need(!m::clear_spatial_dirty(failed,100,attempted,e)&&attempted==(mode>1),"spatial failure attempt differs");}
    Spatial domain;domain.bytes[1]=2;attempted=false;need(!m::clear_spatial_dirty(domain,100,attempted,e)&&!attempted,"dirty domain coerced");
    identity::Registry registry;identity::Scope scope{id('3'),7,9,100,200,300};std::vector<std::pair<std::string,identity::Entry>> entries;
    need(registry.stage(id('1'),scope,400,UINT64_C(0x100000001),e)&&!registry.enumerate(id('3'),7,9,100,entries,e)&&entries.empty(),"pending birth silently skipped");
    need(registry.confirm(id('1'),e)&&registry.enumerate(id('3'),7,9,100,entries,e)&&entries.size()==1&&!entries[0].second.retired,"confirmed full UID census missing");
    const auto entry=entries[0].second;need(registry.retire(id('1'),entry,e)&&registry.enumerate(id('3'),7,9,100,entries,e)&&entries.size()==1&&entries[0].second.retired,"tombstone lost from complete census");
    need(registry.enumerate(id('3'),7,10,100,entries,e)&&entries.empty(),"foreign generation leaked");
    std::cout<<checks<<" Monster frame/motion/CAS/identity checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
