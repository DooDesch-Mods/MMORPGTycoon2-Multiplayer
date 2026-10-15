#include "tutorial_replication/start_capture.h"
#include "tutorial_replication/start_ledger.h"
#include "tutorial_replication/owned_pay_entry.h"
#include "tutorial_replication/start_window.h"
#include "tutorial_replication/start_presentation.h"
#include "tutorial_replication/start_apply.h"
#include <iostream>
#include <map>
#include <stdexcept>
namespace start=mt2mp::game::tutorial_replication::start_intent;
namespace action=mt2mp::action_session;
namespace {
unsigned checks=0;
void need(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
void pass(const char* name){++checks;std::cout<<"PASS "<<name<<'\n';}
struct Calls {
    unsigned originals=0,submits=0;bool capture_ok=true,submit_ok=true,throws=false;
    bool original(){++originals;return true;}
    bool capture(std::string& e){if(throws)throw std::runtime_error("original native UI read failed");if(!capture_ok){e="original owned window rejected";return false;}return true;}
    bool action_uuid(std::string& out,std::string&){out="11111111-1111-4111-8111-111111111111";return true;}
    bool submit(const std::string&,std::string& e){++submits;if(!submit_ok){e="original Controller busy";return false;}return true;}
};
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;std::map<uintptr_t,unsigned> reads;uintptr_t failed=0,unstable=0;
    template<class T>void store(uintptr_t p,const T& value){const auto* data=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof(T);++i)bytes[p+i]=data[i];}
    bool copy(uintptr_t p,void* out,size_t size,std::string& e){if(p==failed){e="original memory read failed";return false;}if(++reads[p]==2&&p==unstable)bytes[p]^=1;auto* data=static_cast<unsigned char*>(out);for(size_t i=0;i<size;++i){auto it=bytes.find(p+i);if(it==bytes.end()){e="fixture missing byte";return false;}data[i]=it->second;}return true;}
};
constexpr uintptr_t native_image=0x140000000,fixture_mode=0x200000,array=0x300000,ui=0x400000,name=0x500000,tabs=0x600000,source=0x700000;
auto snapshot(Memory& m){
    mt2mp::game::tutorial_development::StartingpointSnapshot out;out.image=native_image;out.origin.mode=reinterpret_cast<void*>(fixture_mode);out.record=0x800000;out.row=mt2mp::game::tutorial_replication::startingpoint::Row{};out.creation=mt2mp::game::tutorial_development::CreationOwners{};out.creation->source=source;
    m.store(fixture_mode+0xa0,array);m.store<int32_t>(fixture_mode+0xa8,1);m.store<int32_t>(fixture_mode+0xac,2);m.store(array,ui);
    m.store(ui,native_image+0x1722e60);m.store<uintptr_t>(ui+0x318,0);m.store<uint8_t>(ui+0x45f,1);m.store(ui+0x540,tabs);m.store(ui+0x610,source);m.store(ui+0x1b0,name);m.store<uint64_t>(ui+0x1b8,11);const char text[]="development";for(size_t i=0;i<sizeof(text);++i)m.bytes[name+i]=static_cast<unsigned char>(text[i]);return out;
}
constexpr uintptr_t entry_address=0x100000,relay_address=0x110000,hook_address=0x120000;
struct Code:Memory {
    mt2mp::game::tutorial_sync::OwnedSequenceEntry owner{entry_address,hook_address,relay_address,true,true};
    std::string expected;
    Code(){std::vector<unsigned char> body(start::pay_size);for(size_t i=0;i<body.size();++i)body[i]=static_cast<unsigned char>((i*17+11)%251);const unsigned char prefix[]={0x53,0x48,0x83,0xec,0x20};std::copy_n(prefix,5,body.begin());expected=mt2mp::sha256(body);for(size_t i=0;i<body.size();++i)bytes[entry_address+i]=body[i];for(size_t i=0;i<5;++i)bytes[relay_address+i]=body[i];const unsigned char jump[]={0xff,0x25,0,0,0,0};for(size_t i=0;i<6;++i){bytes[relay_address+5+i]=jump[i];bytes[relay_address+19+i]=jump[i];}store<uintptr_t>(relay_address+11,entry_address+5);store<uintptr_t>(relay_address+25,hook_address);}
    void enable(){bytes[entry_address]=0xe9;store<int32_t>(entry_address+1,int32_t(relay_address+19-entry_address-5));}
    bool verify(bool owned,std::string& e){return start::verify_pay(owned?&owner:nullptr,entry_address,[&](uintptr_t p,void* data,size_t n,std::string& error){return copy(p,data,n,error);},e,expected.c_str());}
};
action::Ledger ledger(){action::Ledger row;row.cash=row.record_cash=800;row.flat_day=row.record_day=9;row.record_present=1;row.record_expense=10;return row;}
using Row=mt2mp::game::tutorial_replication::startingpoint::Row;
struct ApplyCalls {
    action::Ledger current=ledger(),after=current;Row row{},target{};
    unsigned pays=0,creates=0,reads=0;bool ready=true,pay_ok=true,create_ok=true,bad_paid=false,bad_final=false,throw_pay=false;
    ApplyCalls(int price=0){after.cash-=price;after.record_cash-=price;after.record_expense+=price;++after.line_counts[3];after.line_income[3]-=price;target.purchased=target.project_present=true;}
    bool read(action::Ledger& out,Row& value,std::string&){++reads;out=current;value=row;return true;}
    bool preflight(std::string& e){if(!ready)e="actual economic action scope unavailable";return ready;}
    bool pay(std::string& e){++pays;if(throw_pay)throw std::runtime_error("native Pay exception");if(!pay_ok){e="original native Pay rejected";return false;}current=after;row.purchased=true;if(bad_paid)++current.line_income[19];return true;}
    bool create(const Row& paid,const Row& destination,std::string& e){++creates;need(paid.purchased&&!paid.project_present,"adopter did not receive already-purchased baseline");if(!create_ok){e="original SetCurrent readback failed";return false;}row=destination;if(bad_final)--current.cash;return true;}
};
}
int main(){try{
    {need(start::route(true,true,native_image,native_image+0x6c7e02)==start::Route::ClientIntent,"exact Client native Pay origin not selected");need(start::route(false,true,native_image,native_image+0x6c7e02)==start::Route::Original&&start::route(true,false,native_image,native_image+0x6c7e02)==start::Route::Original&&start::route(true,true,native_image,native_image+1)==start::Route::Original,"foreign Pay origin swallowed");pass("exact Client Mainthread UI return selects intent while foreign routes remain original");}
    {Calls c;start::Receipt r;need(!start::dispatch(start::Route::ClientIntent,c,r)&&r.suppressed&&r.submitted&&!r.original_attempted&&!c.originals&&c.submits==1,"Client intent paid locally or failed native false return");pass("Client capture submits once and returns original false path without local Pay");}
    {Calls c;c.capture_ok=false;start::Receipt r;need(!start::dispatch(start::Route::ClientIntent,c,r)&&r.suppressed&&!r.submitted&&!c.originals&&!c.submits&&r.cause=="original owned window rejected","capture rejection fell through to original Pay");pass("failed exact Client ownership never falls through to Pay");}
    {Calls c;c.submit_ok=false;start::Receipt r;need(!start::dispatch(start::Route::ClientIntent,c,r)&&!r.submitted&&!c.originals&&c.submits==1&&r.cause=="original Controller busy","Controller rejection was hidden or retried");pass("Controller rejection preserves cause with no native fallback or retry");}
    {Calls c;c.throws=true;start::Receipt r;need(!start::dispatch(start::Route::ClientIntent,c,r)&&!c.originals&&r.cause.find("original native UI read failed")!=std::string::npos,"C++ exception escaped Client native UI capture");pass("Client read exception stays visible without crossing the GNU UI boundary");}
    {Calls c;start::Receipt r;need(start::dispatch(start::Route::Original,c,r)&&c.originals==1&&!c.submits&&r.original_attempted,"foreign Pay did not preserve original once");pass("foreign native Pay retains original return and one invocation");}
    {Code c;std::string e;need(c.verify(false,e),"synthetic unpatched contract rejected");c.enable();need(c.verify(true,e),"exact owned physical route rejected");pass("full-body fingerprint accepts original or exact owned reconstructed Pay route");}
    {Code c;c.enable();c.bytes[entry_address+start::pay_size-1]^=1;std::string e;need(!c.verify(true,e)&&e=="Startingpoint Pay full original body differs","Pay suffix mutation accepted under own hook");pass("owned Pay still checks the entire original function suffix");}
    {Code c;c.enable();c.store<uintptr_t>(relay_address+25,hook_address+1);std::string e;need(!c.verify(true,e)&&e=="Startingpoint Pay trampoline detour differs","foreign detour inherited ownership");pass("hook owner must match exact physical relay and detour");}
    {Code c;c.enable();c.failed=relay_address;std::string e;need(!c.verify(true,e)&&e=="original memory read failed","owned-hook read cause replaced");pass("owned-hook read failure retains original cause");}
    {Memory m;auto native=snapshot(m);start::Window w;std::string e;need(start::window(m,native,source,w,e)&&w.address==ui&&w.tabs==tabs&&w.selected==source,"owned Window/current catalogue capture rejected");pass("actual Mode-owned Development window and selected catalogue bind capture");}
    {Memory m;auto native=snapshot(m);m.store<uintptr_t>(ui+0x610,source+8);start::Window w;std::string e;need(!start::window(m,native,source,w,e)&&e.find("selected catalogue")!=std::string::npos,"foreign selected UI project admitted");pass("selection must match the actual Pay receiver and owned catalogue source");}
    {Memory m;auto native=snapshot(m);m.store<int32_t>(fixture_mode+0xa8,2);m.store(array+8,ui);start::Window w;std::string e;need(!start::window(m,native,source,w,e)&&e.find("duplicated")!=std::string::npos,"duplicated native Window owner admitted");pass("duplicate Window ownership rejects visibly");}
    {Memory m;auto native=snapshot(m);m.unstable=ui;start::Window w;std::string e;need(!start::window(m,native,source,w,e)&&e.find("changed during capture")!=std::string::npos,"UI lifetime changed without rejection");pass("UI owner witnesses are double-read before submission");}
    {Memory m;auto native=snapshot(m);native.project=0x810000;native.row->purchased=native.row->project_present=true;start::Window w;std::string e;need(!start::window(m,native,source,w,e)&&start::window(m,native,source,w,e,true),"post-creation presentation reused pre-purchase admission");need(start::route(true,true,0,start::ui_return_rva)==start::Route::Original,"missing actual image admitted UI origin");pass("presentation rebind admits only the actual created project and original capture still rejects it");}
    {auto before=ledger(),after=before;++after.line_counts[3];std::string e;need(start::payment(before,after,e),"original zero-price category3 count not admitted");pass("zero-price purchase still requires category3 count increment");}
    {auto before=ledger(),after=before;after.cash-=100;after.record_cash-=100;after.record_expense+=100;++after.line_counts[3];after.line_income[3]-=100;std::string e;need(start::payment(before,after,e),"exact original positive-price ledger not admitted");++after.line_income[19];need(!start::payment(before,after,e),"unrelated ledger group change hidden");pass("full original positive-price cash/record/category3 delta excludes every other ledger field");}
    {auto before=ledger(),after=before;--after.cash;std::string e;need(!start::payment(before,after,e),"cash-only fake purchase admitted");after=before;++after.line_counts[2];need(!start::payment(before,after,e),"wrong category purchase admitted");pass("cash-only or wrong-category fake purchase cannot acknowledge Start");}
    {ApplyCalls c;start::Application r;std::string e;need(start::apply(ledger(),c.after,Row{},c.target,c,r,e)&&r.confirmed&&!r.unknown&&r.payment_confirmed&&c.pays==1&&c.creates==1,"zero-price typed Start did not finish before ACK");need(c.current.line_counts[3]==1,"zero-price Pay skipped category3");pass("typed zero-price Pay then already-purchased Creation produces full AFTER before ACK");}
    {ApplyCalls c(100);start::Application r;std::string e;need(start::apply(ledger(),c.after,Row{},c.target,c,r,e)&&c.current.cash==700&&c.pays==1&&c.creates==1,"positive-price typed Start failed");need(!start::apply(ledger(),c.after,Row{},c.target,c,r,e)&&c.pays==1&&c.creates==1&&e.find("no replay")!=std::string::npos,"confirmed action paid again");pass("positive-price full accounting confirms once and retained receipt rejects replay");}
    {ApplyCalls c;c.current=c.after;start::Application r;std::string e;need(!start::apply(ledger(),c.after,Row{},c.target,c,r,e)&&!r.attempted&&!c.pays&&!c.creates,"already-applied AFTER frame was charged twice");pass("AFTER ledger cannot reenter typed BEFORE path even when purchase price is zero");}
    {ApplyCalls c;c.ready=false;start::Application r;std::string e;need(!start::apply(ledger(),c.after,Row{},c.target,c,r,e)&&!r.attempted&&!c.pays&&e=="actual economic action scope unavailable","failed owner admitted Pay");pass("missing actual economic owner fails before native Pay");}
    {ApplyCalls c;c.pay_ok=false;start::Application r;std::string e;need(!start::apply(ledger(),c.after,Row{},c.target,c,r,e)&&r.unknown&&r.attempted&&!r.confirmed&&!c.creates&&e=="original native Pay rejected","failed Pay allowed creation");need(!start::apply(ledger(),c.after,Row{},c.target,c,r,e)&&c.pays==1,"failed Pay replayed");pass("attempted Pay failure remains Unknown and cannot retry or create");}
    {ApplyCalls c;c.bad_paid=true;start::Application r;std::string e;need(!start::apply(ledger(),c.after,Row{},c.target,c,r,e)&&r.unknown&&!r.payment_confirmed&&!c.creates,"unrelated paid ledger change hidden");pass("Pay readback compares all ledger groups before any creation");}
    {ApplyCalls c;c.create_ok=false;start::Application r;std::string e;need(!start::apply(ledger(),c.after,Row{},c.target,c,r,e)&&r.payment_confirmed&&r.unknown&&!r.confirmed&&e=="original SetCurrent readback failed","partial creation cause hidden");pass("paid but uncertain creation retains original cause and Unknown");}
    {ApplyCalls c;c.bad_final=true;start::Application r;std::string e;need(!start::apply(ledger(),c.after,Row{},c.target,c,r,e)&&r.unknown&&!r.confirmed,"creation altered cash after paid Pin");pass("creation must preserve the entire paid ledger");}
    {ApplyCalls c;c.throw_pay=true;start::Application r;std::string e;need(!start::apply(ledger(),c.after,Row{},c.target,c,r,e)&&r.consumed&&r.unknown&&!c.creates&&e.find("native Pay exception")!=std::string::npos,"throwing original lost uncertainty");pass("exception after consumed Pay remains visible Unknown");}
    {ApplyCalls c;c.target.inventory_count=1;start::Application r;std::string e;need(!start::apply(ledger(),c.after,Row{},c.target,c,r,e)&&!r.attempted&&!c.pays,"invented Host inventory delta admitted");pass("invalid Host creation evidence fails before payment");}
    {auto before=ledger(),after=before;before.line_counts[3]=after.line_counts[3]=INT32_MAX;std::string e;need(!start::payment(before,after,e)&&e.find("overflow")!=std::string::npos,"category3 counter overflow admitted");pass("category3 count overflow fails before effects");}
    std::cout<<checks<<" bounded Start intent scenarios passed\n";return 0;
}catch(const std::exception& cause){std::cerr<<"error: "<<cause.what()<<'\n';return 1;}}
