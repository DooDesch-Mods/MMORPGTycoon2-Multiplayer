#define CHARACTER_RENAME_UI_FIXTURE
#include "test_character_rename_runtime.cpp"
#include "npc_census.h"
namespace npc_census=mt2mp::npc_census;
bool token_valid(const std::string& token){return mt2mp::game::monster_target_key::uuid(token);}
unsigned candidate_reads=0;
namespace character_definition_identity {
struct Definition {std::string key;int32_t kind=0,id=0;uintptr_t object=0,parent=0;uint64_t uid=1;bool deleted=false,released=true;};
struct Census {std::vector<Definition> definitions;};
}
namespace character_membership_reader {struct Users {std::map<std::string,std::vector<int>> by_definition;};}
namespace character_actor_records {
bool read_current(const a::Context&,const a::Binding&,character_definition_identity::Census& out,character_membership_reader::Users& users,std::string&){
    ++candidate_reads;out.definitions={{"character:0:9",0,9,0x17000,0,1,false,true},{"deleted",0,10,0x17100,0,2,true,true},
        {"monster",1,0,0x17200,0,3,false,true},{"variant",0,11,0x17300,0x17000,4,false,true}};
    users.by_definition["character:0:9"]={1,2};return true;
}}
#include "character_rename_read_actual.inc"
bool open_code(Memory&,std::string&){return true;}
unsigned design_activations=0,class_selections=0;int32_t activated_item_state=0;
uint32_t activate_design(uintptr_t button){++design_activations;if(button!=0x13200)return 1;
    native_values[{0x12000,0x45f}]=1;native_values[{0x13300,0x38}]=activated_item_state;return 0;}
uint32_t select_classes(uintptr_t window){++class_selections;if(window!=0x12000)return 1;native_values[{0x900,0x45f}]=1;
    if(preparation_effect)++live.value.release_edits[0];return 0;}
