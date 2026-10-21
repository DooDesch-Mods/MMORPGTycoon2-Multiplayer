#include "service_live_reader.h"
#include "management_live_state.h"
#include "service_building_census.h"
#include "service_membership_fixture.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace mt2mp::game::service_live;
namespace {
int checks=0;void need(bool v,const std::string& e){++checks;if(!v)throw std::runtime_error(e);}
struct Memory {
    static constexpr uintptr_t base=0x200000,image=0x100000,object=base+0x100,daily=object+0x50,history=base+0x1000;
    std::array<unsigned char,0x2000> bytes{};
    bool copy(uintptr_t p,void* out,size_t n,std::string& e){if(p<base||p-base>bytes.size()||n>bytes.size()-(p-base)){e="fixture read extent";return false;}std::memcpy(out,bytes.data()+p-base,n);return true;}
    template<class T>void put(uintptr_t p,const T& v){need(p>=base&&p-base+sizeof v<=bytes.size(),"fixture write extent");std::memcpy(bytes.data()+p-base,&v,sizeof v);}
    void reset(const Count& c,int32_t cap=10){bytes.fill(0);put(daily,image+0x16e4710);put(daily+0x38,image+0x1749370);put(daily+0x40,history);std::array<int32_t,8> w{int32_t(c.history.size()),cap,c.today,c.total,c.day,10,c.highest,17};put(daily+0x48,w);put(daily+0x68,uint8_t(c.warning));for(size_t i=0;i<c.history.size();++i)put(history+i*4,c.history[i]);}
};
#include "building_uptime_cases.inc"
void codec(){
    std::string e;State before{{"action:a",std::string(64,'a'),{{{{Count{1,8,2,4,{3,4},false},Count{0,0,2,0,{},true}}}}}}};
    before[0].uptime={1,0,1};State actual;need(valid(before,e)&&decode(encode(before),actual,e)&&actual==before,e);
    mt2mp::game::management_simulation::LiveState frame,decoded;frame.services=before;
    mt2mp::game::dungeon_gizmo_live::Row gizmo;
    gizmo.key="action:44444444-4444-4444-8444-444444444444:dungeon:gizmo:0";gizmo.state=1;gizmo.immediate=1;gizmo.triggered=true;
    gizmo.clips[3]={uint32_t(0x40000000),uint32_t(0x3f000000),false,uint32_t(0x3f800000)};
    frame.gizmos=mt2mp::game::dungeon_gizmo_live::State{{"action:33333333-3333-4333-8333-333333333333/template",9,{gizmo}}};
    need(mt2mp::game::management_simulation::decode(mt2mp::game::management_simulation::encode(frame),decoded,e)&&frame==decoded,e);
    auto bad_frame=mt2mp::game::management_simulation::encode(frame);bad_frame.erase("services");need(!mt2mp::game::management_simulation::decode(bad_frame,decoded,e),"incomplete Service Frame admitted");
    auto target=before;++target[0].rows[0].counts[0].total;need(compatible(before,target,e),e);
    target[0].metadata[0]='b';need(!compatible(before,target,e),"static service drift admitted");target=before;target[0].key="action:b";need(!compatible(before,target,e),"canonical owner drift admitted");
    target=before;target[0].rows.push_back({});need(!compatible(before,target,e),"membership drift admitted");target=before;target.push_back(before[0]);need(!valid(target,e),"duplicate Building admitted");
    auto history_target=before;history_target[0].uptime={1,1};need(compatible(before,history_target,e),"authoritative uptime transition rejected");
    auto history_wire=encode(before);history_wire[0].erase("uptime");need(!decode(history_wire,actual,e),"missing uptime admitted");
    history_wire=encode(before);history_wire[0]["uptime"]={2};need(!decode(history_wire,actual,e),"non-Boolean uptime admitted");
    history_wire=encode(before);history_wire[0]["uptime"]=std::vector<int>(11);need(!decode(history_wire,actual,e),"oversized uptime admitted");
    auto wire=encode(before);wire[0]["rows"][0][0]["total"]=uint64_t{1}<<35;need(!decode(wire,actual,e),"int32 overflow admitted");wire=encode(before);wire[0]["rows"][0][0]["warning"]=1;need(!decode(wire,actual,e),"non-bool warning admitted");
    wire=encode(before);wire[0]["rows"][0][0]["history"]=std::vector<int>(11);need(!decode(wire,actual,e),"oversized history admitted");wire=encode(before);wire[0]["extra"]=1;need(!decode(wire,actual,e),"unexpected wire field admitted");
    mt2mp::action_session::ServiceMembershipState s;s.building_definition="Inn";s.spawn_type="Spawn";s.rows.push_back({});mt2mp::service_membership_reader::Local local;local.rows.push_back({});
    auto hash=[](const std::string& text){return text;};const auto static_before=metadata(s,hash);s.rows[0].counts[0].total=99;need(metadata(s,hash)==static_before,"runtime count leaked into static metadata");s.rows[0].price[0]=2;need(metadata(s,hash)!=static_before,"price missing from static metadata");
    s.rows[0].price[0]=0;s.configs.push_back({"Inn",{0,25}});need(metadata(s,hash)!=static_before,"shared Config missing from static metadata");
}
void native(){
    Memory m;Count before{1,8,2,4,{3},false},target{2,9,3,5,{3,4},true};std::string e;Receipt receipt;int stores=0,reserves=0,guards=0;bool fail_store=false,fail_guard=false;
    auto guard=[&](std::string& x){++guards;if(fail_guard){x="fixture owner changed";return false;}return true;};
    auto compare=[&](uintptr_t p,const void* a,const void* b,size_t n,std::string& x){++stores;std::vector<unsigned char> actual(n);if(!m.copy(p,actual.data(),n,x))return false;if(fail_store||std::memcmp(actual.data(),a,n)){x="fixture CAS differs";return false;}std::memcpy(m.bytes.data()+p-Memory::base,b,n);return true;};
    auto reserve=[&](uintptr_t p,int32_t capacity,std::string&){++reserves;need(p==Memory::daily+0x38&&capacity==10,"wrong original array Reserve");m.put(Memory::daily+0x4c,capacity);return true;};
    m.reset(before,1);need(apply_count(m,Memory::image,Memory::object,0x50,before,target,compare,reserve,guard,receipt,e),e);need(receipt.confirmed&&receipt.attempted&&!receipt.unknown&&reserves==1&&stores==3,"history/counter/warning original adoption receipt");
    mt2mp::npc_quest_daily_apply::NativeState after;need(mt2mp::npc_quest_daily_apply::read_native(m,Memory::image,Memory::daily,after,e),e);need(after.words[7]==18&&after.words[4]==3&&after.words[6]==5&&after.warning==1&&after.value.total==9,"actual complete counter values");
    const auto saved=m.bytes;stores=0;need(apply_count(m,Memory::image,Memory::object,0x50,target,target,compare,reserve,guard,receipt,e)&&!receipt.attempted&&!stores&&m.bytes==saved,"unchanged counter mutated");
    m.put(Memory::daily+0x60,int32_t{99});need(!apply_count(m,Memory::image,Memory::object,0x50,target,target,compare,reserve,guard,receipt,e)&&!receipt.attempted,"unchanged desired hid wrong highest cache");
    m.reset(before);fail_store=true;need(!apply_count(m,Memory::image,Memory::object,0x50,before,target,compare,reserve,guard,receipt,e)&&receipt.attempted&&receipt.unknown&&e=="fixture CAS differs","mutation failure lost Unknown/first cause");fail_store=false;
    m.reset(before);fail_guard=true;need(!apply_count(m,Memory::image,Memory::object,0x50,before,target,compare,reserve,guard,receipt,e)&&!receipt.attempted&&!receipt.unknown,"owner failure mutated");fail_guard=false;
    m.reset(before);auto day=before;++day.day;need(apply_count(m,Memory::image,Memory::object,0x50,before,day,compare,reserve,guard,receipt,e),e);need(receipt.confirmed&&receipt.attempted,"day-only transition skipped");
    m.reset(before);m.put(Memory::daily+0x68,uint8_t{1});need(!apply_count(m,Memory::image,Memory::object,0x50,before,target,compare,reserve,guard,receipt,e)&&!receipt.attempted,"wrong complete warning before admitted");
    need(guards>10,"real guard path unused");
}
void service_ui_cache(){
    namespace fixture=service_fixture;
    using mt2mp::action_session::ServiceMembershipState;
    using mt2mp::service_membership_reader::Local;
    constexpr uintptr_t config_manager=0x88000,config_data=0x89000,config=0x8a000;
    auto memory=fixture::memory();
    memory.put(fixture::image+0x13df840,config_manager);memory.put(config_manager,fixture::image+0x173fba0);
    memory.array(config_manager,0x18,config_data,1,4);memory.put(config_data,config);
    memory.put(config,fixture::image+0x1736660);memory.strings[config+0x18]="SmallInn_Spawn";
    memory.put(config+0x38,fixture::image+0x174ab50);memory.put(config+0x50,std::array<int32_t,2>{2,75});
    memory.put(fixture::service+0x48,std::array<uint8_t,3>{1,0,1});
    memory.put(fixture::service+0x40,std::array<int32_t,2>{1,25});
    const auto hash=[](const std::string& text){return text;};
    ServiceMembershipState host,client;Local host_local,client_local;std::string error;
    const auto untouched=memory.fields;
    need(fixture::read(memory,host,host_local,error),error);
    const auto host_frame=project("action:client-inn",host,host_local,hash);
    need(memory.fields==untouched,"Frame sampling mutated original Service or Config memory");
    // BuildingInfoWindow::_UpdateServices calls GetPrice. Its existing-Config path
    // stores the Config pointer and copies both price words, leaving membership intact.
    memory.put(fixture::service+0x150,config);
    memory.put(fixture::service+0x40,std::array<int32_t,2>{2,75});
    need(fixture::read(memory,client,client_local,error),error);
    const auto client_frame=project("action:client-inn",client,client_local,hash);
    need(host!=client&&host_local!=client_local,"fixture did not reproduce the UI-only native cache transition");
    need(metadata_fields(host,false)!=metadata_fields(client,false),"raw metadata did not reproduce the former rejection");
    need(host_frame==client_frame,"existing Config UI cache changed authoritative Frame state");
    std::map<std::string,std::string> digest_labels;
    const auto digest_label=[&](const std::string& text){auto found=digest_labels.find(text);if(found==digest_labels.end())found=digest_labels.emplace(text,std::string(64,char('a'+digest_labels.size()))).first;return found->second;};
    mt2mp::game::management_simulation::LiveState host_authority,client_before;
    host_authority.services.push_back(project("action:client-inn",host,host_local,digest_label));
    client_before.services.push_back(project("action:client-inn",client,client_local,digest_label));
    std::vector<mt2mp::game::management_simulation::Change> changes;
    need(mt2mp::game::management_simulation::plan(client_before,host_authority,changes,error)&&changes.empty(),"Client before-apply rejected Host authority after UI-only cache resolution: "+error);
    auto former_client=client_before;former_client.services[0].metadata=digest_label(metadata_fields(client,false).dump());
    auto former_host=host_authority;former_host.services[0].metadata=digest_label(metadata_fields(host,false).dump());
    need(!mt2mp::game::management_simulation::plan(former_client,former_host,changes,error)&&error.find("Service Frame membership or non-counter service state differs")==0,"actual before-apply plan did not reproduce original raw metadata rejection");
    need(host.rows[0].price==std::array<int32_t,2>{1,25}&&!host.rows[0].config,"projection rewrote the sampled raw state");
    need(effective_row(host,host.rows[0]).price==std::array<int32_t,2>{2,75},"effective synced custom Config price lost");
    auto changed=client;changed.rows[0].price={9,999};
    need(metadata(changed,hash)==host_frame.metadata,"synced raw cache replaced existing authoritative Config price");
    changed=client;changed.configs[0].price[1]++;
    need(metadata(changed,hash)!=host_frame.metadata,"real Config price change admitted");
    changed=client;changed.rows[1].price={3,123};
    need(metadata(changed,hash)!=host_frame.metadata,"unsynced custom service price ignored");
    client_before.services[0]=project("action:client-inn",changed,client_local,digest_label);
    need(!mt2mp::game::management_simulation::plan(client_before,host_authority,changes,error),"before-apply plan admitted a true custom price divergence");
    for(size_t flag=0;flag<3;++flag){changed=client;changed.rows[0].flags[flag]=!changed.rows[0].flags[flag];need(metadata(changed,hash)!=host_frame.metadata,"real Service flag change admitted");}
    changed=client;changed.rows.erase(changed.rows.begin());need(metadata(changed,hash)!=host_frame.metadata,"Service membership deletion admitted");
    changed=client;changed.rows[0].config_key="other";need(metadata(changed,hash)!=host_frame.metadata,"foreign Config key admitted");
    changed=host;changed.configs.clear();const auto missing=changed;
    changed.rows[0].config=true;changed.rows[0].price={2,75};
    need(metadata(changed,hash)!=metadata(missing,hash),"missing Config was synthesized by projection");
    changed=host;changed.config_present=false;need(effective_row(changed,changed.rows[0])==changed.rows[0],"absent Config manager was synthesized");
    changed=host;changed.rows[0].config_key.clear();need(effective_row(changed,changed.rows[0])==changed.rows[0],"empty Config key was normalized");
    changed=host;changed.configs.push_back({"SmallInn_Spawn",{8,800}});
    need(effective_row(changed,changed.rows[0])==changed.rows[0],"ambiguous Config selection was invented");
    changed=host;changed.configs.push_back({"OtherService",{4,321}});
    need(metadata(changed,hash)!=host_frame.metadata,"complete global Config coverage was dropped");
    changed=host;changed.characters[0].deleted=true;need(metadata(changed,hash)!=host_frame.metadata,"Character guard was dropped");
    changed=host;changed.rows[0].counts[0].total++;
    const auto counter_frame=project("action:client-inn",changed,host_local,hash);
    need(counter_frame.metadata==host_frame.metadata&&counter_frame.rows!=host_frame.rows,"counter state was lost or moved into metadata");
    memory.put(fixture::service+0x150,uintptr_t{0xdead});
    need(!fixture::read(memory,client,client_local,error)&&error=="Service membership Config membership/key differs","foreign cached Config pointer passed native ownership validation");
    memory.put(fixture::service+0x150,config);memory.strings[config+0x18]="OtherService";
    need(!fixture::read(memory,client,client_local,error)&&error=="Service membership Config membership/key differs","cached Config key mismatch passed native validation");
    const auto diagnostic=sample_diagnostic(host,[](const std::string&){return std::string(64,'a');});
    need(diagnostic.at("rows")[0].at("price")==nlohmann::json::array({1,25})&&diagnostic.at("rows")[0].at("effective_price")==nlohmann::json::array({2,75}),"diagnostic lost raw/effective price");
    need(!diagnostic.at("rows")[0].at("cached_config").get<bool>()&&diagnostic.at("rows")[0].at("effective_config").get<bool>(),"diagnostic lost measured cache distinction");
    need(diagnostic.dump().find("SmallInn")==std::string::npos&&diagnostic.dump().find("Warrior")==std::string::npos,"diagnostic exposed native names or Config key payload");
    changed=host;changed.rows.resize(100);
    const auto bounded=sample_diagnostic(changed,[](const std::string&){return std::string(64,'a');});
    need(bounded.at("row_count")==100&&bounded.at("rows").size()==8,"diagnostic rows are not bounded");
    State actual{{"action:client-inn",std::string(64,'a'),std::vector<Row>(2)}},target=actual;
    target[0].metadata=std::string(64,'b');const auto difference=first_difference(actual,target);
    need(difference.at("field")=="metadata"&&difference.at("actual_metadata")==actual[0].metadata&&difference.at("expected_metadata")==target[0].metadata,"first difference lost actual/expected hashes");
    need(!compatible(actual,target,error)&&error.find("action:client-inn")!=std::string::npos&&error.find("actual_metadata")!=std::string::npos,"failure lacks bounded measured Building diagnostic");
    target=actual;target[0].rows.push_back({});need(first_difference(actual,target).at("field")=="row_count","row count cause collapsed into metadata");
    target=actual;target[0].key="action:foreign";need(first_difference(actual,target).at("field")=="key","canonical owner cause collapsed into metadata");
    target.clear();need(first_difference(actual,target).at("field")=="building_count","complete coverage cause missing");
    need(first_difference(actual,actual).is_null()&&compatible(actual,actual,error)&&error.empty(),"success retained stale error");
}
void service_ui_satisfaction(){
    namespace fixture=service_fixture;
    auto memory=fixture::memory();memory.array(fixture::building,0x9d0,fixture::data,2,4);
    memory.put(fixture::service+0x20,int32_t{0});memory.put(fixture::service+0x1000+0x20,int32_t{1});
    mt2mp::action_session::ServiceMembershipState host,client;
    mt2mp::service_membership_reader::Local host_local,client_local;
    std::string error;need(fixture::read(memory,host,host_local,error),error);
    need(host.rows.size()==2&&host.rows[0].index==-1&&host.rows[1].index==-1,"original satisfaction constructor sentinel fixture differs");
    std::map<std::string,std::string> digest_labels;
    const auto label=[&](const std::string& text){auto found=digest_labels.find(text);if(found==digest_labels.end())found=digest_labels.emplace(text,std::string(64,char('a'+digest_labels.size()))).first;return found->second;};
    const auto host_frame=project("saved:building:42",host,host_local,label);
    const auto before_memory=memory.fields;
    // Original CalculateSatisfactionPercent writes 100 when the raw price amount is 0.
    memory.put(fixture::service+0x140,int32_t{100});memory.put(fixture::service+0x1000+0x140,int32_t{100});
    need(memory.fields!=before_memory,"UI satisfaction fixture did not change native memory");
    need(fixture::read(memory,client,client_local,error),error);
    need(client==host&&client_local==host_local,"local satisfaction UI state changed shared authority or owner identity");
    const auto client_frame=project("saved:building:42",client,client_local,label);
    need(client_frame==host_frame,"UI satisfaction cache changed Service Frame membership or counters");
    mt2mp::game::management_simulation::LiveState host_authority,client_before;
    host_authority.services={host_frame};client_before.services={client_frame};
    std::vector<mt2mp::game::management_simulation::Change> changes;
    need(mt2mp::game::management_simulation::plan(client_before,host_authority,changes,error)&&changes.empty(),"actual before-apply plan rejects original UI satisfaction update: "+error);
    auto old_projection=client;for(auto& row:old_projection.rows)row.index=100;
    client_before.services={project("saved:building:42",old_projection,client_local,label)};
    need(!mt2mp::game::management_simulation::plan(client_before,host_authority,changes,error),"former raw satisfaction projection did not reproduce the failed before-apply comparison");
    const auto reject_change=[&](auto change,const std::string& cause){auto mutated=memory;change(mutated);mt2mp::action_session::ServiceMembershipState sampled;mt2mp::service_membership_reader::Local pins;
        need(fixture::read(mutated,sampled,pins,error),error);client_before.services={project("saved:building:42",sampled,pins,label)};
        need(!mt2mp::game::management_simulation::plan(client_before,host_authority,changes,error),cause);
    };
    reject_change([](auto& value){value.put(fixture::data,fixture::service+0x1000);value.put(fixture::data+8,fixture::service);},"true native Service row order was ignored");
    reject_change([](auto& value){value.put(fixture::service+0x20,int32_t{3});},"true Service type change was ignored");
    reject_change([](auto& value){value.put(fixture::service+0x40,std::array<int32_t,2>{0,25});},"true unsynced Service price change was ignored");
    int32_t actual_cache=-1;need(memory(fixture::service,0x140,actual_cache,error)&&actual_cache==100,"sampling changed the original local UI cache");
}
void census(){
    using mt2mp::game::remove_diagnostics::LocalTarget;using nlohmann::json;
    const std::string native="native-origin-distinct-from-session";
    json data={{"world_epoch",3},{"world_generation",9},{"bindings",json::array({{{"snapshot_id",native},{"record_key","saved:key"},{"local_uid",uint64_t{71}},{"revoked",false},{"unknown",false}}})}};
    int calls=0;LocalTarget target;target.object=123;target.uid=71;target.world_epoch=3;target.world_generation=9;target.snapshot_id=native;target.record_key="saved:key";
    auto resolve=[&](const auto&,LocalTarget& out,std::string&){++calls;out=target;return true;};
    std::vector<LocalTarget> values;std::string e;
    need(mt2mp::game::service_building_census::read(data,resolve,values,e)&&values.size()==1&&calls==1,"single complete current NativeOrigin census");
    target.uid=72;need(!mt2mp::game::service_building_census::read(data,resolve,values,e),"wrong full UID admitted");target.uid=71;
    target.world_generation=10;need(!mt2mp::game::service_building_census::read(data,resolve,values,e),"wrong current generation admitted");target.world_generation=9;
    target.record_key="other";need(!mt2mp::game::service_building_census::read(data,resolve,values,e),"wrong canonical key admitted");target.record_key="saved:key";
    data["bindings"].push_back(data["bindings"][0]);need(!mt2mp::game::service_building_census::read(data,resolve,values,e),"duplicate owner admitted");data["bindings"].erase(1);
    data["bindings"][0]["unknown"]=true;need(!mt2mp::game::service_building_census::read(data,resolve,values,e),"Unknown census admitted");data["bindings"][0]["unknown"]=false;
    data["bindings"][0]["revoked"]=true;calls=0;need(mt2mp::game::service_building_census::read(data,resolve,values,e)&&values.empty()&&!calls,"retired owner resolved");
}
}
int main(){try{uptime_cases();codec();native();census();service_ui_cache();service_ui_satisfaction();std::cout<<checks<<" service codec/native checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
