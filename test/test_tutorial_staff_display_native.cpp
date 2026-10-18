#include "tutorial_replication/staff_display.h"
#include "tutorial_development.h"
#include "typed_action/action_session.h"
#include "mt2mp/game/native_engine.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "live_host_state/memory_query_timing.h"
#define LOG_INFO(...) ((void)0)
#define LOG_ERROR(...) ((void)0)
namespace tr=mt2mp::game::tutorial_replication;
namespace development=mt2mp::game::tutorial_development;
namespace action_session=mt2mp::action_session;
using World=mt2mp::game::building_mixed_identity::opened_file::CurrentWorld;
using json=nlohmann::json;
namespace fixture {
unsigned cases=0;
std::map<uintptr_t,unsigned char> bytes;
std::map<uintptr_t,std::string> strings;
std::map<uintptr_t,json> labels;
development::StartingpointSnapshot snapshot;
World current;bool origin_ok=true,read_ok=true;unsigned origins=0,development_reads=0;
bool complete=false;int summary_calls=0,project_calls=0,developer_calls=0;
unsigned original_calls=0,clock_reads=0;bool throw_string=false;
std::vector<int> setter_rows;std::string setter_fail;bool mismatch=false;
constexpr uintptr_t module=0x140000000,mode=0x10000,state=0x20000,business=0x30000,lounge=0x40000,project=0x50000;
constexpr uintptr_t window=0x70000,windows=0x71000,tabs=0x72000,view=0x73000,staff=0x74000;
constexpr uintptr_t project_view=0x75000,dev=0x76000,summary=0x77000,rows=0x78000,fields=0x79000;
constexpr uintptr_t project_text=0x7a000,grid=0x7b000,grid_fields=0x7c000,cell0=0x7d000,cell1=0x7e000;
void need(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
void pass(const char* name){++cases;std::cout<<"PASS "<<name<<'\n';}
template<class T>void put(uintptr_t p,T value){const auto* b=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof(value);++i)bytes[p+i]=b[i];}
void child(uintptr_t p,uintptr_t addresspoint,uintptr_t parent){put(p,module+addresspoint);put(p+0x280,window);put(p+0x278,parent);}
void reset(){
    bytes.clear();strings.clear();labels.clear();origins=development_reads=0;origin_ok=read_ok=true;complete=false;
    summary_calls=project_calls=developer_calls=0;setter_rows.clear();setter_fail.clear();mismatch=false;
    original_calls=clock_reads=0;throw_string=false;
    snapshot={};snapshot.image=module;snapshot.origin.mode=reinterpret_cast<void*>(mode);snapshot.origin.state=reinterpret_cast<void*>(state);
    snapshot.origin.world=reinterpret_cast<void*>(0x60000);snapshot.origin.uuid="11111111-1111-4111-8111-111111111111";
    snapshot.origin.world_epoch=4;snapshot.origin.generation=7;current=snapshot.origin;
    snapshot.lounge=lounge;snapshot.project=project;snapshot.tech=0x60010;snapshot.record=0x60020;
    snapshot.row=tr::startingpoint::Row{};snapshot.row->project_present=snapshot.row->purchased=true;
    snapshot.creation=development::CreationOwners{};
    snapshot.creation->current=development::ProjectSource{project,0,0,"startingpoint",""};
    snapshot.creation->developer_data=0x80000;
    put(state+0x1c0,business);put<int32_t>(business+0x4c,2);put<int32_t>(lounge+0x310,2);
    put(lounge+0x70,snapshot.creation->developer_data);put<int32_t>(lounge+0x78,50);
    for(int row=0;row<50;++row){const auto address=uintptr_t(0x100000+row*0x1000);int id=row;
        if(row==0)id=41;else if(row==41)id=0;
        snapshot.creation->developers.push_back({address,0});
        put(snapshot.creation->developer_data+size_t(row)*8,address);
        put(address,module+0x17422e0);put<int32_t>(address+0x4c0,id);put<int32_t>(address+0x4c4,0);
        put<uint8_t>(address+0x4d8,uint8_t(row<2));put<uint8_t>(address+0x4d9,uint8_t(row<2));put<uint8_t>(address+0x368,0);
    }
    put(state+0x180,lounge);put(lounge+0xa8,project);put(project,module+0x16e48c0);put<int32_t>(project+0x18,0);strings[project+0x20]="startingpoint";
    put(mode+0xa0,windows);put<int32_t>(mode+0xa8,1);put<int32_t>(mode+0xac,1);put(windows,window);
    put(window,module+0x1722e60);strings[window+0x1b0]="development";put<uint8_t>(window+0x45f,1);put<uint8_t>(window+0x460,0);put<uintptr_t>(window+0x318,0);
    put(window+0x540,tabs);child(tabs,0x17015b0,window);put(tabs+0x3b8,view);
    child(view,0x16dfe90,tabs);put(view+0x410,staff);put<int32_t>(view+0x408,0);strings[view+0x3e8]="{dev_tab_staff}";
    child(staff,0,view);strings[staff+0x1b0]="{dev_tab_staff}";
    put(window+0x568,project_view);child(project_view,0x16dfe90,window);put(project_view+0x410,dev);put<int32_t>(project_view+0x408,0);strings[project_view+0x3e8]="dev";
    child(dev,0,project_view);strings[dev+0x1b0]="dev";
    put(window+0x508,summary);child(summary,0x1700630,staff);put(summary+0x3e0,rows);put<int32_t>(summary+0x3e8,2);put<int32_t>(summary+0x3ec,2);
    put(rows+0x28,fields);put<int32_t>(rows+0x30,2);put<int32_t>(rows+0x34,2);
    put(window+0x5c8,project_text);child(project_text,0x16dc680,dev);
    put(window+0x510,grid);child(grid,0x16dade0,staff);put<int32_t>(grid+0x314,3);put(grid+0x3d8,grid_fields);put<int32_t>(grid+0x3e0,6);put<int32_t>(grid+0x3e4,6);
    put(grid_fields+8,cell0);put(grid_fields+32,cell1);put(cell0,module+0x1789a90);put(cell1,module+0x1789a90);
}
tr::staff_display::State target(){return {1,1,{{1,0,false},{41,6,false}}};}
}
namespace mt2mp::game::tutorial_development {
bool read_startingpoint(StartingpointSnapshot& out,json&,std::string& e,bool,bool){++fixture::development_reads;if(!fixture::read_ok){e="original snapshot read failed";return false;}out=fixture::snapshot;return true;}
}
namespace mt2mp::game::tutorial_development {
bool read_research(const std::string& key,StartingpointSnapshot& out,json& diagnostic,std::string& e,bool creation,bool share){
    if(!fixture::snapshot.creation||!fixture::snapshot.creation->current||key!=fixture::snapshot.creation->current->key){e="fixture selected Research key differs";return false;}
    return read_startingpoint(out,diagnostic,e,creation,share);
}
}
namespace mt2mp::game::native {
bool read_clock(ClockState& out,std::string&){++fixture::clock_reads;out={};out.time_scale=0;return true;}
}
namespace mt2mp::game::tutorial_replication {
constexpr uintptr_t image=fixture::module;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
template<class T>bool at(uintptr_t p,size_t off,T& value,std::string& e){auto* bytes=reinterpret_cast<unsigned char*>(&value);for(size_t i=0;i<sizeof(value);++i){auto it=fixture::bytes.find(p+off+i);if(it==fixture::bytes.end())return fail(e,"original unmapped memory read");bytes[i]=it->second;}return true;}
bool str(uintptr_t p,size_t off,std::string& value,std::string& e){if(fixture::throw_string)throw std::runtime_error("original visible-page string read threw");auto it=fixture::strings.find(p+off);if(it==fixture::strings.end())return fail(e,"original missing native string");value=it->second;return true;}
bool type(uintptr_t p,uintptr_t rva,std::string& e){uintptr_t ap=0;return at(p,0,ap,e)&&(ap==image+rva||fail(e,"original address point differs"));}
bool origin(const action_session::Context&,const action_session::Binding&,World& world,std::string& e){++fixture::origins;if(!fixture::origin_ok)return fail(e,"original current Origin read failed");world=fixture::current;return true;}
#include "same_world.inc"
#include "staff_display_owner.inc"
namespace display_native {
struct Borrowed {const char* data;uint64_t length;unsigned char reserved[16]{};};
struct Value {
    struct {unsigned char bytes[0x38]{};} value;
    bool number(int32_t n,std::string&){fixture::labels[reinterpret_cast<uintptr_t>(value.bytes)]={{"key",std::to_string(n)},{"args",json::array()}};return true;}
    bool literal(const char* key,std::string&){fixture::labels[reinterpret_cast<uintptr_t>(value.bytes)]={{"key",key},{"args",json::array()}};return true;}
    bool close(std::string&){return true;}
};
bool loc(uintptr_t p,json& out,std::string& e,unsigned=0){auto it=fixture::labels.find(p);if(it==fixture::labels.end())return fail(e,"original missing native LocString");out=it->second;return true;}
#include "code_result.inc"
unsigned native_complete(uintptr_t,bool& out){out=fixture::complete;return 0;}
unsigned native_summary(uintptr_t,const void* value){++fixture::summary_calls;if(fixture::setter_fail=="summary")return 1;fixture::labels[fixture::fields+0x50]=fixture::labels.at(reinterpret_cast<uintptr_t>(value));return 0;}
unsigned native_project(uintptr_t pane,const void* value){++fixture::project_calls;if(fixture::setter_fail=="project")return 1;fixture::labels[pane+0x2c8]=fixture::labels.at(reinterpret_cast<uintptr_t>(value));return 0;}
unsigned native_developer(uintptr_t,int row,const void* value,const void*){
    ++fixture::developer_calls;fixture::setter_rows.push_back(row);if(fixture::setter_fail=="developer")return 1;
    uintptr_t field=0;std::string e;if(!at(fixture::grid_fields,size_t(row*3+1)*8,field,e))return 2;
    fixture::labels[field+0x10]=fixture::labels.at(reinterpret_cast<uintptr_t>(value));
    if(fixture::mismatch)fixture::labels[field+0x10]["key"]="wrong";return 0;
}
bool activity_value(const staff_display::Developer& d,uintptr_t,Value& out,std::string& e){return out.literal(staff_display::activity_key(d),e);}
}
#include "staff_display_ui.inc"
uint32_t mainthread=GetCurrentThreadId();
void original_fake(void*,float){++fixture::original_calls;}
void(*original_display)(void*,float)=&original_fake;
#include "posthook.inc"
}
namespace {
namespace dn=tr::display_native;
action_session::Context test_context{3,2,1,3,action_session::Role::Client,"11111111-1111-4111-8111-111111111111"};
action_session::Binding binding{4,7};
void reset(){fixture::reset();dn::cache.reset();dn::receipt={};dn::native_fault.clear();tr::mainthread=GetCurrentThreadId();}
void prepared_staff(){using fixture::need;using fixture::pass;std::string e;
    reset();fixture::snapshot.creation->current->key="bridges";const auto observed=fixture::snapshot;const auto target=fixture::target();
    fixture::read_ok=false;
    need(dn::admit(test_context,binding,target,e,true,"bridges",false,&observed)&&dn::cache&&fixture::development_reads==0&&fixture::origins==2,"immediate readback still checks actual Origin and Staff without repeated Development");
    pass("successful project readback supplies only the adjacent Development value");
    reset();fixture::snapshot.creation->current->key="bridges";auto stale=fixture::snapshot;++stale.origin.generation;
    need(!dn::admit(test_context,binding,target,e,true,"bridges",false,&stale)&&!dn::cache&&e=="Staff display full Origin differs","stale supplied Origin installed");
    reset();fixture::snapshot.creation->current->key="bridges";const auto before=fixture::snapshot;
    fixture::put<uint8_t>(before.creation->developers[0].address+0x4d8,0);
    need(!dn::admit(test_context,binding,target,e,true,"bridges",false,&before)&&!dn::cache&&e.find("prefix")!=std::string::npos,"fresh Staff owner change hidden by Development reuse");
    reset();need(!dn::admit(test_context,binding,target,e,false,"bridges",false,&fixture::snapshot)&&!dn::cache,"prepared snapshot escaped immediate install route");
    need(!dn::admit(test_context,binding,target,e,true,"startingpoint",false,&fixture::snapshot)&&!dn::cache,"prepared snapshot escaped selected Research");
    pass("prepared admission retains fresh world, live Staff and route guards");
}
void owners(){using fixture::need;using fixture::pass;std::string e;dn::Owners actual;
    reset();need(dn::read_owners(fixture::snapshot,actual,e)&&actual.developers[0].id==41&&actual.developers[0].row==0&&actual.developers[1].id==1,"actual array row confused with stable ID");pass("native ownership retains local array row separately from stable Developer ID");
    fixture::put<int32_t>(fixture::snapshot.creation->developers[49].address+0x4c0,41);need(!dn::read_owners(fixture::snapshot,actual,e)&&e.find("identity")!=std::string::npos,"duplicate native Developer ID admitted");pass("duplicate source identity rejects before cache installation");
    reset();fixture::put<uint8_t>(fixture::snapshot.creation->developers[0].address+0x4d8,0);need(!dn::read_owners(fixture::snapshot,actual,e),"hired native prefix mismatch admitted");pass("actual wanted prefix must match the original hired count");
    reset();need(dn::read_owners(fixture::snapshot,actual,e),e);auto bad=fixture::target();bad.developers[0].id=2;need(!dn::bind_target(bad,actual,e)&&e.find("IDs differ")!=std::string::npos,"foreign Host ID admitted");pass("Host IDs must match actual employed Client members");
    reset();auto target=fixture::target();need(dn::admit(test_context,binding,target,e,true)&&dn::cache.has_value(),e);
    const auto jobs=fixture::bytes;std::optional<tr::staff_display::State> captured;need(dn::capture(test_context,binding,fixture::snapshot,captured,e)&&captured==target&&fixture::bytes==jobs,"Client receipt rewrote simulation jobs");pass("Client capture returns adopted Host display while actual local jobs stay immutable");
    auto different=fixture::snapshot;different.project+=8;need(!dn::cache_bound(*dn::cache,test_context,binding,different),"new project inherited old cache");different=fixture::snapshot;++different.origin.world_epoch;need(!dn::cache_bound(*dn::cache,test_context,binding,different),"new world inherited old cache");auto changed=test_context;++changed.session;need(!dn::cache_bound(*dn::cache,changed,binding,fixture::snapshot),"new session inherited old cache");pass("project, native world and session changes invalidate exact cache binding");
    need(dn::admit(test_context,binding,std::nullopt,e,false)&&dn::cache.has_value(),"preflight retired live cache");need(dn::admit(test_context,binding,std::nullopt,e,true)&&!dn::cache&&dn::status()["adopted"]==false,"confirmed retirement retained old display");pass("only installation of absent display clears cache");
    reset();fixture::read_ok=false;need(!dn::admit(test_context,binding,target,e,true)&&e=="original snapshot read failed"&&!dn::cache,"snapshot failure was replaced or cached");pass("failed actual ownership read retains its cause and installs no cache");
}
void ui(){using fixture::need;using fixture::pass;std::string e;const auto target=fixture::target();
    reset();need(dn::admit(test_context,binding,target,e,true),e);dn::UI pinned;need(dn::ui_pin(*dn::cache,fixture::window,pinned,e)&&pinned.cells.size()==2&&pinned.cells[0].id==41&&pinned.cells[0].row==0,"actual Staff cell mapping rejected");pass("real UI owner binding resolves visible cells through native rows");
    const auto actual_jobs=fixture::bytes;dn::Calls calls(*dn::cache,fixture::window);bool attempted=false;
    need(tr::staff_display::render(target,calls,attempted,e),e);need(fixture::setter_rows==std::vector<int>{1,0},"sorted Host ID was used as grid row");need(fixture::bytes==actual_jobs&&dn::receipt.visible["developers"][0]["id"]==41&&fixture::summary_calls==1&&fixture::project_calls==1&&fixture::developer_calls==2,"native setters repeated or actual Client job state changed");pass("actual Calls maps sorted Host IDs to local rows and confirms exact setters/readback");
    reset();need(dn::admit(test_context,binding,target,e,true),e);fixture::put<uint8_t>(fixture::window+0x45f,0);dn::Calls hidden(*dn::cache,fixture::window);need(!tr::staff_display::render(target,hidden,attempted,e)&&!attempted&&fixture::summary_calls==0,"hidden UI mutated before layout readiness");pass("hidden actual Window defers before any native setter");
    reset();need(dn::admit(test_context,binding,target,e,true),e);fixture::put(fixture::summary+0x280,uintptr_t(0xdead));need(!dn::ui_pin(*dn::cache,fixture::window,pinned,e)&&e.find("another Window")!=std::string::npos,"foreign summary owner accepted");pass("foreign UI child ownership rejects before native mutation");
    reset();need(dn::admit(test_context,binding,target,e,true),e);fixture::put(fixture::grid_fields+32,fixture::cell0);need(!dn::ui_pin(*dn::cache,fixture::window,pinned,e)&&e.find("duplicated")!=std::string::npos,"duplicate TextField ownership admitted");pass("duplicated activity cell ownership cannot falsely render");
    reset();need(dn::admit(test_context,binding,target,e,true),e);fixture::setter_fail="project";dn::Calls partial(*dn::cache,fixture::window);need(!tr::staff_display::render(target,partial,attempted,e)&&attempted&&dn::receipt.unknown&&!dn::receipt.rendered&&fixture::summary_calls==1&&fixture::project_calls==1&&!fixture::developer_calls,"partial native setter became rendered success");pass("partial original setter failure is visible Unknown and stops remaining setters");
    reset();need(dn::admit(test_context,binding,target,e,true),e);fixture::mismatch=true;dn::Calls mismatch(*dn::cache,fixture::window);need(!tr::staff_display::render(target,mismatch,attempted,e)&&attempted&&e.find("readback differs")!=std::string::npos&&dn::receipt.visible.is_null(),"native cell mismatch became rendered success");pass("actual native LocString readback must match all expected visible cells");
}
void research_ui(){using fixture::need;using fixture::pass;std::string e;const auto target=fixture::target();
    reset();fixture::snapshot.creation->current->key="bridges";fixture::strings[fixture::project+0x20]="bridges";
    need(dn::admit(test_context,binding,target,e,true,"bridges"),e);dn::Calls calls(*dn::cache,fixture::window);bool attempted=false;
    need(tr::staff_display::render(target,calls,attempted,e)&&attempted&&fixture::summary_calls==1&&fixture::project_calls==1&&fixture::developer_calls==2,e);
    pass("Bridges display uses selected Research current key and exact existing setters");
    fixture::strings[fixture::project+0x20]="startingpoint";dn::UI pinned;
    need(!dn::ui_pin(*dn::cache,fixture::window,pinned,e)&&e.find("kind/key differs")!=std::string::npos,"mismatched Research UI key admitted");
    pass("Research current source and visible project key must match");
}
void hook(){using fixture::need;using fixture::pass;std::string e;const auto target=fixture::target();
    reset();tr::display_update(reinterpret_cast<void*>(fixture::window),0.1f);need(fixture::original_calls==1&&dn::receipt.original_calls==1&&!dn::receipt.post_calls&&!fixture::clock_reads,"missing-cache hook skipped or repeated original");pass("actual UpdateUI hook preserves original once without an adopted display");
    reset();need(dn::admit(test_context,binding,target,e,true),e);const auto jobs=fixture::bytes;tr::display_update(reinterpret_cast<void*>(fixture::window),0.1f);
    need(fixture::original_calls==1&&dn::receipt.original_calls==1&&dn::receipt.confirmed_renders==1&&dn::receipt.rendered&&!dn::receipt.unknown&&fixture::bytes==jobs,"hook failed original-once rendered readback");pass("actual posthook renders once only after original and exact native setter readback");
    reset();need(dn::admit(test_context,binding,target,e,true),e);fixture::put<uint8_t>(fixture::window+0x45f,0);tr::display_update(reinterpret_cast<void*>(fixture::window),0.1f);
    need(fixture::original_calls==1&&dn::receipt.deferred&&!dn::receipt.rendered&&!dn::receipt.unknown&&!fixture::clock_reads&&!fixture::summary_calls,"hidden window was rendered or scanned through Clock");pass("hidden-window quick gate preserves original and defers without Clock/cell scan");
    fixture::put<uint8_t>(fixture::window+0x45f,1);tr::display_update(reinterpret_cast<void*>(fixture::window),0.1f);need(fixture::original_calls==2&&dn::receipt.rendered&&!dn::receipt.deferred&&dn::receipt.confirmed_renders==1,"visible window did not recover with one new original");pass("deferred receipt renders after the same actual Window becomes ready");
    reset();need(dn::admit(test_context,binding,target,e,true),e);++tr::mainthread;tr::display_update(reinterpret_cast<void*>(fixture::window),0.1f);need(fixture::original_calls==1&&dn::receipt.deferred&&!fixture::summary_calls,"worker callback mutated cached UI");pass("wrong-thread post observation never swallows the native original");
    reset();need(dn::admit(test_context,binding,target,e,true),e);fixture::setter_fail="project";tr::display_update(reinterpret_cast<void*>(fixture::window),0.1f);const auto count=fixture::summary_calls+fixture::project_calls+fixture::developer_calls;
    need(dn::receipt.unknown&&!dn::receipt.rendered&&dn::receipt.cause.find("project SetText")!=std::string::npos,"partial hook failure lost Unknown/cause");
    tr::display_update(reinterpret_cast<void*>(fixture::window),0.1f);need(fixture::original_calls==2&&fixture::summary_calls+fixture::project_calls+fixture::developer_calls==count&&!dn::receipt.confirmed_renders,"Unknown repeated a UI setter or skipped native original");pass("Unknown presentation blocks setter replay while original UpdateUI continues once");
    reset();need(dn::admit(test_context,binding,target,e,true),e);fixture::throw_string=true;bool escaped=false;try{tr::display_update(reinterpret_cast<void*>(fixture::window),0.1f);}catch(const std::exception&){escaped=true;}
    need(!escaped&&fixture::original_calls==1&&dn::receipt.deferred&&!dn::receipt.rendered&&dn::receipt.cause.find("original visible-page string read threw")!=std::string::npos,"post-observation exception escaped into the native caller");pass("original preflight read exception stays visible inside the posthook boundary");
}
}
int main(){try{prepared_staff();owners();ui();research_ui();hook();std::cout<<fixture::cases<<" actual Staff owner/cache/UI body checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
