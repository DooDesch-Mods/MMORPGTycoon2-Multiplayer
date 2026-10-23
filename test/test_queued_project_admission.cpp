#include "../src/game/tutorial_startingpoint_fixture/queued_project_admission.h"
#include "../src/game/tutorial_startingpoint_fixture/queued_project_expected.h"
#include <iostream>
#include <stdexcept>
int main(){try{
    unsigned checks=0;auto need=[&](bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);};
    using mt2mp::game::tutorial_startingpoint_fixture::queued_project_admission;
    using mt2mp::game::tutorial_startingpoint_fixture::expected_project_intent;
    const std::string id="01234567-89ab-4def-8123-456789abcdef";std::string error;
    auto check=[&](bool present,uint64_t before,uint64_t captures,int context,int binding,int intent,const std::string& uuid){
        return queued_project_admission(present,before,captures,context,binding,intent,uuid,1,2,3,error);};
    need(check(true,4,5,1,2,3,id)&&error.empty(),"fresh exact queued capture rejected");
    need(!check(true,4,4,1,2,3,id),"zero captures accepted");
    need(!check(true,4,6,1,2,3,id),"double capture accepted");
    need(!check(false,4,5,1,2,3,id),"missing queue accepted");
    need(!check(true,4,5,9,2,3,id),"wrong Context accepted");
    need(!check(true,4,5,1,9,3,id),"wrong Binding accepted");
    need(!check(true,4,5,1,2,9,id),"wrong command/source/workers accepted");
    need(!check(true,4,5,1,2,3,""),"missing action identity accepted");
    need(!check(true,UINT64_MAX,0,1,2,3,id),"counter overflow accepted");
    need(check(true,0,1,1,2,3,id)&&error.empty(),"previous cause not cleared on fresh success");
    using namespace mt2mp::game;
    tutorial_development::StartingpointSnapshot snapshot;
    snapshot.creation.emplace();snapshot.creation->source=0x1000;
    snapshot.creation->catalogue={{0x1000,0,10,"startingpoint",""}};
    general_project::Intent expected;
    using Command=general_project::Command;
    need(expected_project_intent(snapshot,"startingpoint",Command::Start,expected,error),"native Start source rejected");
    need(expected.source.statistic==10&&expected.source.art.empty()&&expected.workers==0,"observed native Source sentinel10 lost");
    auto captured=expected;
    need(queued_project_admission(true,4,5,1,2,captured,id,1,2,expected,error),"exact full native Start acknowledgement rejected");
    general_project::Intent synthesized;synthesized.source.tech="startingpoint";
    need(!queued_project_admission(true,4,5,1,2,captured,id,1,2,synthesized,error),"old default Source unexpectedly matches native Source");
    need(error=="General project UI acknowledgement Context/Binding/intent differs","original mismatch cause changed");
    snapshot.creation->catalogue.front().statistic=-1;
    snapshot.creation->catalogue.front().art="native-inactive-art";
    need(expected_project_intent(snapshot,"startingpoint",Command::Start,expected,error)&&expected.source.statistic==-1&&expected.source.art=="native-inactive-art","native inactive fields replaced");
    snapshot.project=0x2000;auto current=snapshot.creation->catalogue.front();current.address=snapshot.project;
    current.statistic=-7;current.art="native-current-art";snapshot.creation->current=current;
    need(expected_project_intent(snapshot,"startingpoint",Command::Workers,expected,error),"native owning clone rejected");
    need(expected.source.statistic==-7&&expected.source.art=="native-current-art"&&expected.workers==1,"Workers used catalogue/default instead of current clone");
    captured=expected;
    need(queued_project_admission(true,7,8,1,2,captured,id,1,2,expected,error),"exact full Workers acknowledgement rejected");
    ++captured.source.statistic;
    need(!queued_project_admission(true,7,8,1,2,captured,id,1,2,expected,error),"changed inactive native statistic accepted");
    need(!queued_project_admission(true,7,9,1,2,expected,id,1,2,expected,error),"full native Source bypassed capture+1 guard");
    need(!queued_project_admission(true,7,8,9,2,expected,id,1,2,expected,error),"full native Source bypassed Context guard");
    need(!queued_project_admission(true,7,8,1,9,expected,id,1,2,expected,error),"full native Source bypassed Binding guard");
    snapshot.creation->current->address=0x3000;
    need(!expected_project_intent(snapshot,"startingpoint",Command::Workers,expected,error),"foreign current clone accepted");
    snapshot.creation->current=current;snapshot.project=0x1000;snapshot.creation->current->address=snapshot.project;
    need(!expected_project_intent(snapshot,"startingpoint",Command::Workers,expected,error),"catalogue/current alias accepted");
    snapshot.creation->catalogue.push_back(snapshot.creation->catalogue.front());
    need(!expected_project_intent(snapshot,"startingpoint",Command::Start,expected,error),"duplicate selected catalogue source accepted");
    snapshot.creation->catalogue.pop_back();
    need(!expected_project_intent(snapshot,"bridges",Command::Start,expected,error),"foreign Tech source accepted");
    snapshot.creation.reset();
    need(!expected_project_intent(snapshot,"startingpoint",Command::Start,expected,error),"missing native owners accepted");
    std::cout<<checks<<" queued-project checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
