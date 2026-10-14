#include "release_entry.h"
#include "release_bytes.h"
#include <cstring>
#include <exception>
#include <limits>
#include <vector>

namespace mt2mp::game::initial_release {
namespace {
bool fail(std::string& cause,const char* text){cause=text;return false;}
template<class T>bool field(Memory& m,uintptr_t p,size_t offset,T& out,std::string& cause){
    if(!p||offset>UINTPTR_MAX-p)return fail(cause,"release field address invalid");
    if(!m.copy(p+offset,&out,sizeof out,cause)){if(cause.empty())cause="release field read failed without cause";return false;}return true;
}
bool ap(Memory& m,uintptr_t object,uintptr_t expected,std::string& cause){uintptr_t value=0;return field(m,object,0,value,cause)&&(value==expected||fail(cause,"release actual object class differs"));}
bool sample(Memory& m,const Binding& b,State& out,std::string& cause){
    if(!b.image||b.image>UINTPTR_MAX-0x1787930||!b.window||!b.manager||!b.release||!b.software||!b.server||!b.map||
       !b.epoch||!b.generation||!b.thread||b.snapshot_uuid.empty())return fail(cause,"release actual world binding absent");
    uintptr_t manager=0,software=0,server=0,map=0,release=0;
    if(!field(m,b.image,0x13df4f0,manager,cause)||manager!=b.manager||!field(m,b.image,0x13df500,software,cause)||software!=b.software||
       !field(m,b.image,0x13df430,server,cause)||server!=b.server||!field(m,b.image,0x13df8c0,map,cause)||map!=b.map||
       !field(m,b.manager,0x38,release,cause)||release!=b.release){if(cause.empty())cause="release actual singleton/current release changed";return false;}
    if(!ap(m,b.window,b.image+0x171fe80,cause)||!ap(m,b.manager,b.image+0x1714ed0,cause)||!ap(m,b.release,b.image+0x16c12b0,cause)||
       !ap(m,b.software,b.image+0x1715450,cause)||!ap(m,b.server,b.image+0x1702a40,cause)||!ap(m,b.map,b.image+0x1742860,cause))return false;
    if(!field(m,b.software,0x88,out.version,cause)||!field(m,b.release,0x50,out.planned,cause)||
       !field(m,b.release,0x60,out.release_type,cause)||!field(m,b.release,0x158,out.changes,cause)||
       !field(m,b.manager,0x98,out.changelogs,cause)||!field(m,b.server,0x18,out.server_current,cause)||
       !field(m,b.server,0x1c,out.server_target,cause)||!field(m,b.map,0x3d4,out.map_state,cause))return false;
    if(out.changelogs<0||out.changelogs>4096||out.changes<0)return fail(cause,"release native collection count invalid");
    if(out.changelogs){uintptr_t rows=0,last=0;
        if(!field(m,b.manager,0x90,rows,cause)||!field(m,rows,size_t(out.changelogs-1)*8,last,cause)||
           !field(m,last,0x18,out.last_changelog_type,cause)||!field(m,last,0x38,out.last_changelog,cause))return false;
    }
    return true;
}
bool uuid(const std::string& v){if(v.size()!=36)return false;for(size_t i=0;i<v.size();++i){
    if(i==8||i==13||i==18||i==23){if(v[i]!='-')return false;}else if(!((v[i]>='0'&&v[i]<='9')||(v[i]>='a'&&v[i]<='f')))return false;}return true;}
}
bool read_state(Memory& m,const Binding& b,State& out,std::string& cause){
    out={};cause.clear();State first,second;if(!sample(m,b,first,cause)||!sample(m,b,second,cause))return false;
    if(first!=second)return fail(cause,"release native state changed during double read");out=second;return true;
}
bool initial_before(const State& s,std::string& cause){
    if(s.version.major>0)return fail(cause,"release initial version already published");
    if(s.release_type!=4||s.planned!=Version{1,0,0})return fail(cause,"release actual planned initial type/version differs");
    if(s.changes<=0)return fail(cause,"release original release_status_empty");
    if(s.server_current==3)return fail(cause,"release original release_status_notoffline");
    cause.clear();return true;
}
bool functional_starting_region(Memory& m,const Binding& b,std::string& cause){
    cause.clear();std::string details;size_t rejected=0,non_starting_regions=0;
    // Bound diagnostic text without changing which native candidates are examined.
    auto note=[&](const std::string& why){if(rejected<8)details+="; "+why;++rejected;};
    uintptr_t control=0,character=0,regions=0;int32_t count=0;
    if(!field(m,b.image,0x13df9f0,control,cause))return false;
    if(control&&(!field(m,control,0x148,character,cause)||character)){
        if(cause.empty())cause="release original release_status_controllingcharacter";return false;}
    if(!field(m,b.map,0x2c0,regions,cause)||!field(m,b.map,0x2c8,count,cause))return false;
    if(count<0||count>4096||(!regions&&count))return fail(cause,"release actual Region array bounds invalid");
    for(int32_t i=0;i<count;++i){uintptr_t region=0,manager=0,buildings=0;int32_t type=0,size=0;
        if(!field(m,regions,size_t(i)*8,region,cause)||!ap(m,region,b.image+0x1787930,cause)||!field(m,region,0x854,type,cause))return false;
        const auto region_label="region_index="+std::to_string(i);
        if(type!=1){++non_starting_regions;continue;}
        if(!field(m,region,0x950,manager,cause)||!ap(m,manager,b.image+0x17183c0,cause)||!field(m,manager,0x410,buildings,cause)||!field(m,manager,0x418,size,cause))return false;
        if(size<0||size>16384||(!buildings&&size))return fail(cause,"release actual Building array bounds invalid");
        if(!size)note(region_label+" building_count actual=0 required>0");
        for(int32_t j=0;j<size;++j){uintptr_t building=0;int32_t state=0;uint8_t active=0,connected=0,other_route=0;
            if(!field(m,buildings,size_t(j)*8,building,cause)||!ap(m,building,b.image+0x16d93c0,cause)||!field(m,building,0x7a8,state,cause))return false;
            const auto label=region_label+" building_index="+std::to_string(j);
            if(state!=2){note(label+" state actual="+std::to_string(state)+" required=2");continue;}
            if(!field(m,building,0xa8e,active,cause))return false;if(!active){note(label+" active actual=0 required=nonzero");continue;}
            if(!field(m,building,0x7e2,connected,cause)||!field(m,building,0xcbb,other_route,cause))return false;if(!connected&&!other_route){note(label+" connected actual=0 other_route actual=0 required=either_nonzero");continue;}
            uintptr_t services=0;int32_t service_count=0;
            if(!field(m,building,0x9d8,services,cause)||!field(m,building,0x9e0,service_count,cause))return false;
            if(service_count<0||service_count>1024||(!services&&service_count))return fail(cause,"release actual Building service array bounds invalid");
            std::string service_types;
            for(int32_t k=0;k<service_count;++k){uintptr_t service=0;int32_t service_type=-1;
                if(!field(m,services,size_t(k)*8,service,cause)||!field(m,service,0x20,service_type,cause))return false;
                if(service_type==2){cause.clear();return true;}
                if(k<8)service_types+=(k?",":"")+std::to_string(service_type);}
            note(label+" service_count="+std::to_string(service_count)+" service_types=["+service_types+"] required=2");
        }
    }
    cause="release original release_status_nofunctionalstartingregion region_count="+std::to_string(count)+
        " non_starting_regions="+std::to_string(non_starting_regions)+" rejected_candidates="+std::to_string(rejected)+details;
    if(rejected>8)cause+="; omitted_candidates="+std::to_string(rejected-8);
    return false;
}
bool release_effect(const State& before,const State& after,std::string& cause){
    if(!initial_before(before,cause))return false;
    if(before.changelogs<0||before.changelogs==std::numeric_limits<int32_t>::max()||after.changelogs!=before.changelogs+1)
        return fail(cause,"release actual changelog append not confirmed");
    if(after.version!=before.planned||after.last_changelog!=before.planned||after.last_changelog_type!=4)
        return fail(cause,"release actual published version/changelog differs");
    if(after.server_target!=3)return fail(cause,"release native online request not observed");
    cause.clear();return true;
}
bool settled_open(const State& s,std::string& cause){
    if(s.version!=Version{1,0,0})return fail(cause,"release initial version not observed");
    if(s.server_current!=3||s.server_target!=3)return fail(cause,"release native server transition incomplete");
    if(s.map_state!=7)return fail(cause,"release native Map is not open for subscribers");cause.clear();return true;
}
bool resolve_unpatched(Memory& memory,uintptr_t image,std::string_view sha,Api& out,std::string& cause){
    out={};cause.clear();if(!image||image>UINTPTR_MAX-0x1742860||sha!=bytes::exe_sha256)return fail(cause,"release exact executable identity differs");
    for(const auto& g:bytes::guards){std::vector<uint8_t> actual(g.size);if(!memory.copy(image+g.rva,actual.data(),actual.size(),cause)){
        if(cause.empty())cause="release full code guard read failed without cause";return false;}
        if(std::memcmp(actual.data(),g.data,g.size)){cause="release full code guard differs rva="+std::to_string(g.rva);return false;}}
    out={reinterpret_cast<RecordCtor>(image+0x967370),reinterpret_cast<RecordDtor>(image+0x9676a0),
         reinterpret_cast<RecordEquals>(image+0x1113d80),reinterpret_cast<UiCommand>(image+0x79baa0)};return true;
}
bool execute(const Request& request,Root& root,Memory& memory,Calls& calls,Receipt& receipt,std::string& cause){
    receipt={};cause.clear();auto failed=[&](const char* stage){if(cause.empty())cause=std::string("release failed without cause stage=")+stage;
        receipt.cause=cause;root.failed(stage,cause);return false;};
    if(!uuid(request.snapshot_uuid)||!uuid(request.action_uuid)){cause="release requires canonical action and snapshot UUIDs";return failed("schema");}
    Binding b;Api api;if(!root.authorize(request,b,api,cause))return failed("controller_lifecycle_authority");
    if(b.snapshot_uuid!=request.snapshot_uuid||!api.construct||!api.destroy||!api.equals||!api.command){cause="release Root authority returned incomplete binding/API";return failed("binding");}
    State before;if(!read_state(memory,b,before,cause)||before!=request.before||!initial_before(before,cause)){
        if(cause.empty())cause="release actual before state differs from ordered request";return failed("native_before");}
    if(!root.current(b,cause))return failed("native_current_before_effect");
    if(!calls.command(api,b.window,receipt.native,cause))return failed("original_release_ui_command");
    const auto& n=receipt.native;
    if(!n.constructed||!n.label_confirmed||!n.attempted||!n.returned||!n.accepted||!n.cleanup_attempted||!n.cleanup_confirmed||n.seh_code||n.cleanup_exception){
        cause="release original command/record cleanup receipt incomplete";return failed("native_call_receipt");}
    if(!root.current(b,cause))return failed("native_current_after_effect");
    // DoRelease replaces Manager.current. Refresh only this native pointer for readback.
    Binding after=b;if(!field(memory,b.manager,0x38,after.release,cause)||!after.release||after.release==b.release){
        if(cause.empty())cause="release actual current release replacement absent";return failed("native_release_replacement");}
    if(!read_state(memory,after,receipt.after,cause)||!release_effect(before,receipt.after,cause))return failed("native_effect_readback");
    receipt.effect_observed=true;return true;
}
Dispatch capture(uintptr_t window,uint32_t current_thread,uint32_t game_thread,bool cooperative,CommandClass command,IntentRoot& root,std::string& cause,uint32_t command_fault)noexcept{
    cause.clear();if(!cooperative||!game_thread||current_thread!=game_thread||command==CommandClass::Other)return Dispatch::Forward;
    auto rejected=[&](){if(cause.empty())cause="release capture failed without cause";root.rejected(cause);return Dispatch::Suppress;};
    try {
        if(command==CommandClass::Unreadable){cause="release native command read failed SEH="+std::to_string(command_fault);return rejected();}
        Binding b;State before;if(!root.read_current(b,before,cause))return rejected();
        if(b.window!=window)return Dispatch::Forward;
        if(b.thread!=game_thread){cause="release native thread binding differs";return rejected();}
        if(!initial_before(before,cause))return rejected();
        if(!root.submit(b,before,cause))return rejected();
        return Dispatch::Suppress;
    }catch(const std::exception& e){cause=std::string("release capture callback exception: ")+e.what();return rejected();}
    catch(...){cause="release capture callback unknown exception";return rejected();}
}
}
