#pragma once
#include "job_codec.h"
#include "manual_ui.h"
#include "developer_staff/bug_policy_read.h"
#include "initial_release/release_entry.h"
namespace mt2mp::game::bug_authority::manual_action {
namespace fixture {
using Json=nlohmann::json;
enum class Op {Show,Select,Click};
enum class Input {Hover,Press,Release};
struct Request {Op op=Op::Show;std::string token,origin;uint64_t epoch=0,generation=0;BugIdentity bug;bool schedule=true;};
inline Json identity(const BugIdentity& b){return Json::array({int(b.parent_kind),b.parent_key,int(b.birth.kind),b.birth.source,b.birth.sequence,b.birth.occurrence});}
inline bool parse(const Json& j,Request& r,std::string& e){
    try{
        if(!j.is_object()||j.size()!=7)throw std::runtime_error("exact seven fields required");
        for(const auto* k:{"command","token","native_origin_uuid","epoch","generation","bug","schedule"})if(!j.contains(k))throw std::runtime_error("required field missing");
        const auto command=j.at("command").get<std::string>();
        if(command!="Show"&&command!="Select"&&command!="Click")throw std::runtime_error("command must be Show, Select or Click");
        r.op=command=="Show"?Op::Show:command=="Select"?Op::Select:Op::Click;
        r.token=j.at("token").get<std::string>();r.origin=j.at("native_origin_uuid").get<std::string>();
        r.epoch=codec_detail::u64(j.at("epoch"));r.generation=codec_detail::u64(j.at("generation"));r.schedule=codec_detail::boolean(j.at("schedule"));
        const auto& b=j.at("bug");if(!b.is_array()||b.size()!=6)throw std::runtime_error("Bug requires flat six-field identity");
        r.bug={static_cast<ParentKind>(codec_detail::i32(b[0])),b[1].get<std::string>(),
            {static_cast<BirthKind>(codec_detail::i32(b[2])),b[3].get<std::string>(),codec_detail::u64(b[4]),codec_detail::u64(b[5])}};
        if(!codec_detail::uuid(r.token)||!codec_detail::uuid(r.origin)||!r.epoch||!r.generation)throw std::runtime_error("canonical UUID and positive epoch/generation required");
        return work_codec::identity(r.bug,e);
    }catch(const std::exception& ex){e=std::string("BugButton UI arguments: ")+ex.what();return false;}
}
struct Control {uintptr_t window=0,tabs=0,view=0,page=0,detail=0,button=0;bool operator==(const Control&)const=default;};
template<class Copy>bool tabs(Copy&& copy,const developer_staff::Capture& c,Control& out,std::string& e){
    developer_staff::bug_policy::Snapshot s;s.capture=c;developer_staff::bug_policy::Control ctl;
    if(!developer_staff::bug_policy::read_tabs(copy,s,ctl,e))return false;
    out={ctl.window,ctl.tabs,ctl.view};return true;
}
template<class Copy>bool page(Copy&& copy,uintptr_t image,uintptr_t view,const char* name,uintptr_t& current,std::string& e){
    namespace d=developer_staff::bug_policy::detail;
    std::string requested,id;uintptr_t next=0;int32_t transition=0;
    if(!d::text(copy,view,0x3e8,requested,e)||!d::at(copy,view,0x410,current,e)||!d::at(copy,view,0x418,next,e)||!d::at(copy,view,0x408,transition,e))return false;
    if(requested!=name||!current||next!=current||transition){e="BugButton original page not settled: "+std::string(name)+" transition="+std::to_string(transition);return false;}
    uintptr_t page_ap=0;if(!d::at(copy,current,0,page_ap,e))return false;
    if(page_ap!=image+0x1745a80){e="BugButton page "+std::string(name)+" requires original mmoView AP0x1745A80";return false;}
    if(!d::text(copy,current,0x1b0,id,e))return false;
    if(id!=name){e="BugButton current page name differs";return false;}return true;
}
template<class Copy>bool visible(Copy&& copy,const developer_staff::Capture& c,Control& out,std::string& e){
    namespace d=developer_staff::bug_policy::detail;uint8_t shown=0;
    if(!tabs(copy,c,out,e)||!d::at(copy,out.window,0x45f,shown,e))return false;
    if(shown!=1){e="BugButton Development window is not visible";return false;}
    return page(copy,c.image,out.view,"{dev_tab_bugs}",out.page,e);
}
template<class Copy>bool button(Copy&& copy,const developer_staff::Capture& c,uintptr_t bug,bool pressed,Control& out,std::string& e){
    namespace d=developer_staff::bug_policy::detail;
    uintptr_t selected=0,details=0;
    if(!visible(copy,c,out,e)||!d::at(copy,out.window,0x558,out.detail,e)||!selected_bug(copy,c,out.detail,selected,e))return false;
    if(selected!=bug){e="BugButton selected stable Bug differs";return false;}
    if(!page(copy,c.image,out.detail,"details",details,e)||!d::at(copy,out.detail,0x450,out.button,e)||!d::type(copy,out.button,c.image+0x16e3ec0,e))return false;
    uint8_t disabled=0,toggle=0,press_activation=0,down=0,clickable=0;uintptr_t on_click=0;std::string command;
    if(!d::at(copy,out.button,0x46d,disabled,e)||!d::at(copy,out.button,0x46c,toggle,e)||!d::at(copy,out.button,0x46e,press_activation,e)||
       !d::at(copy,out.button,0x584,down,e)||!d::at(copy,out.button,0x174,clickable,e)||!d::text(copy,out.button,0x388,command,e)||
       !d::at(copy,c.image+0x16e3ec0,0x1c8,on_click,e))return false;
    if(disabled||toggle||press_activation||down!=uint8_t(pressed)||clickable!=1||command!="MessageTo Parent BugButton"||on_click!=c.image+0xb63ee0){
        e="BugButton original enabled/pressed/release command contract differs";return false;}
    // Parent commands walk native pane ancestry. Require the exact owned detail
    // view, with only the original page/view wrappers between it and the button.
    uintptr_t node=out.button;std::set<uintptr_t> seen;bool details_seen=false;
    while(node!=out.detail){
        if(!node||!seen.insert(node).second||seen.size()>8){e="BugButton parent chain is absent/cyclic or exceeds eight panes";return false;}
        uintptr_t parent=0,window_owner=0,ap=0;
        if(!d::at(copy,node,0x278,parent,e)||!d::at(copy,node,0x280,window_owner,e)||!d::at(copy,node,0,ap,e))return false;
        if(window_owner!=out.window||(node!=out.button&&ap!=c.image+0x1745a80)){e="BugButton pane ancestry/class/window owner differs";return false;}
        details_seen|=node==details;node=parent;
    }
    if(!details_seen){e="BugButton is outside the settled detail page";return false;}return true;
}
struct NativeCall {bool returned=false,accepted=false,constructed=false,record_confirmed=false,cleanup_confirmed=false;uint32_t fault=0,cleanup_fault=0;};
void original_select(uintptr_t,uintptr_t,int32_t,NativeCall*)noexcept;
uint32_t original_page(uintptr_t,uintptr_t)noexcept;
uint32_t original_input(uintptr_t,uintptr_t,Input)noexcept;
// A failed phase stops the sequence; the retained mask makes partial UI input
// distinguishable from an input that was never attempted.
template<class Port>bool click(Port& port,uint32_t& attempted,uint32_t& returned,std::string& e){
    for(const auto input:{Input::Hover,Input::Press,Input::Release}){
        const auto bit=uint32_t(1)<<unsigned(input);
        if(!port.before(input,e))return false;
        attempted|=bit;if(!port.input(input,e))return false;returned|=bit;
        if(!port.after(input,e))return false;
    }return true;
}
}
}
