#include "monster_target_scope.h"
#include <iostream>
#include <stdexcept>
namespace fixture {
namespace owner=mt2mp::game::monster_target_scope;
using Scope=owner::Scope;using Entry=owner::Entry;
const std::string snapshot="11111111-1111-4111-8111-111111111111";
const std::string action="22222222-2222-4222-8222-222222222222";
inline void need(bool ok,const std::string& cause){if(!ok)throw std::runtime_error(cause);}
}
int main()try{using namespace fixture;unsigned checks=0;std::string error,key;Entry actual;
    const auto check=[&](bool ok,const std::string& cause){++checks;need(ok,cause);};
    const Scope world{snapshot,3,4,0x1000};
    for(const bool dungeon:{false,true}){
        auto scope=world;scope.region=0x2000;scope.manager=0x3000;
        scope.scene=dungeon?std::string(64,'b')+":world:building:17/instance/9":"world";
        const Entry record{scope,0x4000,0x5000,true,false};
        const std::vector<std::pair<std::string,Entry>> rows{{action,record}};
        auto incomplete=world;incomplete.root=0;check(!owner::target(incomplete,rows,action,false,actual,error),"missing current world owner accepted");
        check(owner::target(world,rows,action,false,actual,error)&&actual==record,error);
        check(owner::receiver(world,rows,record.pointer,record.uid,key,error)&&key==action,error);
        for(unsigned change=0;change<8;++change){auto changed=rows;auto& row=changed[0].second;
            if(change==0)++row.scope.epoch;if(change==1)++row.scope.generation;if(change==2)++row.scope.root;
            if(change==3)row.scope.snapshot="33333333-3333-4333-8333-333333333333";
            if(change==4)row.confirmed=false;if(change==5)row.retired=true;if(change==6)row.scope.scene="foreign";if(change==7)row.uid=0;
            check(!owner::target(world,changed,action,false,actual,error)&&!error.empty(),"foreign/unconfirmed/replaced target admitted");}
        auto changed=rows;changed.push_back(rows[0]);check(!owner::target(world,changed,action,false,actual,error),"duplicate target accepted");
        changed=rows;changed[0].second.retired=true;
        check(owner::target(world,changed,action,true,actual,error)&&actual.retired,"retained tombstone owner unavailable for absence readback");
        check(!owner::receiver(world,changed,record.pointer,record.uid,key,error),"retired receiver returned as live capture");
        check(!owner::receiver(world,rows,record.pointer,record.uid+1,key,error),"reused receiver with foreign full UID admitted");
        changed=rows;changed.push_back({"33333333-3333-4333-8333-333333333333",record});
        check(!owner::receiver(world,changed,record.pointer,record.uid,key,error),"receiver rekey ambiguity accepted");
    }
    std::cout<<checks<<" actual Monster target current-world/scene/tombstone guards passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
