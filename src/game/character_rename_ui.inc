namespace {
struct UiJob {a::Context context;a::Binding binding;json arguments,result=json::object();
    bool attempted=false,returned=false,waiting=false;unsigned stage=0,waits=0;
    OrderedView input_view;a::CharacterRenameIntent input_intent;json open_proof=json::object();std::string action_uuid,cause;};
std::map<std::string,UiJob> ui_jobs;std::optional<std::string> queued_ui;
bool diagnostic_pending(){return queued_ui.has_value();}
void diagnostic_retire_lobby(){queued_ui.reset();ui_jobs.clear();active_ui_token.clear();}
json diagnostic_status(){json result=json::object();for(const auto& [token,job]:ui_jobs)
    result[token]={{"input_attempted",job.attempted},{"input_returned",job.returned},{"action_uuid",job.action_uuid},
        {"cause",job.cause},{"native",job.result},{"arguments",job.arguments},{"stage",job.stage},{"waiting_for_ui",job.waiting}};return result;}
bool diagnostic_submitted(const std::string& token,const std::string& id,std::string& error){
    const auto found=ui_jobs.find(token);
    if(found==ui_jobs.end()||!found->second.attempted||!found->second.returned||!found->second.action_uuid.empty())
        return fail(error,"Character rename diagnostic lacks its exact original input receipt");
    found->second.action_uuid=id;return true;
}
bool offered_control(Memory& memory,uintptr_t pane,uintptr_t outer,std::string& error){
    uint8_t shown=0;std::set<uintptr_t> seen;
    while(pane!=outer){uintptr_t owner=0,parent=0;
        if(!pane||!seen.insert(pane).second||seen.size()>64||!field(memory,pane,0x69,shown,error)||shown!=1||
           !field(memory,pane,0x280,owner,error)||owner!=outer||!field(memory,pane,0x278,parent,error))
            return fail(error,error.empty()?"Character rename original offered control is hidden or has a different owner":error);
        pane=parent;
    }
    return field(memory,outer,0x45f,shown,error)&&(shown==1||fail(error,"Character rename requires the actual visible Character window"));
}
#include "character_rename_open.inc"
bool rename_ancestry(Memory& memory,uintptr_t pane,const OrderedView& view,json& result,std::string& error){
    std::set<uintptr_t> seen;bool reached_view=false;result=json::array();
    while(pane!=view.outer){uintptr_t parent=0,owner=0,ap=0;uint8_t shown=0;std::string name;
        if(!pane||!seen.insert(pane).second||seen.size()>64)return fail(error,"Character Rename pencil ancestry is absent or cyclic pane="+std::to_string(pane));
        if(!field(memory,pane,0,ap,error)||!field(memory,pane,0x278,parent,error)||!field(memory,pane,0x280,owner,error)||
           !field(memory,pane,0x69,shown,error)||!npc_quest_records::read_string(memory,pane,0x1b0,"Character Rename ancestor id",name,error))return false;
        result.push_back({{"pane",pane},{"type_rva",ap-state.image},{"id",name},{"parent",parent},{"window",owner},{"shown",shown}});
        if(owner!=view.outer||shown!=1)return fail(error,"Character Rename pencil ancestor is not offered: "+result.back().dump()+" expected_window="+std::to_string(view.outer));
        reached_view|=pane==view.view;pane=parent;
    }
    uint8_t shown=0;if(!field(memory,view.outer,0x45f,shown,error))return false;
    return (reached_view&&shown==1)||fail(error,"Character Rename pencil live chain differs reached_view="+std::to_string(reached_view)+" window_shown="+std::to_string(shown));
}
bool offered_rename_button(Memory& memory,const OrderedView& view,uintptr_t& button,json& result,std::string& error){
    struct Content {uintptr_t pane,container;size_t offset;};
    button=0;std::vector<Content> panes{{view.view,0,0}};std::set<uintptr_t> seen;
    result["rename_control_tree"]=json::array();
    while(!panes.empty()){const auto content=panes.back();const auto pane=content.pane;panes.pop_back();uintptr_t ap=0,gather=0,parent=0,owner=0;uint8_t shown=0;std::string name;
        if(!seen.insert(pane).second||seen.size()>2048||!field(memory,pane,0,ap,error)||!field(memory,ap,0x178,gather,error))
            return fail(error,error.empty()?"Character Rename content tree is cyclic, oversized or unreadable pane="+std::to_string(pane):error);
        if(!field(memory,pane,0x278,parent,error)||!field(memory,pane,0x280,owner,error)||!field(memory,pane,0x69,shown,error)||
           !npc_quest_records::read_string(memory,pane,0x1b0,"Character Rename content id",name,error))return false;
        result["rename_control_tree"].push_back({{"pane",pane},{"type_rva",ap-state.image},{"id",name},{"parent",parent},{"window",owner},
            {"shown",shown},{"container",content.container},{"array_offset",content.offset},{"parent_is_container",parent==content.container}});
        if(ap==state.image+0x16e3ec0){std::string command;
            if(!npc_quest_records::read_string(memory,pane,0x388,"Character Rename ButtonPane command",command,error))return false;
            if(command=="MessageTo Parent BeginRename"){
                uintptr_t bound=0;uint8_t disabled=1,held=1,pressed=1,clickable=0;
                if(button||!rename_ancestry(memory,pane,view,result["rename_pencil_ancestry"],error)||
                   !field(memory,pane,0x578,bound,error)||bound||!field(memory,pane,0x584,held,error)||held||
                   !field(memory,pane,0x585,pressed,error)||pressed||!field(memory,pane,0x46d,disabled,error)||disabled||
                   !field(memory,pane,0x174,clickable,error)||clickable!=1)
                    return fail(error,error.empty()?"Character actual Rename pencil is hidden, disabled, bound or ambiguous":error);
                button=pane;
            }
        }
        if(gather==state.image+0xc35730)continue;
        if(gather!=state.image+0x8167c0)return fail(error,"Character Rename GatherAllContents differs pane="+std::to_string(pane)+" type_rva="+std::to_string(ap-state.image)+" gather_rva="+std::to_string(gather-state.image));
        for(const auto offset:{size_t(0x2c8),size_t(0x2f8)}){std::vector<uintptr_t> children;
            if(!open_array(memory,pane,offset,children,error))return false;
            // GatherAllContents enumerates content; AddPane does not establish live ancestry until Layout.
            // The exact pencil must independently pass its complete visible owner chain above.
            for(auto child:children)panes.push_back({child,pane,offset});}
    }
    return (button&&(!view.button||view.button==button))||fail(error,"Character original Rename pencil is absent or cached to another control");
}
bool inspect_ui(Memory& memory,const a::Binding& binding,const json& args,OrderedView& view,a::CharacterRenameIntent& intent,Sample& before,json& result,std::string& error){
    uintptr_t current=0,editor=0,outer=0,button=0;
    if(!window(memory,0,current,editor,outer,error)||!read_ordered_view(memory,current,view,error)||!view.selected||
       !field(memory,view.selected,0x2c,intent.kind,error)||!field(memory,view.selected,0x30,intent.id,error)||
       !offered_rename_button(memory,view,button,result,error))
        return fail(error,error.empty()?"Character rename requires a genuine currently offered Rename button":error);
    if((args.at("kind").get<int64_t>()>=0&&args.at("kind")!=intent.kind)||(args.at("id").get<int64_t>()>=0&&args.at("id")!=intent.id))
        return fail(error,"Character rename diagnostic expected target differs from actual selected Character");
    intent.name=args.at("name").get<std::string>();
    if(!sample(memory,binding,net::current_snapshot_id(),intent,before,error)||before.selected!=view.selected)return false;
    intent.before=before.value.name;
    result.update({{"kind",intent.kind},{"id",intent.id},{"before",intent.before},{"name",intent.name},{"rename_offered",true},
        {"editor_command","MessageTo Parent DoRename"},{"button_command","MessageTo Parent BeginRename"},{"cached_button_present",view.button!=0},{"global_random",before.value.global_random},
        {"complete_before_sha256",before.value.all_sha256},{"complete_others_sha256",before.value.others_sha256}});
    return true;
}
bool execute_ui(UiJob& job,const std::string& token,std::string& error){
    Memory memory;OrderedView view;a::CharacterRenameIntent intent;Sample before;
    job.waiting=false;
    if(job.stage==0){
        if(job.arguments.value("open",false)){
            if(!execute_open(job,error))return false;
            job.open_proof=job.result;job.stage=1;job.waiting=true;return false;
        }
        job.stage=1;
    }
    if(job.stage==1){
    job.result=json::object();if(job.arguments.value("open",false))job.result["open_proof"]=job.open_proof;
    if(!code(memory,error)||!inspect_ui(memory,job.binding,job.arguments,view,intent,before,job.result,error)||intent.before==intent.name)
        return fail(error,error.empty()?"Character rename diagnostic needs an actual changed name":error);
    a::ui_diagnostic::Api api;if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error))return false;
    api.command=reinterpret_cast<Command>(state.image+ui_code.rva);api.destroy_label="BeginRename";
    a::ui_diagnostic::Window control;control.window=view.view;a::ui_diagnostic::Receipt receipt;
    const auto before_captures=captures;
    job.attempted=true;
    if(!a::ui_diagnostic::process_calls().command(api,control,receipt,error)||!receipt.ui_command_returned||!receipt.record_cleanup_confirmed)
        return fail(error,error.empty()?"Character original BeginRename lacks a complete native receipt":error);
    Sample opened;
    if(!sample(memory,job.binding,job.context.snapshot_uuid,intent,opened,error)||!same_local(before,opened)||before.value!=opened.value||
       captures!=before_captures||pending)
        return fail(error,error.empty()?"Character original BeginRename changed gameplay/RNG or captured a command":error);
    job.input_view=view;job.input_intent=intent;job.stage=2;job.waits=0;job.waiting=true;return false;
    }
    if(job.stage!=2)return fail(error,"Character Rename original input has no pending editor stage; no replay");
    intent=job.input_intent;
    if(!read_ordered_view(memory,job.input_view.view,view,error)||view.selected!=job.input_view.selected||view.editor!=job.input_view.editor||
       view.pages!=job.input_view.pages||view.outer!=job.input_view.outer||view.button!=job.input_view.button)
        return fail(error,error.empty()?"Character Rename editor owner/target changed while awaiting native layout":error);
    uintptr_t current=0,requested=0,parent=0,owner=0;
    if(!field(memory,view.pages,0x410,current,error)||!field(memory,view.pages,0x418,requested,error)||
       !field(memory,view.editor,0x278,parent,error)||!field(memory,view.editor,0x280,owner,error))return false;
    job.result["editor_layout"]={{"current_is_editor",current==view.editor},{"requested_is_editor",requested==view.editor},
        {"parent_is_pages",parent==view.pages},{"owner_is_window",owner==view.outer},{"detached",!parent&&!owner},{"waits",job.waits}};
    if(requested!=view.editor)return fail(error,"Character Rename original requested editor page changed before native layout");
    if(current!=view.editor||parent!=view.pages||owner!=view.outer){
        if(++job.waits>8)return fail(error,"Character Rename original editor layout did not become current after 8 native UI updates: "+job.result["editor_layout"].dump());
        job.waiting=true;return false;
    }
    if(!offered_control(memory,view.editor,view.outer,error)||!sample(memory,job.binding,job.context.snapshot_uuid,intent,before,error)||
       before.selected!=view.selected||before.value.name!=intent.before)
        return fail(error,error.empty()?"Character Rename native editor target/name changed before text input":error);
    if(view.focus!=view.editor||view.editing!=1||view.commit_baseline!=intent.before)
        return fail(error,"Character Rename original focused editor/baseline differs focused="+std::to_string(view.focus==view.editor)+
            " editing="+std::to_string(view.editing)+" baseline="+view.commit_baseline+" expected="+intent.before);
    const auto before_captures=captures;
    auto seh=set_view_text(view.editor,intent.name.c_str(),intent.name.size());
    Sample typed;OrderedView draft;
    if(seh||fault||pending||captures!=before_captures||!read_ordered_view(memory,view.view,draft,error)||
       draft.text!=intent.name||!same_editor_input(view,draft)||
       !sample(memory,job.binding,job.context.snapshot_uuid,intent,typed,error)||!same_local(before,typed)||before.value!=typed.value)
        return fail(error,error.empty()?"Character original text preparation captured or changed gameplay/input baseline SEH="+std::to_string(seh):error);
    const auto dispatch_before=capture_dispatch;const auto rejected_before=rejected;
    active_ui_token=token;seh=drop_view_focus(view.editor);active_ui_token.clear();
    Sample after;
    OrderedView committed;
    constexpr std::array<uint32_t,8> lost_focus_selection{0,0xffffffffu,0,0xffffffffu,0,0xffffffffu,0,0xffffffffu};
    const bool commit_readback=read_ordered_view(memory,view.view,committed,error);
    job.result["editor_commit"]={{"original_drop_focus_attempted",true},{"original_drop_focus_returned",seh==0},{"exception",seh},
        {"readback_valid",commit_readback},{"focus_cleared",committed.focus==0},{"editing",committed.editing},
        {"baseline_before",view.commit_baseline},{"baseline_after",committed.commit_baseline},
        {"selection",committed.selection},{"captures",captures-before_captures},
        {"handler_entries",capture_dispatch.entries-dispatch_before.entries},{"rename_labels",capture_dispatch.rename_labels-dispatch_before.rename_labels},
        {"handler_receiver",capture_dispatch.receiver},{"handler_disposition",capture_dispatch.disposition},
        {"rejections",rejected-rejected_before},{"capture_cause",rejected!=rejected_before?cause:std::string{}}};
    if(seh||fault||!pending||pending->diagnostic_token!=token||pending->intent!=intent||captures!=before_captures+1||
       !commit_readback||committed.focus||committed.editing||committed.commit_baseline!=intent.name||
       committed.selection!=lost_focus_selection||
       !sample(memory,job.binding,job.context.snapshot_uuid,intent,after,error)||!same_local(before,after)||before.value!=after.value)
        return fail(error,error.empty()?"Character original editor DropFocus lacks exact deferred Rename/cleared focus/new baseline SEH="+std::to_string(seh):error);
    job.result["editor_commit"]["original_drop_focus"]=true;
    job.result["captured"]=true;job.result["capture_count"]=captures;
    job.result["original_class_card_open"]=job.arguments.value("open",false);
    job.stage=3;return true;
}
}
bool native_ui(const json& args,json& result,std::string& error)try{
    result=json::object();const std::set<std::string> keys{"token","kind","id","name","read_only"};
    if(!args.is_object()||(args.size()!=keys.size()&&args.size()!=keys.size()+1)||
       (args.size()==keys.size()+1&&(!args.contains("open")||!args.at("open").is_boolean())))
        return fail(error,"Character Rename UI requires token/kind/id/name/read_only and optional Boolean open");
    for(const auto& key:keys)if(!args.contains(key))return fail(error,"Character Rename UI missing field="+key);
    if(!args.at("token").is_string()||!args.at("kind").is_number_integer()||!args.at("id").is_number_integer()||
       !args.at("name").is_string()||!args.at("read_only").is_boolean())return fail(error,"Character Rename UI scalar types differ");
    const auto token=args.at("token").get<std::string>(),name=args.at("name").get<std::string>();
    const auto kind=args.at("kind").get<int64_t>(),id=args.at("id").get<int64_t>();const bool read_only=args.at("read_only").get<bool>();
    if(!token_valid(token)||kind< -1||kind>4||id< -1||id>=1000000||(!read_only&&(kind<0||id<0))||
       name.find('\0')!=std::string::npos||!npc_census::utf8(name)||std::count_if(name.begin(),name.end(),[](unsigned char c){return (c&0xc0)!=0x80;})>20||
       (read_only&&!name.empty())||(args.value("open",false)&&(kind<0||id<0))||ui_jobs.contains(token)||ui_jobs.size()>=64)
        return fail(error,"Character Rename UI token/target/name invalid or already attempted; no replay");
    const auto controller=typed_action_runtime::controller_status();
    if(!Config::instance().local_test||!ready||fault||GetCurrentThreadId()!=state.thread||!net::session_wire::connected()||!ordered_mode()||pending_ui()||
       !controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))return fail(error,"Character Rename UI requires idle owned ordered Mainthread");
    UiJob job;job.arguments=args;
    if(!a::selected_session().current(job.context,error)||job.context.snapshot_uuid!=net::current_snapshot_id()||
       Config::instance().is_host!=(job.context.role==a::Role::Host)||!typed_action_runtime::network_binding(job.context.snapshot_uuid,job.binding,error))return false;
    if(read_only){Memory memory;OrderedView view;a::CharacterRenameIntent intent;Sample before;
        if(args.value("open",false)){DesignMenu menu;intent.kind=int32_t(kind);intent.id=int32_t(id);
            if(!code(memory,error)||!open_code(memory,error)||!design_menu(memory,menu,error)||
               !sample(memory,job.binding,job.context.snapshot_uuid,intent,before,error))return false;
            result={{"original_design_offered",true},{"collection_shown",menu.collection_shown},{"design_shown",menu.shown},{"kind",kind},{"id",id},{"before",before.value.name},
                {"complete_before_sha256",before.value.all_sha256},{"complete_others_sha256",before.value.others_sha256},
                {"global_random",before.value.global_random}};
            job.returned=true;job.result=result;ui_jobs.emplace(token,std::move(job));return true;
        }
        if(!code(memory,error)||!inspect_ui(memory,job.binding,args,view,intent,before,result,error))return false;
        job.returned=true;job.result=result;ui_jobs.emplace(token,std::move(job));return true;
    }
    ui_jobs.emplace(token,std::move(job));queued_ui=token;result={{"token",token},{"queued",true},{"input_attempted",false}};return true;
}catch(const std::exception& exception){return fail(error,std::string("Character Rename UI boundary: ")+exception.what());}
catch(...){return fail(error,"Character Rename UI non-standard boundary exception");}
void tick_original_ui(){
    if(!queued_ui)return;bool inside=false;std::string error;
    if(!typed_action_runtime::original_go_admission(inside,error)||!inside){if(error.empty())return;}
    const auto token=*queued_ui;queued_ui.reset();auto& job=ui_jobs.at(token);
    a::Context actual;a::Binding binding;
    if(error.empty()&&(!a::selected_session().current(actual,error)||actual!=job.context||
       !typed_action_runtime::read_binding(job.context.snapshot_uuid,binding,error)||binding!=job.binding))
        if(error.empty())error="Character Rename queued input Session/Binding changed";
    if(error.empty()){
        try{job.returned=execute_ui(job,token,error);}
        catch(const std::exception& exception){error=std::string("Character Rename original input boundary: ")+exception.what();}
        catch(...){error="Character Rename original input non-standard exception";}
    }
    active_ui_token.clear();job.cause=error;
    if(!job.returned&&job.waiting&&error.empty()){queued_ui=token;return;}
    if(!job.returned){if(job.cause.empty())job.cause="Character Rename original UI failed without cause";if(job.attempted)fault=true;reject(job.cause);}
}
