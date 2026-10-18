#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include "vendor/nlohmann/json.hpp"

using Json=nlohmann::json;
struct Config {bool local_test=false;static Config& instance(){static Config value;return value;}};
constexpr unsigned MEM_COMMIT=1,PAGE_READWRITE=2,PAGE_WRITECOPY=4,PAGE_GUARD=8,PAGE_NOACCESS=16;
struct MEMORY_BASIC_INFORMATION {unsigned State=MEM_COMMIT,Protect=PAGE_READWRITE;};
bool writable=true,fixture_member=false,fixture_host=false,pending=false,load_attempted=false,choosing=false,refresh_ok=true;
bool host_world_pick_requested=false,current_host_world=false;
bool memory_readable=true;int ui_mode=2;size_t list_page=0;unsigned refreshes=0,shows=0;
int fail_show=0;void* panel=reinterpret_cast<void*>(1);void* browser_panel=reinterpret_cast<void*>(2);
std::vector<std::string> saved_worlds={"Saved World"};
std::string ui_status,cause,list_status,last_ui_status,selected_world,selected_file;
std::map<std::string,std::array<unsigned char,0x500>> panes;
std::map<std::string,std::string> labels;
std::map<std::string,bool> disabled;
size_t VirtualQuery(void*,MEMORY_BASIC_INFORMATION* out,size_t){*out={MEM_COMMIT,writable?PAGE_READWRITE:PAGE_NOACCESS};return sizeof(*out);}
template<class T>bool read(void* pointer,size_t offset,T& out){if(!memory_readable)return false;std::memcpy(&out,static_cast<unsigned char*>(pointer)+offset,sizeof(out));return true;}
uintptr_t image=0x10000000;
unsigned text_edit_calls=0,button_calls=0;unsigned caption_failure=0;
bool caption_readable=true,caption_wrong=false,missing_pane=false;
bool is_button(const std::string& id){return id=="leave"||id=="list0"||id=="list1"||id=="list2"||id=="list3";}
void* pane(const char* id){
    if(missing_pane){cause="original pane lookup failure";return nullptr;}
    auto [found,added]=panes.try_emplace(id);
    if(added){const uintptr_t vtable=image+(is_button(id)?0x16e3ec0:0x16dbe00);std::memcpy(found->second.data(),&vtable,sizeof(vtable));
        found->second[0x2d0]=0x5a;}
    return found->second.data();
}
std::string pane_id(void* pointer){for(auto& [id,bytes]:panes)if(bytes.data()==pointer)return id;throw std::runtime_error("foreign pane receiver");}
unsigned string_call(uintptr_t rva,void* receiver,const char* value){
    const auto id=pane_id(receiver);if(rva!=0x7f1630||is_button(id))throw std::runtime_error("wrong TextEdit API or Button receiver");
    ++text_edit_calls;labels[id]=value;return 0;
}
unsigned button_caption_call(void* receiver,const char* value){
    const auto id=pane_id(receiver);if(!is_button(id))throw std::runtime_error("TextEdit receiver passed to Button API");
    ++button_calls;if(caption_failure)return caption_failure;
    labels[id]=caption_wrong?"wrong":value;return 0;
}
bool text(const void* pointer,std::string& value){
    if(!caption_readable)return false;
    for(const auto& [id,bytes]:panes)if(bytes.data()+0x3e8==pointer){value=labels[id];return true;}
    throw std::runtime_error("wrong native Button caption field");
}

bool fail(const std::string& error){cause=error;return false;}
#include "native_lobby_text_label.inc"
#include "native_lobby_button_label.inc"
bool disable(const char* id,bool value){disabled[id]=value;return true;}
bool admitted(){return fixture_member;}
bool lobby_host(){return fixture_member&&fixture_host;}
bool refresh_saved_worlds(std::string& error){++refreshes;if(!refresh_ok){error="original saved-world discovery failure";return false;}return true;}
int show(void*,bool){++shows;return shows==unsigned(fail_show)?123:0;}
#include "native_lobby_ux_body.inc"