#include "character_rename_ui_helpers_actual.inc"
#include "character_rename_ui_public_actual.inc"
void reset_ui(){reset();ui_text=live.value.name;ui_commit_baseline=ui_text;}
int main()try{
    std::string error;json result;
    const auto args=[](const char* token,bool read_only){return json{{"token",token},{"kind",0},{"id",1},{"name",read_only?"":"New name"},{"read_only",read_only}};};
    const char* token="11111111-1111-4111-8111-111111111111";
    reset_ui();native_text[{0x17000,0xb0}]="Existing class";
    need(read_native(json::object(),result,error)&&result.at("candidates").size()==1&&result.at("candidates").at(0).at("id")==9&&
        result.at("candidates").at(0).at("current_users")==2&&candidate_reads==1&&!captures&&!original_count,error);
    need(!read_native({{"action_uuid","bad"}},result,error)&&candidate_reads==1,"Malformed cached read reached native census");
    need(!read_native({{"action_uuid",token}},result,error)&&candidate_reads==1,"Unknown action detail became a census");
    const auto collection=[](){
        design_activations=class_selections=0;activated_item_state=0;
        auto put=[](uintptr_t p,size_t o,uint64_t v){native_values[{p,o}]=v;};
        put(state.image,0x1a3c478,0x800);put(state.image,0x1a3c480,3);put(state.image,0x1a3c484,3);put(0x800,0,0x900);
        put(0x800,8,0x11000);put(0x800,16,0x12000);
        native_text[{0x11000,0x1b0}]="system_buttons";native_text[{0x12000,0x1b0}]="designmenu";
        put(0x900,0x45f,0);put(0x900,0,0xd00);put(0xd00,0x178,state.image+0x8167c0);
        put(0x900,0x2d0,0xe00);put(0x900,0x2d8,1);put(0x900,0x2dc,1);
        put(0x900,0x300,0);put(0x900,0x308,0);put(0x900,0x30c,0);put(0xe00,0,0xa00);
        put(0xa00,0,state.image+0x173a470);put(state.image+0x173a470,0x178,state.image+0xc35730);
        put(0xa00,0x280,0x900);put(0xa00,0x278,0x900);put(0xa00,0x358,0);put(0xa00,0x35c,1);
        put(0xa00,0x340,0xf00);put(0xa00,0x348,0xf80);put(0xf80,0,0xf30);put(0xa00,0x390,0xb00);
        put(0xb00,0,state.image+0x16e3ec0);put(0xb00,0x46d,0);put(0xa00,0x174,1);
        put(0xb00,0x280,0x900);put(0xb00,0x278,0xa00);put(0xb00,0x69,1);put(0xa00,0x69,1);
        put(0x11000,0,0xd00);put(0x11000,0x45f,1);put(0x11000,0x2d0,0x13000);put(0x11000,0x2d8,1);put(0x11000,0x2dc,1);
        put(0x11000,0x300,0);put(0x11000,0x308,0);put(0x11000,0x30c,0);put(0x13000,0,0x13100);
        put(0x13100,0,state.image+0x16db1c0);put(state.image+0x16db1c0,0x178,state.image+0xc35730);
        put(0x13100,0x278,0x11000);put(0x13100,0x280,0x11000);put(0x13100,0x69,1);put(0x13100,0x368,0x13200);
        native_text[{0x13100,0x318}]="@DesignMenu";put(0x13200,0,state.image+0x16e5b90);
        put(0x13200,0x2d0,0x13300);put(0x13200,0x2d8,0x13400);put(0x13400,0,0x13320);put(0x13300,0x28,0x13400);
        put(0x13100,0x360,0x13300);put(0x13300,0,state.image+0x17437f0);
        put(0x13200,0x278,0x13100);put(0x13200,0x280,0x11000);put(0x13200,0x350,0x13100);
        put(0x13200,0x381,1);put(0x13200,0x174,1);put(0x13200,0x69,1);put(0x13300,0x38,1);
        put(0x12000,0,state.image+0x16e6400);put(0x12000,0x45f,0);put(0x12000,0x540,0x14000);
        put(0x14000,0,state.image+0x1743a60);put(0x14000,0x3b8,0x124e8);put(0x14000,0x69,1);
        put(0x14000,0x278,0x12000);put(0x14000,0x280,0x12000);
        put(0x12000,0x518,0x15000);put(0x12000,0x520,1);put(0x12000,0x524,1);put(0x15000,0,0x15100);
        native_text[{0x15100,0x18}]="pcs";native_text[{0x15100,0x38}]="MessageTo Window HideAndOpenWindow \"collection_classes\"";
        put(0x14000,0x370,0x15200);put(0x14000,0x378,1);put(0x14000,0x37c,1);put(0x15200,0,0x15300);
        native_text[{0x15300,0x1b0}]="pcs";put(0x15300,0x69,1);put(0x15300,0x278,0x14000);put(0x15300,0x280,0x12000);
        put(0x14000,0x3a0,0);put(0x14000,0x3a8,0);put(0x14000,0x3ac,0);
    };
    reset_ui();collection();ui_disabled=0;fixture_controller["pending"]=false;
    auto opening=args(token,false);opening["open"]=true;
    need(native_ui(opening,result,error),error);original_go=true;tick_original_ui();
    need(queued_ui&&ui_jobs.at(token).stage==1&&!captures&&!begin_rename_count,"Opening did not yield for normal UI layout");
    tick_original_ui();need(queued_ui&&begin_rename_count==1&&!captures,"BeginRename did not yield before text input");tick_original_ui();
    need(ui_jobs.at(token).returned&&ui_jobs.at(token).result.at("original_class_card_open")==true&&captures==1&&!original_count,ui_jobs.at(token).cause);
    need(ui_jobs.at(token).result.at("open_proof").contains("open_complete_sha256"),"Original class open proof was overwritten by Rename input receipt");
    need(drop_focus_count==1&&ui_jobs.at(token).result.at("editor_commit").at("original_drop_focus")==true&&
        ui_jobs.at(token).result.at("editor_commit").at("focus_cleared")==true&&ui_commit_baseline=="New name"&&!ui_editing&&!ui_focus,
        "Original DropFocus did not commit once through real command/baseline/focus state");
    need(ui_jobs.at(token).result.at("editor_commit").at("handler_entries")==1&&
        ui_jobs.at(token).result.at("editor_commit").at("rename_labels")==1&&
        ui_jobs.at(token).result.at("editor_commit").at("handler_disposition")=="captured"&&
        ui_jobs.at(token).result.at("editor_commit").at("rejections")==0,
        "Native text commit argument lost its exact hook/label/capture receipt");
    need(design_activations==1&&class_selections==1&&ui_jobs.at(token).result.at("open_proof").at("original_design_classes_open")==true,"Original full menu route was skipped or replayed");
    const auto opened=ui_jobs.at(token).result.at("open_proof");
    need(opened.at("design_before_activation").at("item_state")==1&&
        opened.at("design_after_activation").at("item_state")==0&&opened.at("design_after_activation").at("design_shown")==true&&
        opened.at("design_after_activation").at("offer_verified")==true,"Native Window Activate state transition was rejected or omitted");
    need(opened.at("before")==opened.at("after_collection")&&opened.at("before")==opened.at("after_card")&&
        opened.at("before").at("character").contains("global_random")&&opened.at("before").at("proof").contains("art_store_sha256"),"Opening readback omitted or changed retained RNG/ArtStore proof");
    reset_ui();collection();ui_disabled=0;fixture_controller["pending"]=false;
    auto open_read=args(token,true);open_read["open"]=true;
    need(native_ui(open_read,result,error)&&result.at("original_design_offered")==true&&!captures&&!original_count,error);
    need(!design_activations&&!class_selections,"Read-only menu inspection invoked an original callback");
    reset_ui();collection();ui_disabled=0;fixture_controller["pending"]=false;native_values[{0x13200,0x381}]=0;
    need(native_ui(opening,result,error),error);original_go=true;tick_original_ui();
    need(!ui_jobs.at(token).attempted&&!captures&&!fault&&!design_activations&&!class_selections,"Disabled Design input entered original menu");
    reset_ui();collection();ui_disabled=0;fixture_controller["pending"]=false;native_values[{0x13300,0x38}]=0;
    need(native_ui(opening,result,error),error);original_go=true;tick_original_ui();
    need(!ui_jobs.at(token).attempted&&!design_activations&&!class_selections&&!captures,"Active item with hidden Design menu was accepted");
    reset_ui();collection();ui_disabled=0;fixture_controller["pending"]=false;activated_item_state=2;
    need(native_ui(opening,result,error),error);original_go=true;tick_original_ui();
    need(ui_jobs.at(token).attempted&&!ui_jobs.at(token).returned&&fault&&design_activations==1&&!class_selections&&!captures&&
        ui_jobs.at(token).result.at("design_after_activation").at("item_state")==2&&
        ui_jobs.at(token).result.at("design_after_activation").at("offer_verified")==false&&
        ui_jobs.at(token).cause.find("state=2 expected=0 shown=1")!=std::string::npos,"Invalid native post-activation state lost its measured failure");
    reset_ui();collection();ui_disabled=0;fixture_controller["pending"]=false;native_values[{0x12000,0x45f}]=1;
    native_values[{0x13300,0x38}]=0;
    native_values[{0x14000,0x3a0}]=0x16000;native_values[{0x14000,0x3a8}]=native_values[{0x14000,0x3ac}]=1;native_text[{0x16000,0}]="pcs";
    need(native_ui(opening,result,error),error);original_go=true;tick_original_ui();
    need(!ui_jobs.at(token).attempted&&!captures&&!class_selections,"Locked original classes row was bypassed");
    reset_ui();collection();ui_disabled=0;fixture_controller["pending"]=false;native_values[{0xb00,0x46d}]=1;
    need(native_ui(opening,result,error),error);original_go=true;tick_original_ui();
    need(ui_jobs.at(token).attempted&&!ui_jobs.at(token).returned&&!captures&&fault,"Disabled original collection card bypassed");
    reset_ui();collection();ui_disabled=0;fixture_controller["pending"]=false;preparation_effect=true;
    need(native_ui(opening,result,error),error);original_go=true;tick_original_ui();
    need(ui_jobs.at(token).attempted&&!ui_jobs.at(token).returned&&fault&&!captures,"Original collection open gameplay effect was normalized");
    reset_ui();ui_disabled=0;fixture_controller["pending"]=false;
    auto inspect=args(token,true);inspect["kind"]=inspect["id"]=-1;
    need(native_ui(inspect,result,error)&&result.at("rename_offered")==true&&!original_count&&!captures,error);
    need(!native_ui(inspect,result,error),"Read-only token replay admitted");
    reset_ui();ui_disabled=0;fixture_controller["pending"]=false;
    native_values[{0x100,0x2d8}]=native_values[{0x100,0x2dc}]=2;native_values[{0x1800,8}]=0x1900;
    native_values[{0x1900,0}]=state.image+0x1703290;
    native_values[{state.image+0x1703290,0x178}]=state.image+0xc35730;
    native_values[{0x1900,0x278}]=native_values[{0x1900,0x280}]=0;native_values[{0x1900,0x69}]=0;
    native_text[{0x1900,0x1b0}]="inactive_content";
    need(native_ui(args(token,true),result,error)&&result.at("rename_offered")==true&&!captures&&!begin_rename_count,error);
    need(result.at("rename_control_tree").size()==3&&result.at("rename_control_tree").at(1).at("parent_is_container")==false&&
        result.at("rename_control_tree").at(1).at("id")=="inactive_content"&&result.at("rename_pencil_ancestry").size()==2,
        "Content discovery treated inactive content as offered ancestry or lost measured controls");
    for(unsigned variant=0;variant<4;++variant){
        reset_ui();ui_disabled=0;fixture_controller["pending"]=false;native_values[{0x100,0x370}]=0;
        if(variant==0)native_values[{0x600,0x278}]=0;
        if(variant==1)native_values[{0x600,0x280}]=0x999;
        if(variant==2)native_values[{0x600,0x69}]=0;
        if(variant==3)native_values[{0x600,0x278}]=0x300;
        need(native_ui(args(token,false),result,error),error);original_go=true;tick_original_ui();
        need(!ui_jobs.at(token).attempted&&!begin_rename_count&&!captures&&!fault&&
            ui_jobs.at(token).result.at("rename_pencil_ancestry").at(0).at("pane")==0x600&&
            !ui_jobs.at(token).cause.empty(),"Invalid exact pencil ancestry was admitted or lost its measured failure");
    }
    reset_ui();ui_disabled=0;fixture_controller["pending"]=false;
    need(native_ui(args(token,false),result,error)&&queued_ui&&!captures&&!original_count,error);
    tick_original_ui();need(queued_ui&&!ui_jobs.at(token).attempted,"Native input ran outside OriginalGo");
    original_go=true;tick_original_ui();need(queued_ui&&!captures&&begin_rename_count==1,"BeginRename did not wait for native UI update");
    tick_original_ui();need(!queued_ui&&ui_jobs.at(token).attempted&&ui_jobs.at(token).returned&&pending&&captures==1&&!original_count,error+ui_jobs.at(token).cause);
    need(pending->diagnostic_token==token&&live.value.name=="Old name","Native input bypassed deferred capture");
    original_go=false;tick();need(submit_count==1&&ui_jobs.at(token).action_uuid==fixture_request.action_uuid&&!fault,"Native input lost submitted correlation");
    need(!ui_focus&&!ui_editing&&ui_commit_baseline=="New name"&&live.value.name=="Old name",
        "Native commit aftermath did not retain changed baseline with deferred gameplay");
    set_view_text(0x200,live.value.name.data(),live.value.name.size());
    fixture_controller["pending"]=true;need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::Confirmed&&original_count==1&&!seed_count,error);
    need(ui_text=="Old name"&&ui_commit_baseline=="New name"&&live.value.name=="New name"&&
        operations.at(fixture_request.action_uuid).ui_state.at("current_draft_matches")==false&&
        operations.at(fixture_request.action_uuid).ui_state.at("before")==operations.at(fixture_request.action_uuid).ui_state.at("restored"),
        "Normal unfocused model-text refresh broke immutable captured intent or current UI restoration");
    read_failed=true;const auto census_calls=candidate_reads;
    need(read_native({{"action_uuid",fixture_request.action_uuid}},result,error)&&result.at("character_before").at("name")=="Old name"&&
        result.at("character_after").at("name")=="New name"&&result.at("proof_before").contains("art_store_sha256")&&
        result.at("ledger_before").at("line_counts").size()==20&&candidate_reads==census_calls,error);
    read_failed=false;
    need(!native_ui(args(token,false),result,error)&&original_count==1,"Original UI token replayed");
    reset_ui();ui_disabled=0;fixture_controller["pending"]=false;
    native_values[{0x100,0x370}]=0;native_values[{0x200,0x278}]=0;native_values[{0x200,0x280}]=0;
    native_values[{0x700,0x410}]=0x222;
    need(native_ui(args(token,false),result,error),error);original_go=true;tick_original_ui();
    need(queued_ui&&ui_jobs.at(token).stage==2&&begin_rename_count==1&&!captures&&!fault,"Owned inactive editor/unnamed pencil was rejected");
    tick_original_ui();need(queued_ui&&begin_rename_count==1&&!captures&&!fault&&
        ui_jobs.at(token).result.at("editor_layout").at("detached")==true,"Pending native layout replayed original input or lost measured state");
    native_values[{0x700,0x410}]=0x200;native_values[{0x200,0x278}]=0x700;native_values[{0x200,0x280}]=0x300;
    ++live.value.global_random[0];tick_original_ui();
    need(ui_jobs.at(token).returned&&captures==1&&begin_rename_count==1&&!original_count&&!fault,
        "Original editor normal-layout continuation failed or passive frame RNG was treated as an action effect: "+ui_jobs.at(token).cause);
    reset_ui();ui_disabled=0;fixture_controller["pending"]=false;
    need(native_ui(args(token,false),result,error),error);original_go=true;tick_original_ui();drop_focus_noop=true;tick_original_ui();
    need(fault&&!queued_ui&&!captures&&drop_focus_count==1&&begin_rename_count==1&&
        ui_jobs.at(token).result.at("editor_commit").at("readback_valid")==true&&
        ui_jobs.at(token).result.at("editor_commit").at("focus_cleared")==false&&
        ui_jobs.at(token).result.at("editor_commit").at("captures")==0,
        "Failed original DropFocus lost measured focus/selection/baseline/capture state or replayed input");
    reset_ui();ui_disabled=0;fixture_controller["pending"]=false;native_text[{0x600,0x388}]="Wrong command";
    native_text[{0x600,0x318}]="MessageTo Parent BeginRename";
    need(native_ui(args(token,false),result,error),error);original_go=true;tick_original_ui();
    need(!ui_jobs.at(token).attempted&&!begin_rename_count&&!captures,"TextPane field was mistaken for actual ButtonPane command");
    for(unsigned variant=0;variant<3;++variant){
        reset_ui();ui_disabled=0;fixture_controller["pending"]=false;
        if(variant==0)native_text[{0x200,0x400}]="MessageTo Parent DoRename";
        if(variant==1)native_text[{0x200,0x3e0}]="";
        if(variant==2)native_values[{0x200,0x4a0}]=0x999;
        need(native_ui(args(token,false),result,error),error);original_go=true;tick_original_ui();
        need(!ui_jobs.at(token).attempted&&!begin_rename_count&&!drop_focus_count&&!captures,
            "Actual commandPreview/command/engine-value contract was replaced by the old assumed setter layout");
    }
    reset_ui();ui_disabled=0;fixture_controller["pending"]=false;native_values[{0x200,0x278}]=0;native_values[{0x200,0x280}]=0;
    native_values[{0x700,0x410}]=0x222;
    need(native_ui(args(token,false),result,error),error);original_go=true;tick_original_ui();
    for(unsigned i=0;i<9&&queued_ui;++i)tick_original_ui();
    need(fault&&!queued_ui&&!captures&&begin_rename_count==1&&ui_jobs.at(token).cause.find("8 native UI updates")!=std::string::npos&&
        ui_jobs.at(token).result.at("editor_layout").at("current_is_editor")==false,"Native layout timeout lost measured state or replayed BeginRename");
    reset_ui();ui_disabled=0;fixture_controller["pending"]=false;
    need(native_ui(args(token,false),result,error),error);++session.session;original_go=true;tick_original_ui();
    need(!ui_jobs.at(token).attempted&&!ui_jobs.at(token).returned&&!ui_jobs.at(token).cause.empty()&&!original_count&&!captures,"Changed queued Context executed");
    reset_ui();ui_disabled=0;fixture_controller["pending"]=false;
    need(native_ui(args(token,false),result,error),error);original_go=true;bound_button=true;tick_original_ui();
    need(!ui_jobs.at(token).attempted&&!original_count&&!captures&&!fault,"Unsafe native control preparation executed or poisoned ordinary runtime");
    reset_ui();ui_disabled=0;fixture_controller["pending"]=false;
    need(native_ui(args(token,false),result,error),error);preparation_rng=true;original_go=true;tick_original_ui();
    need(ui_jobs.at(token).attempted&&!ui_jobs.at(token).returned&&fault&&!original_count&&!captures,"Original BeginRename RNG drift was hidden");
    std::cout<<checks<<" actual Character Rename original-UI queue/ownership/capture/no-replay checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
