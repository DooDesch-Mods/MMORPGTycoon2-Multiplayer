#include "typed_action/action_session.h"
#define state fixture_shared_native_state
#define main fixture_name_ui_regression
#include "test_character_rename_ui.cpp"
#undef main
unsigned fixture_edit_calls=0;
std::string fixture_edit_seen_token;
std::string native_edit_input_token();
bool execute_edit_ui(UiJob& job,const std::string& token,std::string&){
    ++fixture_edit_calls;job.attempted=true;active_ui_token=token;fixture_edit_seen_token=native_edit_input_token();active_ui_token.clear();return true;
}
#include "character_offered_public_actual.inc"
#undef state
int main()try{
    if(fixture_name_ui_regression())return 1;
    const auto prepare=[](){reset_ui();fixture_controller["pending"]=false;original_go=false;fixture_edit_calls=0;fixture_edit_seen_token.clear();};
    const auto before=checks;std::string error;json result;
    const std::string token="11111111-1111-4111-8111-111111111111",uuid="33333333-3333-4333-8333-333333333333";
    const json args={{"token",token},{"kind",0},{"id",1},{"operation","ShowCostume"}};
    prepare();need(native_edit_ui(args,result,error)&&queued_ui&&result.at("queued"),error);
    need(native_edit_read({{"token",token}},result,error)&&!result.at("input_attempted").get<bool>()&&!result.at("input_returned").get<bool>(),"Queued input receipt claimed execution");
    tick_original_ui();need(!fixture_edit_calls&&queued_ui,"Shared edit queue escaped OriginalGo");
    original_go=true;tick_original_ui();need(fixture_edit_calls==1&&!queued_ui&&fixture_edit_seen_token==token,"Shared edit executor/token was not dispatched inside OriginalGo");
    tick_original_ui();need(fixture_edit_calls==1,"Shared edit queue replayed native input");
    need(native_edit_read({{"token",token}},result,error)&&result.at("input_returned").get<bool>()&&result.at("action_uuid")=="","Returned input invented an ordered UUID");
    need(native_edit_submitted(token,uuid,error)&&native_edit_read({{"token",token}},result,error)&&result.at("action_uuid")==uuid,"Exact held UUID did not link to original input receipt");
    need(!native_edit_submitted(token,uuid,error),"Submitted UI receipt replayed");
    need(!native_edit_ui(args,result,error)&&!queued_ui,"Attempted token was accepted again");
    need(!native_edit_submitted("unknown",uuid,error),"Unowned token accepted a submission");
    prepare();need(native_edit_ui(args,result,error),error);++session.session;original_go=true;tick_original_ui();
    need(!fixture_edit_calls&&!ui_jobs.at(token).attempted&&!ui_jobs.at(token).cause.empty()&&!fault,"Changed bound Context reached original edit executor");
    for(const auto& operation:{"ShowAbility","AbilityState","ApplyAbility"}){
        prepare();auto ability=args;ability["operation"]=operation;ability["slot"]=2;
        need(native_edit_ui(ability,result,error)&&queued_ui,"Actual Ability operation failed its exact schema");
    }
    prepare();auto catalogue=args;catalogue["operation"]="ShowAbility";
    need(native_edit_ui(catalogue,result,error)&&queued_ui,"Offered Ability catalogue required a guessed canonical slot");
    prepare();auto draft=args;draft.update({{"operation","EditAbilityText"},{"slot",2},{"control","ability_name"},{"value","Unsent input"},{"commit",false}});
    need(native_edit_ui(draft,result,error)&&queued_ui,"Original uncommitted text route failed exact queue schema");
    prepare();draft["commit"]=1;need(!native_edit_ui(draft,result,error)&&!queued_ui,"Non-Boolean text commit reached input queue");
    for(const auto& key:{"kind","id","operation","slot"}){
        prepare();auto invalid=args;if(std::string(key)=="slot"){invalid["operation"]="ApplyAbility";invalid["slot"]=true;}else invalid[key]=true;
        need(!native_edit_ui(invalid,result,error)&&!queued_ui&&!fixture_edit_calls,"Malformed Character scalar reached queue");
    }
    prepare();auto invalid=args;invalid["operation"]="HiddenBuff";
    need(!native_edit_ui(invalid,result,error)&&!queued_ui,"Hidden Ability operation reached queue");
    prepare();invalid=args;invalid["pointer"]=123;
    need(!native_edit_ui(invalid,result,error)&&!queued_ui,"Raw field input reached queue");
    need(!native_edit_read({{"token","unknown"}},result,error),"Unknown retained input read succeeded");
    std::cout<<checks-before<<" actual shared Character input queue/UUID/session assertions passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
