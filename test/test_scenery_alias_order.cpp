#include "scenery_alias_order.h"
#include <iostream>
#include <stdexcept>
namespace alias=mt2mp::game::scenery_alias_order;
namespace {
unsigned checks=0;
void need(bool value,const std::string& e){++checks;if(!value)throw std::runtime_error(e);}
struct World {
    std::map<uint64_t,std::string> bindings{{1,"old-a"},{2,"old-b"},{3,"old-c"}};
    std::set<uint64_t> alive{2,3},revoked{3};std::vector<std::string> order;
    std::string fail;unsigned records=0;
    bool retire(const std::vector<uint64_t>& ids,std::vector<std::string>& keys,std::string& e){
        order.push_back("retire");if(fail=="retire"){e="original native absence receipt failed";return false;}
        for(auto uid:ids){if(alive.contains(uid)||revoked.contains(uid)){e="native retirement target is live or already retired";return false;}revoked.insert(uid);keys.push_back(bindings.at(uid));}
        if(fail=="wrong keys")keys={"foreign"};if(fail=="duplicate keys")keys.push_back(keys.front());return true;
    }
    bool restore(uint64_t uid,const std::string& key,std::string& e){
        order.push_back("restore");if(fail=="restore"){e="original native restoration metadata differs";return false;}
        if(!alive.contains(uid)||!revoked.contains(uid)||bindings.at(uid)!=key){e="restored alias native UID/tombstone differs";return false;}revoked.erase(uid);return true;
    }
    bool capture(std::string& e){
        ++records;order.push_back("records");for(const auto& [uid,key]:bindings){(void)key;
            if(!revoked.contains(uid)&&!alive.contains(uid)){e="live alias full UID no longer resolves";return false;}}
        if(fail=="records"){e="original complete Record reader failed";return false;}return true;
    }
};
}
int main()try{
    World world;std::string e;alias::Receipt receipt;
    need(!world.capture(e)&&e=="live alias full UID no longer resolves","original stale-live-alias failure was not reproduced");
    auto run=[&](bool history){return alias::capture({{1,"old-a"},{2,"old-b"}},world.alive,history?std::map<uint64_t,std::string>{{3,"old-c"}}:std::map<uint64_t,std::string>{},
        [&](const auto& ids,auto& keys,std::string& why){return world.retire(ids,keys,why);},
        [&](uint64_t uid,const auto& key,std::string& why){return world.restore(uid,key,why);},
        [&](std::string& why){return world.capture(why);},receipt,e);};
    world={};need(run(false)&&world.order==std::vector<std::string>{"retire","records"}&&world.records==1&&receipt.retired==std::vector<std::string>{"old-a"}&&world.revoked.contains(3),e);
    world={};need(run(true)&&world.order==std::vector<std::string>{"retire","restore","records"}&&receipt.restored==std::vector<std::string>{"old-c"}&&!world.revoked.contains(3),e);
    for(const std::string failure:{"retire","wrong keys","duplicate keys","restore"}){
        world={};world.fail=failure;e.clear();need(!run(true)&&world.records==0,"failed alias acknowledgement entered the complete Record reader");
        if(failure=="retire")need(e=="original native absence receipt failed","retirement first cause was replaced");
        if(failure=="restore")need(e=="original native restoration metadata differs","restoration first cause was replaced");
    }
    world={};world.fail="records";need(!run(true)&&e=="original complete Record reader failed"&&world.revoked.contains(1)&&!world.revoked.contains(3),"Record failure hid confirmed native alias effects");
    world={};world.alive={1,2};need(run(false)&&world.order==std::vector<std::string>{"records"}&&receipt.retired.empty(),e);
    world={};world.alive={1,2};need(run(true)&&world.order==std::vector<std::string>{"records"}&&receipt.restored.empty(),e);
    world={};world.alive={2};need(run(true)&&receipt.restored.empty()&&world.revoked.contains(3),"absent intermediate birth was restored");
    std::cout<<checks<<" Scenery native alias/complete Record ordering cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
