#include "character_membership_npc_completion.h"
#include <iostream>
#include <map>
namespace completion=mt2mp::game::character_membership_npc_completion;
struct Port {
    std::vector<completion::Pool> pools;std::set<uint64_t> marked_uids;unsigned calls=0;int mode=0;std::vector<uint64_t> order;
    bool read(const completion::Pool& expected,completion::Pool& out,std::string& error){
        if(mode==1){error="original current NPC pool failure";return false;}
        for(const auto& pool:pools)if(pool.region==expected.region){out=pool;if(mode==2)out.manager++;if(mode==6&&calls)out.next_tag++;return true;}
        error="original NPC Region absent";return false;
    }
    bool marked(const completion::Pool&,const completion::Member& member,bool& value,std::string& error){
        if(mode==3){error="original current NPC pending flag failure";return false;}value=marked_uids.contains(member.uid);return true;
    }
    bool remove(const completion::Pool& expected,const completion::Member& member,bool& attempted,std::string& error){
        attempted=true;++calls;order.push_back(member.uid);if(mode==4){error="original RemoveNPC SEH";return false;}
        for(auto& pool:pools)if(pool.region==expected.region){const auto position=std::find(pool.rows.begin(),pool.rows.end(),member);
            const auto tail=std::find_if(pool.rows.rbegin(),pool.rows.rend(),[&](const auto& value){return value.type==member.type;});
            if(position==pool.rows.end()||tail==pool.rows.rend()){error="original NPC type member absent";return false;}
            const auto index=size_t(position-pool.rows.begin()),last=size_t(pool.rows.rend()-tail-1);
            pool.rows[index]=pool.rows[last];pool.rows.erase(pool.rows.begin()+ptrdiff_t(last));return true;}
        error="original NPC manager absent";return false;
    }
    bool registered(uint64_t uid,bool& value,std::string& error){if(mode==5){error="original NPC UID lookup failure";return false;}value=mode==7;
        for(const auto& pool:pools)for(const auto& row:pool.rows)value=value||row.uid==uid;return true;}
};
int main()try{
    unsigned checks=0;std::string error;const auto check=[&](bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);};
    const std::vector<completion::Pool> initial{{10,20,50,{{100,0x100000001,0},{101,0x200000001,0},{102,0x300000001,0},{103,0x400000001,1}}},
        {11,21,51,{{104,0x500000001,2},{105,0x600000001,2}}}};
    const std::vector<completion::Member> targets{initial[0].rows[1],initial[1].rows[0],initial[0].rows[0]};
    const auto setup=[&]{Port port;port.pools=initial;for(const auto& row:targets)port.marked_uids.insert(row.uid);return port;};
    auto port=setup();completion::Receipt receipt;check(completion::finish(port,initial,targets,receipt,error),error);
    check(receipt.confirmed&&receipt.attempted&&receipt.calls==3&&receipt.returned==3&&port.calls==3,"Native pending NPC completion did not retain exact original call/return counts");
    check(port.order==std::vector<uint64_t>{0x100000001,0x200000001,0x500000001},"Deferred NPC completion used definition/request order instead of original manager type-array order");
    check(port.pools[0].rows==std::vector<completion::Member>{initial[0].rows[2],initial[0].rows[3]}&&port.pools[1].rows==std::vector<completion::Member>{initial[1].rows[1]},"Original swap-last survivor membership/order differs");
    for(int mode=1;mode<=7;++mode){port=setup();port.mode=mode;error.clear();
        check(!completion::finish(port,initial,targets,receipt,error)&&!receipt.confirmed&&!error.empty(),"Incomplete original deferred NPC removal claimed success mode="+std::to_string(mode));
        if(mode<=3)check(!receipt.attempted&&!port.calls,"NPC completion mutated before complete current pool/flag admission");
        if(mode==1)check(error=="original current NPC pool failure","Original native pool cause replaced");
        if(mode==3)check(error=="original current NPC pending flag failure","Original pending flag cause replaced");
        if(mode==4)check(receipt.attempted&&receipt.calls==1&&!receipt.returned,"Failed native removal was reported as unattempted or returned");
        if(mode>=5)check(receipt.attempted&&receipt.returned==1,"Post-effect failure lost actual original completion receipt");
    }
    port=setup();port.marked_uids.erase(targets[1].uid);check(!completion::finish(port,initial,targets,receipt,error)&&!port.calls,"A not-yet-marked NPC was synthetically destroyed");
    for(int mode=0;mode<5;++mode){port=setup();auto wrong=targets;
        if(mode==0)wrong.push_back(targets[0]);if(mode==1)wrong[0].uid=0x900000001;if(mode==2)wrong[0].pointer++;if(mode==3)wrong[0].type=3;if(mode==4)wrong[0].uid=0;
        check(!completion::finish(port,initial,wrong,receipt,error)&&!port.calls,"Foreign, repeated, truncated or absent original NPC target mutated");}
    port=setup();check(completion::finish(port,{}, {},receipt,error)&&receipt.confirmed&&!receipt.attempted&&!port.calls,"No affected NPCs required synthetic work");
    std::cout<<checks<<" original deferred NPC destruction admission, original order, exact swap-last completion and failure receipt comparisons passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
