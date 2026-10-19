#include "dungeon_zone_cascade.h"
#include <iostream>
#include <stdexcept>
namespace q=mt2mp::game::npc_quest_targets;
namespace cascade=mt2mp::game::dungeon_zone_cascade;
int main()try{
    unsigned checks=0;const auto check=[&](bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);};
    std::string error;q::SavedPool before,after;before.origin.sha256=std::string(64,'a');before.saved_region=67;before.array.pointer=100;
    q::Zone first,second;first.pointer=200;first.uid=0x100000007;first.saved_child=0;first.link_id=1;second.pointer=300;second.uid=0x200000008;second.saved_child=1;second.link_id=2;
    before.array.zones={first,second};before.action_keys={{200,"original-first"},{300,"original-second"}};
    auto actual=before.array;actual.zones.erase(actual.zones.begin());actual.zones[0].saved_child=0;actual.zones[0].content.sections={{{1,2,3},{4,5,6}}};
    auto find=[&](uint64_t uid,uintptr_t& p,std::string&){p=uid==second.uid?second.pointer:0;return true;};
    check(cascade::reconcile(before,actual,find,after,error),error);
    check(after.array==actual&&after.action_keys.size()==1&&q::zone_key(after,after.array.zones[0])=="original-second","Original Zone canonical key changed with native child order or cull geometry");
    for(int mode=0;mode<6;++mode){auto bad=actual;if(mode==0)bad.pointer++;if(mode==1)bad.zones[0].uid^=uint64_t(1)<<32;
        if(mode==2)bad.zones[0].pointer++;if(mode==3)bad.zones[0].link_id++;if(mode==4)bad.zones.push_back(bad.zones[0]);if(mode==5)bad.zones.push_back(first);
        check(!cascade::reconcile(before,bad,find,after,error)&&!error.empty(),"Foreign, unretired or duplicate original Zone cascade accepted");}
    const auto still_live=[&](uint64_t uid,uintptr_t& p,std::string&){p=uid==second.uid?second.pointer:first.pointer;return true;};
    check(!cascade::reconcile(before,actual,still_live,after,error)&&error.find("remains registered")!=std::string::npos,"Queued original Zone deletion falsely confirmed");
    const auto failed=[](uint64_t,uintptr_t&,std::string& e){e="original Zone lookup failure";return false;};
    check(!cascade::reconcile(before,actual,failed,after,error)&&error=="original Zone lookup failure","Original UID lookup cause was replaced");
    actual.zones.clear();const auto absent=[](uint64_t,uintptr_t& p,std::string&){p=0;return true;};
    check(cascade::reconcile(before,actual,absent,after,error)&&after.array.zones.empty()&&after.action_keys.empty(),"Complete original Zone retirement was rejected");
    auto retired=after;auto restored=before.array;restored.zones[0].pointer=400;restored.zones[1].pointer=500;
    const auto original_restore=[&](uint64_t uid,uintptr_t& p,std::string&){p=uid==first.uid?400:uid==second.uid?500:0;return true;};
    const std::set<uint64_t> restored_uids{first.uid,second.uid};
    check(cascade::reconcile_restored(retired,before,restored,restored_uids,original_restore,after,error),error);
    check(q::zone_key(after,after.array.zones[0])=="original-first"&&q::zone_key(after,after.array.zones[1])=="original-second","Native MakeZone new pointers changed original canonical keys");
    for(int mode=0;mode<5;++mode){auto invalid=restored;auto authority=restored_uids;auto origin=before;
        if(mode==0)authority.erase(first.uid);if(mode==1)invalid.zones[0].uid^=uint64_t(1)<<32;
        if(mode==2)invalid.zones[0].pointer++;if(mode==3)invalid.zones.pop_back();if(mode==4)origin.origin.sha256=std::string(64,'f');
        check(!cascade::reconcile_restored(retired,origin,invalid,authority,original_restore,after,error)&&!error.empty(),"Incomplete original Make/Edit Zone authority accepted");}
    std::cout<<checks<<" original Dungeon Zone cull geometry, canonical keys and full-UID retirement comparisons passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
