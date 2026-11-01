// Controlled queue port; production intent construction and acknowledgement guards stay real.
bool fixture_ordered=false,queue_missing=false,queue_wrong_intent=false;
uint64_t queue_captures=0;
mt2mp::game::general_project::Intent queued_intent;
void reset_project_queue(){fixture_ordered=queue_missing=queue_wrong_intent=false;queue_captures=0;queued_intent={};}
void capture_project(mt2mp::game::general_project::Command command,const std::string& tech_key){
    if(queue_missing)return;
    ++queue_captures;queued_intent={command,{0,0,tech_key,""},command==mt2mp::game::general_project::Command::Workers?1:0};
    if(queue_wrong_intent)queued_intent.source.tech="foreign";
}
namespace general_project_runtime {
json status(){return {{"captures",queue_captures}};}
bool observe_pending_ui(const action_session::Context& context,const action_session::Binding& binding,
    const mt2mp::game::general_project::Intent& intent,uint64_t before,std::string& id,std::string& e){
    id=action;
    return mt2mp::game::tutorial_startingpoint_fixture::queued_project_admission(queue_captures!=0,before,queue_captures,
        action_session::Context{},action_session::Binding{},queued_intent,action,context,binding,intent,e);
}
}