void check(bool condition,const char* why){if(!condition)throw std::runtime_error(why);}
bool visible(const std::string& id){return panes[id][0x2b4]&&panes[id][0x69];}
void reset(){text_edit_calls=button_calls=caption_failure=0;caption_readable=true;caption_wrong=missing_pane=false;writable=memory_readable=refresh_ok=true;fixture_member=fixture_host=pending=load_attempted=choosing=host_world_pick_requested=current_host_world=false;ui_mode=2;list_page=refreshes=shows=0;fail_show=0;saved_worlds={"Saved World"};cause.clear();selected_world.clear();selected_file.clear();labels.clear();disabled.clear();panes.clear();Config::instance()={};}
int main(){try{
    std::string error;
    reset();check(button_label("leave","Lobby verlassen")&&button_calls==1&&text_edit_calls==0&&panes["leave"][0x2d0]==0x5a,
                  "actual Button helper uses TextEdit API or changes material field");
    check(label("status","Ready")&&text_edit_calls==1&&button_calls==1,"existing TextEdit path changed");
    check(!button_label("status","wrong")&&button_calls==1&&cause.find("receiver class differs")!=std::string::npos,
          "actual Button helper admits TextEdit receiver");
    reset();void* receiver=pane("leave");const uintptr_t foreign=image+0x16e3ed0;std::memcpy(receiver,&foreign,sizeof(foreign));
    check(!button_label("leave","wrong")&&button_calls==0,"foreign native vtable reached setter");
    reset();memory_readable=false;check(!button_label("leave","wrong")&&button_calls==0,"unreadable receiver reached setter");
    reset();missing_pane=true;check(!button_label("leave","wrong")&&button_calls==0&&cause=="original pane lookup failure","lookup failure hidden");
    reset();caption_failure=123;check(!button_label("leave","wrong")&&cause.find("exception=123")!=std::string::npos&&button_calls==1,
                                    "native caption exception hidden or replayed");
    reset();caption_wrong=true;check(!button_label("leave","expected")&&cause.find("readback differs")!=std::string::npos,"wrong caption confirmed");
    reset();caption_readable=false;check(!button_label("leave","wrong")&&cause.find("readback differs")!=std::string::npos,"unreadable caption confirmed");

    reset();check(render_context(true,false,Json::object(),error),"entry presentation failed");
    check(button_calls==5&&text_edit_calls==0,"Title captions did not use five native Button calls");
    check(visible("host")&&visible("join")&&visible("lobby_id")&&visible("lobby_id_label"),"Title entry lost an admission action or persistent ID label");
    check(!visible("invite")&&!visible("pick")&&!visible("start")&&!visible("chat_input")&&!visible("leave"),"Title entry exposes unrelated session controls");
    fixture_member=fixture_host=true;check(render_context(true,false,Json::object(),error),"Host presentation failed");
    check(!visible("host")&&!visible("join")&&!visible("lobby_id")&&visible("invite")&&visible("pick")&&visible("start")&&visible("chat_input")&&visible("leave"),"Host context lost required controls or exposes readmission");
    check(labels["leave"]=="Lobby verlassen"&&labels["list0"]=="Beitreten","lobby action copy differs");
    fixture_host=false;check(render_context(true,false,Json::object(),error)&&!visible("pick")&&!visible("start")&&labels["status"]=="Warte auf den Host","Client exposes Host controls or misses next-state wording");
    labels.clear();cause="original transport failure";check(render_context(true,false,Json::object(),error)&&!labels.contains("status")&&cause=="original transport failure","context overwrites original failure");
    fixture_host=true;load_attempted=true;check(render_context(false,true,{{"state",5},{"playing",true}},error),"Playing presentation failed");
    check(!visible("host")&&!visible("join")&&!visible("pick")&&!visible("start")&&visible("chat_input")&&visible("leave")&&labels["leave"]=="Spiel verlassen","Playing exposes admission/world reload or loses Chat/Leave");
    Config::instance().local_test=true;check(render_context(false,true,Json::object(),error)&&!visible("invite"),"Local test advertises unavailable Steam invitation");
    reset();fixture_member=fixture_host=true;check(open_world_picker()&&refreshes==1&&shows==2&&choosing,"Host picker does not open existing explicit selection");
    check(render_context(true,false,Json::object(),error)&&labels["list0"]=="Auswählen","world row does not name selection");
    reset();fixture_member=fixture_host=true;saved_worlds.clear();check(open_world_picker()&&!choosing&&shows==0&&ui_status=="Keine gespeicherte Welt. Zuerst eine Welt erstellen und speichern.","empty saves unexpectedly open or select a world");
    reset();fixture_member=fixture_host=true;refresh_ok=false;check(!open_world_picker()&&cause=="original saved-world discovery failure"&&shows==0,"picker loses discovery cause or mutates after failure");
    reset();fixture_member=fixture_host=true;fail_show=1;check(!open_world_picker()&&shows==1&&!choosing&&cause.find("lobby hide failed exception=123")!=std::string::npos,"failed hide opens picker or loses cause");
    reset();fixture_member=fixture_host=true;fail_show=2;check(!open_world_picker()&&shows==2&&!choosing&&cause.find("browser show failed exception=123")!=std::string::npos,"failed picker show is presented as open");
    reset();check(!open_world_picker()&&refreshes==0&&shows==0,"nonfixture_member opens picker");
    reset();fixture_member=fixture_host=true;ui_mode=3;check(!open_world_picker()&&refreshes==0&&shows==0,"in-game picker replaces the active world");
    reset();fixture_member=fixture_host=host_world_pick_requested=true;check(process_host_world_picker_request(true,error)&&!host_world_pick_requested&&choosing&&refreshes==1,"confirmed new Title host misses one automatic picker");
    check(process_host_world_picker_request(true,error)&&refreshes==1,"observation replays a consumed Host picker");
    reset();fixture_member=fixture_host=host_world_pick_requested=current_host_world=true;check(process_host_world_picker_request(true,error)&&!host_world_pick_requested&&refreshes==0,"current Host world opens picker");
    reset();fixture_member=fixture_host=host_world_pick_requested=true;selected_file="save/Selected/autosave.vrz";check(process_host_world_picker_request(true,error)&&refreshes==0,"selected save is replaced by picker");
    reset();fixture_member=fixture_host=host_world_pick_requested=true;check(process_host_world_picker_request(false,error)&&refreshes==0&&!host_world_pick_requested,"in-game transition leaves stale picker request");
    reset();fixture_member=fixture_host=host_world_pick_requested=true;refresh_ok=false;check(!process_host_world_picker_request(true,error)&&error=="original saved-world discovery failure"&&!host_world_pick_requested&&refreshes==1,"automatic picker loses cause or keeps failed request");
    check(process_host_world_picker_request(true,error)&&refreshes==1,"failed picker observation replays request");
    reset();writable=false;check(!set_pane_visible("host",true)&&cause.find("not writable")!=std::string::npos&&!visible("host"),"read-only pane mutation was attempted");
    reset();panes["host"][0x2b4]=2;check(!set_pane_visible("host",true)&&cause.find("unreadable")!=std::string::npos,"invalid native boolean accepted");
    std::cout<<"PASS actual context visibility, explicit picker, Host/Client/Playing copy, native write guards and original failure causes\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
