#include "mt2mp/net/chat_core.h"
#include "vendor/nlohmann/json.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
#include <atomic>
#include <type_traits>
namespace net=mt2mp::net;
using Json=nlohmann::json;
namespace fixture {
unsigned checks=0,submits=0,input_reads=0,input_writes=0;
bool member=true,submit_ok=true,read_ok=true,write_ok=true,readback_drift=false;
std::string draft,submitted;std::map<std::string,std::string> labels;
bool send_disabled=false;unsigned char input[0x300]{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
}
namespace mt2mp::net::chat {
struct Snapshot {bool connected=true,retired=false;uint8_t local_peer=2;std::vector<Entry> entries;};
Snapshot current;
struct Runtime {
    static Runtime& instance(){static Runtime runtime;return runtime;}
    Snapshot snapshot()const{return current;}
    bool submit(const std::string& value,std::string& error,uint64_t* sequence){
        ++fixture::submits;fixture::submitted=value;
        if(!fixture::submit_ok){error="original chat queue failure";return false;}
        *sequence=fixture::submits;return true;
    }
};
}
unsigned command_failures=0;
std::string chat_error,last_chat_transcript,last_chat_status,cause;
uint64_t chat_last_sequence=0;
bool chat_draft_clear_failed=false;
bool admitted(){return fixture::member;}
void* pane(const char* id){return std::string(id)=="chat_input"?fixture::input:nullptr;}
bool text(const void* pointer,std::string& value,size_t limit){
    fixture::check(pointer==fixture::input+0x2d0,"native input field offset retained");
    ++fixture::input_reads;
    if(!fixture::read_ok||fixture::draft.size()>limit)return false;
    value=fixture::draft+(fixture::readback_drift?"changed":"");return true;
}
bool label(const char* id,const std::string& value){
    if(std::string(id)=="chat_input"){
        ++fixture::input_writes;
        if(!fixture::write_ok){cause="original native setter failure";++command_failures;return false;}
        fixture::draft=value;
    }
    fixture::labels[id]=value;return true;
}
bool disable(const char* id,bool value){fixture::check(std::string(id)=="chat_send","only native Send disabled");fixture::send_disabled=value;return true;}
#define LOG_WARN(...) ((void)0)
#include "native_chat_body.inc"
uintptr_t image=0x1000;unsigned thread=1,current_thread=1;int ui_mode=3;
unsigned char game_storage=0,owner_storage=0,panel_storage=0;
void* ui_owner=&owner_storage;void* panel=&panel_storage;bool choosing=false;
bool toggle_read_ok=true,toggle_write=true,toggle_owner_drift=false;
uint8_t toggle_visible=0;unsigned show_calls=0,show_exception=0;
unsigned GetCurrentThreadId(){return current_thread;}
bool fail(const std::string& error){++command_failures;cause=error;return false;}
template<class T>bool read(const void* base,size_t offset,T& value){
    if constexpr(std::is_same_v<T,uint8_t>){
        fixture::check(base==panel&&offset==0x45f,"exact native Window visibility field");
        value=toggle_visible;return toggle_read_ok;
    }else{
        if(base==reinterpret_cast<void*>(image)&&offset==0x13df8f0)value=&game_storage;
        else if(base==&game_storage&&(offset==0x28||offset==0x350||offset==0x358))value=toggle_owner_drift&&show_calls?nullptr:ui_owner;
        else return false;
        return true;
    }
}
unsigned show(void* window,bool visible){
    fixture::check(window==panel,"only current registered panel shown");++show_calls;
    if(!show_exception&&toggle_write)toggle_visible=uint8_t(visible);
    return show_exception;
}
#include "native_lobby_toggle.inc"
#include "native_lobby_toggle_request.inc"
namespace game::native_lobby {void request_panel_toggle(){::request_panel_toggle();}}
constexpr unsigned SDL_EVENT_KEY_DOWN=1,SDL_EVENT_KEY_UP=2,SDL_SCANCODE_F10=10;
struct KeyEvent {unsigned type;struct {unsigned scancode;bool repeat;}key;};
unsigned feed_keys(const std::vector<KeyEvent>& keys){unsigned forwarded=0;for(const auto& item:keys){const auto* event=&item;
#include "native_lobby_f10.inc"
    ++forwarded;
}return forwarded;}
void toggle_cases(){
    using fixture::check;std::string error;
    toggle_requested=0;toggle_processed=toggle_attempted=toggle_returned=toggle_confirmed=toggle_failed=0;
    toggle_error.clear();toggle_first_error.clear();
    check(feed_keys({{SDL_EVENT_KEY_DOWN,{SDL_SCANCODE_F10,false}},{SDL_EVENT_KEY_DOWN,{SDL_SCANCODE_F10,true}},{SDL_EVENT_KEY_UP,{SDL_SCANCODE_F10,false}}})==0&&toggle_requested==1&&!show_calls,"F10 repeat/up consumed, only initial down queues, no native call on input thread");
    check(process_panel_toggle(true,error)&&toggle_visible==1&&toggle_processed==1&&toggle_attempted==1&&toggle_returned==1&&toggle_confirmed==1,"game thread opens hidden current panel with confirmed readback");
    check(process_panel_toggle(true,error)&&show_calls==1,"processed request never replays");
    request_panel_toggle();check(process_panel_toggle(true,error)&&toggle_visible==0&&toggle_confirmed==2,"second distinct F10 closes native panel");
    choosing=true;request_panel_toggle();check(!process_panel_toggle(true,error)&&show_calls==2&&toggle_failed==1,"browser mode rejected before native attempt");choosing=false;
    const auto first=toggle_first_error;request_panel_toggle();check(!process_panel_toggle(false,error)&&show_calls==2&&toggle_first_error==first,"unsupported mode rejected with first cause retained");
    current_thread=2;request_panel_toggle();check(!process_panel_toggle(true,error)&&show_calls==2,"wrong thread rejects native toggle");current_thread=1;
    toggle_read_ok=false;request_panel_toggle();check(!process_panel_toggle(true,error)&&show_calls==2,"unreadable visibility rejects before Show");toggle_read_ok=true;
    show_exception=5;request_panel_toggle();check(!process_panel_toggle(true,error)&&toggle_attempted==3&&toggle_returned==2&&toggle_confirmed==2&&error.find("exception=5")!=std::string::npos,"native exception remains attempted, unreturned and unconfirmed");show_exception=0;
    const auto failed_calls=show_calls;check(process_panel_toggle(true,error)&&show_calls==failed_calls,"failed native attempt never retries");
    toggle_write=false;request_panel_toggle();check(!process_panel_toggle(true,error)&&toggle_returned==3&&toggle_confirmed==2,"returned Show without matching flag is not confirmation");toggle_write=true;
    toggle_owner_drift=true;show_calls=0;request_panel_toggle();check(!process_panel_toggle(true,error)&&error.find("owner changed after")!=std::string::npos,"owner drift after callback rejects readback");toggle_owner_drift=false;
    request_panel_toggle();check(process_panel_toggle(true,error)&&toggle_error.empty()&&toggle_first_error==first,"fresh request can recover while first failure remains visible");
    const auto result=panel_toggle_status();check(result.at("requested")==result.at("processed")&&result.at("failed")==7,"readback distinguishes queued, processed, failed and confirmed");
}
void dispatch_send(){const std::string cmd="MT2MPChatSend";
#include "native_chat_dispatch.inc"
}
Json send_receipt(){
    const auto failed=command_failures;const auto chat_sequence_before=chat_last_sequence;
    dispatch_send();Json result;const std::string button="chat_send";const bool pending=false;
#include "native_chat_receipt.inc"
    return result;
}
bool validate(const Json& args,std::string& error){const auto button=args.at("button").get<std::string>();
#include "native_chat_args.inc"
    return true;
}
bool set_input(const Json& args,std::string& error){const auto button=args.at("button").get<std::string>();
#include "native_chat_input.inc"
    return true;
}
void reset(){
    fixture::submits=fixture::input_reads=fixture::input_writes=command_failures=0;
    fixture::member=fixture::submit_ok=fixture::read_ok=fixture::write_ok=true;fixture::readback_drift=false;
    fixture::draft="typed native draft";fixture::submitted.clear();fixture::labels.clear();fixture::send_disabled=false;
    net::chat::current={};chat_error.clear();cause.clear();last_chat_transcript.clear();last_chat_status.clear();chat_last_sequence=0;chat_draft_clear_failed=false;
}
int main(){try{
    using fixture::check;std::string error;
    reset();check(render_chat(error)&&fixture::draft=="typed native draft"&&!fixture::input_writes&&!fixture::send_disabled,"render preserves native draft and focus route");
    check(send_chat()&&fixture::submitted=="typed native draft"&&fixture::draft.empty()&&chat_last_sequence==1,"Send reads exact native draft then clears after accepted queue");
    reset();fixture::submit_ok=false;
    check(!send_chat()&&fixture::draft=="typed native draft"&&!fixture::input_writes&&chat_error=="original chat queue failure","failed submit preserves draft and original cause");
    reset();fixture::write_ok=false;
    const auto clear_failed=send_receipt();
    check(clear_failed.at("chat_queued")==true&&clear_failed.at("handler_failed")==true&&clear_failed.at("chat_draft_clear_failed")==true&&fixture::submits==1&&chat_last_sequence==1,"clear failure reports failed UI operation while retaining actual queue admission");
    check(clear_failed.at("chat_error")=="chat queued sequence=1 but native draft clear failed; no replay: original native setter failure"&&command_failures==1,"clear failure retains first cause and is counted once");
    check(render_chat(error)&&fixture::labels["chat_status"]=="Chat - Nachricht vorgemerkt, Eingabe nicht geleert"&&fixture::draft=="typed native draft"&&fixture::submits==1,"clear-failed UI reports queued message and retained draft without replay");
    reset();fixture::submit_ok=false;const auto queue_failed=send_receipt();
    check(queue_failed.at("chat_queued")==false&&queue_failed.at("handler_failed")==true&&queue_failed.at("chat_draft_clear_failed")==false&&fixture::draft=="typed native draft","ordinary queue failure remains unqueued and preserves draft");
    check(render_chat(error)&&fixture::labels["chat_status"]=="Chat - Nachricht nicht gesendet","ordinary queue rejection keeps truthful not-sent state");
    for(const auto& invalid:std::vector<std::string>{"",std::string(241,'x'),"bad\nline",std::string("\xc2\x80"),std::string("\xc0\xaf")}){
        reset();fixture::draft=invalid;check(!send_chat()&&!fixture::submits&&!fixture::input_writes,"invalid UTF-8/control/length never submitted or clears draft");
        check(!validate({{"acceptance_token","test"},{"button","chat_send"},{"text",invalid}},error),"acceptance validation uses same text contract");
    }
    reset();fixture::draft="Grüße aus der Lobby";check(send_chat()&&fixture::submits==1,"native UTF-8 draft accepted");
    reset();net::chat::current.connected=false;net::chat::current.retired=true;
    check(render_chat(error)&&fixture::send_disabled&&fixture::labels["chat_status"]=="Chat beendet","Host retirement or chat failure closes UI admission");
    check(!send_chat()&&!fixture::submits&&fixture::draft=="typed native draft","retired chat cannot send and preserves draft");
    reset();fixture::member=false;check(!send_chat()&&!fixture::input_reads,"no confirmed native lobby rejects before reading draft");
    reset();
    for(unsigned i=1;i<=4;++i){net::chat::Entry entry;entry.origin=uint8_t(i==4?2:1);entry.sequence=i;entry.text="message"+std::to_string(i);entry.delivery=i==4?net::chat::Delivery::Failed:net::chat::Delivery::Received;net::chat::current.entries.push_back(entry);}
    check(render_chat(error)&&fixture::labels["chat_transcript"]=="Host: message2\nHost: message3\nDu: message4 - Nicht zugestellt","recent transcript preserves supplied Host order and delivery failure");
    check(validate({{"acceptance_token","test"},{"button","chat_send"}},error),"omitted text uses current draft");
    check(!validate({{"acceptance_token","test"},{"button","leave"},{"text","hello"}},error),"text field restricted to Send");
    check(set_input({{"button","chat_send"},{"text","native field"}},error)&&fixture::draft=="native field","native setter receives exact provided text and readback");
    fixture::readback_drift=true;check(!set_input({{"button","chat_send"},{"text","native field"}},error)&&error=="native chat input setter readback differs; Send not attempted","readback drift blocks activation");
    const auto writes=fixture::input_writes;check(set_input({{"button","chat_send"}},error)&&fixture::input_writes==writes,"omitted text never overwrites human draft");
    toggle_cases();
    std::cout<<fixture::checks<<" actual native lobby chat assertions passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
