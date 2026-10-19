#include "building_status_fixture.h"
#include <iostream>
int main()try{size_t checks=0;std::string error;std::vector<uint8_t> bytes;Message decoded;const auto check=[&](bool ok,const char* cause){++checks;need(ok,cause);};
    for(bool dungeon:{false,true})for(int32_t state=0;state<3;++state)for(int32_t old=0;old<3;++old){desired=state;prior=old;is_dungeon=dungeon;
        for(const auto& message:{request(),result()}){check(encode(message,bytes,error),error.c_str());check(decode(bytes.data(),bytes.size(),decoded,error)&&decoded==message,"Building status complete schema35 roundtrip differs");}
        const auto invalid=[&](auto mutate){auto message=result();mutate(message);check(!encode(message,bytes,error),"Building status accepted incorrect native state/side effect/domain");};
        invalid([](auto& m){m.request.building_status.before=-1;});invalid([](auto& m){m.request.building_status.state=3;});invalid([](auto& m){m.request.building_status.key="bad";});
        invalid([](auto& m){m.evidence.building_status_after.state=(desired+1)%3;});invalid([](auto& m){m.evidence.building_status_after.user_set=false;});
        invalid([](auto& m){m.evidence.building_status_after.bandwidth+=.5f;});invalid([](auto& m){m.evidence.building_status_after.bugs_sha256=std::string(64,'e');});
        invalid([](auto& m){++m.evidence.building_status_after.jobs;});invalid([](auto& m){++m.evidence.building_status_after.linked;});invalid([](auto& m){++m.evidence.building_status_after.bugs;});
        invalid([](auto& m){++m.evidence.building_status_after.developers;});invalid([](auto& m){++m.evidence.building_status_after.undo_count;});
        invalid([](auto& m){++m.evidence.after.cash;});invalid([](auto& m){m.request.ability_edit.id=1;});invalid([](auto& m){m.evidence.ability_edit_before.count=1;});
        if(desired==2)invalid([](auto& m){m.evidence.building_status_before.construction=m.evidence.building_status_after.construction=1;});
        if(dungeon&&desired)invalid([](auto& m){m.evidence.building_status_before.dungeon_ready=m.evidence.building_status_after.dungeon_ready=false;});
        if(!dungeon)invalid([](auto& m){m.request.building_status.dungeon_ui=true;});
        if(dungeon&&desired==2){auto message=result();message.request.building_status.dungeon_ui=true;message.evidence.building_status_before.construction=message.evidence.building_status_after.construction=1;
            check(encode(message,bytes,error),"DungeonStatusWindow incorrectly inherited the BuildingInfoWindow state2 guard");}
    }
    std::cout<<checks<<" Building/Dungeon state/Bug/Job/ledger/Undo checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
