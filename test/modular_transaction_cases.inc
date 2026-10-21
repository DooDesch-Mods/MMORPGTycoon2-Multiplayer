#include "modular_transaction.h"
#include "modular_transition.h"
#include <limits>
#include <atomic>
#include <optional>
namespace {
struct ModularTestPort {
    std::vector<std::string> order;std::string fail_at;bool fail_cleanup=false,throw_edit=false,cancel_enabled=false,cancel_called=false;
    bool step(const char* name,std::string& e){order.emplace_back(name);if(fail_at==name){e=std::string("original ")+name+" cause";return false;}return true;}
    bool ready(const ModularIntent&,std::string& e){return step("ready",e);}bool purchase(const ModularIntent&,std::string& e){return step("purchase",e);}
    bool begin_edit(bool& opened,std::string& e){opened=true;return step("begin",e);}bool end_edit(std::string& e){if(fail_cleanup){order.emplace_back("end");e="EndEdit cleanup cause";return false;}return step("end",e);}
    bool make_graph(uintptr_t& graph,std::string& e){graph=10;return step("graph",e);}bool layout_inverse(uintptr_t,const ModularIntent&,std::string& e){return step("inverse",e);}
    bool can_edit(const ModularIntent&,std::string& e){return step("can",e);}bool edit(const ModularIntent&,std::string& e){if(throw_edit)throw std::runtime_error("edit exception cause");return step("edit",e);}
    bool release_point(const ModularIntent&,std::string& e){return step("release",e);}bool make_prefab(const ModularIntent&,uintptr_t& object,std::string& e){object=20;return step("prefab",e);}
    bool clear_scenery(const ModularIntent&,uintptr_t,std::string& e){return step("clearance",e);}bool cash_inverse(uintptr_t,uintptr_t,std::string& e){return step("cash",e);}bool destroy_inverse(uintptr_t,uintptr_t,std::string& e){return step("destroy",e);}
    bool commit(uintptr_t,uintptr_t,const ModularIntent&,std::string& e){return step("commit",e);}bool readback(uintptr_t,uintptr_t,const ModularIntent&,std::string& e){return step("readback",e);}
    bool cancel_no_effect(mt2mp::game::modular_transaction::Receipt&,std::string&){cancel_called=true;return cancel_enabled;}
    bool discard_rejected_graph(mt2mp::game::modular_transaction::Receipt& value,std::string&){if(cancel_enabled&&value.graph){order.emplace_back("discard");value.graph=0;}return true;}
};
unsigned modular_record_diagnostic_cases(){namespace transition=mt2mp::game::modular_transition;using j=nlohmann::json;unsigned count=0;
    const j source=j::array({
        j::array({int64_t(-1),size_t(0),j::array({1,uint32_t(0),"mmoBuildingLayout"}),j::array()}),
        j::array({int64_t(0),size_t(0),j::array({1,uint32_t(0),"grid"}),j::array()}),
        j::array({int64_t(1),size_t(0),j::array({0,uint32_t(0),""}),j::array({j::array({4,uint32_t(17),""})})}),
        j::array({int64_t(1),size_t(1),j::array({0,uint32_t(0),""}),j::array({j::array({4,uint32_t(33),""})})}),
        j::array({int64_t(0),size_t(1),j::array({1,uint32_t(0),"origin"}),j::array({j::array({3,uint32_t(0),""}),j::array({3,uint32_t(0),""}),j::array({3,uint32_t(0),""})})})
    });
    need(transition::record_readback_difference(source,source,false).empty(),"Equal complete Modular records produced a mismatch");++count;
    auto actual=source;actual[2][3][0][1]=uint32_t(19);auto detail=transition::record_readback_difference(source,actual,false);
    need(detail.find("detached Layout Record readback differs expected_records=5 actual_records=5")!=std::string::npos&&detail.find("path=/2/3/0/1 expected=unsigned(17) actual=unsigned(19)")!=std::string::npos,"Modular Layout diagnostic lost exact record counts, path or bits");++count;
    need(detail.find("expected_owner=string(bytes=4,prefix=\"grid\") actual_owner=string(bytes=4,prefix=\"grid\")")!=std::string::npos,"Modular anonymous cell diagnostic lost its owning property");++count;
    need(source[2][3][0][1]==uint32_t(17)&&actual[2][3][0][1]==uint32_t(19),"Modular diagnostic changed expected or actual records");++count;
    detail=transition::record_readback_difference(source,actual,true);need(detail.find("detached Plan Record readback differs")!=std::string::npos,"Modular Plan mismatch reported as Layout");++count;
    actual=source;actual.erase(actual.end()-1);detail=transition::record_readback_difference(source,actual,false);
    need(detail.find("expected_records=5 actual_records=4")!=std::string::npos&&detail.find("path=/ expected=array(count=5) actual=array(count=4)")!=std::string::npos,"Modular missing record diagnostic lost the count difference");++count;
    actual=source;actual[4][3].push_back(j::array({3,uint32_t(0),""}));detail=transition::record_readback_difference(source,actual,false);
    need(detail.find("path=/4/3 expected=array(count=3) actual=array(count=4)")!=std::string::npos&&detail.find("prefix=\"origin\"")!=std::string::npos,"Modular token count diagnostic lost its property path");++count;
    actual=source;actual[2][3][0][1]="19";detail=transition::record_readback_difference(source,actual,false);
    need(detail.find("expected=unsigned(17) actual=string(bytes=2,prefix=\"19\")")!=std::string::npos,"Modular scalar type difference was hidden");++count;
    actual=source;actual[2][3][0][1]=uint32_t(19);actual[3][3][0][1]=uint32_t(35);for(auto& token:actual[4][3])token[1]=uint32_t(0x3f800000);detail=transition::record_readback_difference(source,actual,false);
    size_t paths=0,offset=0;while((offset=detail.find("; path=",offset))!=std::string::npos){++paths;offset+=7;}
    need(paths==4&&detail.find("path=/4/3/2/1")==std::string::npos,"Modular diagnostic exceeded four differences or omitted the first differences");++count;
    actual=source;actual[1][2][2]=std::string(15,'a')+"\xc3\xa4"+std::string(10000,'b')+"unprinted-tail";detail=transition::record_readback_difference(source,actual,false);
    need(detail.size()<=900&&detail.find("unprinted-tail")==std::string::npos&&detail.find("path=/1/2/2")!=std::string::npos,"Modular bounded string diagnostic failed at a UTF-8 boundary or exposed the entire value");++count;
    auto escaped=source;for(auto& row:escaped)row[2][2]=std::string(10000,'\x01');detail=transition::record_readback_difference(source,escaped,false);
    need(detail.size()<=900&&detail.find("path=/0/2/2")!=std::string::npos,"Modular escaped diagnostic exceeded the ordered-action cause limit or lost its first difference");++count;
    return count;
}
unsigned modular_bounds_cases(){namespace transition=mt2mp::game::modular_transition;using R=mt2mp::subscriber_lifecycle::Record;using T=mt2mp::subscriber_lifecycle::Token;unsigned count=0;std::string error;nlohmann::json projected;
    const auto f=[](float v){return T{3,std::bit_cast<uint32_t>(v),{}};};const auto integer=[](uint32_t v){return T{4,v,{}};};
    std::vector<R> rows{{SIZE_MAX,0,{1,0,"mmoBuildingLayout"},{}}};
    const auto field=[&](const char* name,std::vector<T> tokens){const auto index=rows.size();rows.push_back({0,index-1,{1,0,name},std::move(tokens)});};
    field("grid",{});field("dimX",{integer(4)});field("dimY",{integer(1)});field("dimZ",{integer(4)});
    field("bounds",{f(-11.25f),f(0),f(-11.25f),f(11.25f),f(9),f(11.25f)});field("origin",{f(-6.75f),f(0),f(-6.75f)});field("gridSize",{f(4.5f)});field("scale",{f(1.25f)});
    for(size_t i=0;i<16;++i)rows.push_back({1,i,{0,0,{}},{integer(1)}});
    const auto source=rows;const auto input=transition::records_json(rows);
    need(transition::resolved_layout_record(rows,projected,error),error);++count;
    auto expected=input;for(size_t i:{size_t(0),size_t(2)})expected[5][3][i][1]=uint32_t(0xc1100000);for(size_t i:{size_t(3),size_t(5)})expected[5][3][i][1]=uint32_t(0x41100000);
    need(projected==expected,"Modular resolved cache failed exact -11.25/+11.25 to -9/+9 bound bits");++count;
    need(transition::records_json(rows)==input,"Modular bounds projection changed immutable source Records");++count;
    auto actual=expected;actual[5][3][0][1]=uint32_t(0xc1100001);need(actual!=projected&&!transition::record_readback_difference(projected,actual,false).empty(),"Modular projection accepted wrong regenerated native bound");++count;
    actual=expected;actual[8][3][0][1]=std::bit_cast<uint32_t>(1.f);need(actual!=projected,"Modular projection hid an unrelated authored scale difference");++count;
    rows=source;rows.resize(9);rows[2].tokens={integer(2)};rows[3].tokens={integer(2)};rows[4].tokens={integer(2)};rows[6].tokens={f(1.25f),f(0),f(-2.5f)};rows[7].tokens={f(1.5f)};
    for(size_t i=0;i<8;++i)rows.push_back({1,i,{0,0,{}},{integer(i==3?uint32_t(-1):0)}});
    need(transition::resolved_layout_record(rows,projected,error),error);++count;
    const std::array<float,6> sparse{2.f,1.5f,-3.25f,3.5f,4.5f,-1.75f};for(size_t i=0;i<6;++i)need(projected[5][3][i][1]==std::bit_cast<uint32_t>(sparse[i]),"Modular sparse occupied cell origin/spacing/height projection differs");++count;
    const auto sparse_rows=rows;rows[1].tokens.clear();for(size_t i=9;i<rows.size();++i)rows[1].tokens.push_back(rows[i].tokens[0]);rows.resize(9);
    nlohmann::json token_projection;need(transition::resolved_layout_record(rows,token_projection,error)&&token_projection[5]==projected[5],"Modular token and child grid bounds differ");++count;
    rows=sparse_rows;rows[6].tokens[0]=f(-1.5f);rows[7].tokens[0]={3,0x3f800001,{}};need(transition::resolved_layout_record(rows,projected,error)&&projected[5][3][3][1]==uint32_t(0x34800000),"Modular bounds fused multiply/add or lost original Float32 rounding");++count;
    rows=sparse_rows;for(size_t i=9;i<rows.size();++i)rows[i].tokens[0]=integer(0);need(transition::resolved_layout_record(rows,projected,error),error);for(const auto& token:projected[5][3])need(token[1]==uint32_t(0),"Modular empty occupied grid did not clear bounds");++count;
    rows=sparse_rows;rows[6].tokens[1]=f(7.f);need(transition::resolved_layout_record(rows,projected,error)&&projected[6]==transition::records_json(rows)[6],"Modular bounds projection hid authored origin.y normalization");++count;
    rows=sparse_rows;rows[2].tokens[0]=integer(3);need(!transition::resolved_layout_record(rows,projected,error)&&error.find("grid length")!=std::string::npos,error);++count;
    rows=sparse_rows;rows[5].tokens.pop_back();need(!transition::resolved_layout_record(rows,projected,error)&&error.find("token shape")!=std::string::npos,error);++count;
    rows=sparse_rows;rows.push_back({0,8,{1,0,"bounds"},source[5].tokens});need(!transition::resolved_layout_record(rows,projected,error)&&error.find("repeated")!=std::string::npos,error);++count;
    rows=sparse_rows;rows[7].tokens[0]=f(std::numeric_limits<float>::infinity());need(!transition::resolved_layout_record(rows,projected,error),"Modular bounds accepted nonfinite spacing");++count;
    rows=sparse_rows;rows[7].tokens[0]=f(std::numeric_limits<float>::max());need(!transition::resolved_layout_record(rows,projected,error)&&error.find("not finite")!=std::string::npos,error);++count;
    rows=sparse_rows;rows.push_back({9,0,{0,0,{}},{integer(1)}});need(!transition::resolved_layout_record(rows,projected,error)&&error.find("descendants")!=std::string::npos,error);++count;
    rows=sparse_rows;rows[0].label.text="mmoBuildingPlan";need(!transition::resolved_layout_record(rows,projected,error)&&error.find("Layout root")!=std::string::npos,error);++count;
    return count;
}
unsigned modular_target_cases(){namespace transition=mt2mp::game::modular_transition;unsigned count=0;std::string error;
    ModularIntent input;input.mode=1;input.region=60;input.target="action:target";input.before_record_sha=std::string(64,'a');
    DuplicateObject row;row.key=input.target;row.family=DuplicateFamily::Building;row.region=input.region;row.record_sha=input.before_record_sha;
    const auto original=row;
    need(transition::target_matches(input,&row,true,error),error);++count;
    need(!transition::target_matches(input,nullptr,true,error)&&error.find("row required=present actual=absent")!=std::string::npos,error);++count;
    need(!transition::target_matches(input,&row,false,error)&&error.find("handle required=present actual=absent")!=std::string::npos,error);++count;
    row.family=DuplicateFamily::Scenery;need(!transition::target_matches(input,&row,true,error)&&error.find("family required=")!=std::string::npos&&error.find("actual=")!=std::string::npos,error);++count;
    row=original;row.region=61;need(!transition::target_matches(input,&row,true,error)&&error.find("region required=60 actual=61")!=std::string::npos,error);++count;
    row=original;row.record_sha=std::string(64,'b');need(!transition::target_matches(input,&row,true,error)&&error.find("record_sha required="+input.before_record_sha+" actual="+row.record_sha)!=std::string::npos,error);++count;
    need(input.before_record_sha==original.record_sha&&row.record_sha!=original.record_sha,"Modular diagnostic guard rewrote complete captured or actual SHA");++count;
    ModularState before,after;before.props_sha=std::string(64,'c');after.props_sha=std::string(64,'d');ModularEffect effect;effect.untouched_sha=std::string(64,'e');effect.changed.push_back({original,row});
    const auto forward=transition::authored_status(before,after,effect);std::swap(effect.changed[0][0],effect.changed[0][1]);const auto inverse=transition::authored_status(after,before,effect);
    need(forward.at("before_sha")==inverse.at("after_sha")&&forward.at("after_sha")==inverse.at("before_sha")&&forward.at("untouched_sha")==inverse.at("untouched_sha")&&inverse.at("created").empty()&&inverse.at("removed").empty()&&inverse.at("changed").at(input.target)==nlohmann::json::array({row.record_sha,original.record_sha}),"Modular history authored projection lost full inverse Record hashes");++count;
    return count;
}
namespace modular_receipt_route_fixture {
namespace action=mt2mp::action_session;
unsigned observed=0,retired=0,fallbacks=0,current_reads=0;bool owner_ok=true,receipt_ok=true;
bool current(const action::Binding&,const std::string&,std::string& error){++current_reads;if(!owner_ok)error="original current World changed";return owner_ok;}
namespace ui_move_runtime::modular {
bool observe(const action::Binding&,const action::Request&,action::Evidence& out,std::string& error){++observed;if(!receipt_ok){error="original Modular inverse graph changed";return false;}out.undo_confirmed=true;return true;}
bool retire(const action::Binding&,const action::Request&,std::string& error){++retired;if(!receipt_ok)error="original Modular retirement failed";return receipt_ok;}
}
bool observe_route(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& error){
#include "modular_observe_route_actual.inc"
}
bool retire_route(const action::Binding& binding,const action::Request& request,std::string& error){
#include "modular_retire_route_actual.inc"
}
unsigned run(){unsigned count=0;action::Binding binding;action::Request request;action::Evidence actual;std::string error;
    for(const auto kind:{action::Action::UndoBuilding,action::Action::RedoBuilding}){
        request={};request.action=kind;request.target_action_uuid=fixture_action;request.canonical_record_key="modular:"+request.target_action_uuid;
        owner_ok=receipt_ok=true;observed=retired=fallbacks=current_reads=0;
        need(observe_route(binding,request,actual,error)&&observed==1&&current_reads==1&&!fallbacks&&actual.undo_confirmed,"Modular history routed into generic Building receipt lookup");++count;
        need(retire_route(binding,request,error)&&retired==1&&!fallbacks,"Modular history retirement lost its original route");++count;
        owner_ok=false;need(!observe_route(binding,request,actual,error)&&observed==1&&!fallbacks&&error=="original current World changed","Modular history bypassed current binding or lost cause");++count;owner_ok=true;
        receipt_ok=false;need(!observe_route(binding,request,actual,error)&&observed==2&&!fallbacks&&error=="original Modular inverse graph changed","Modular history concealed failed native inverse observation");++count;
        need(!retire_route(binding,request,error)&&retired==2&&!fallbacks&&error=="original Modular retirement failed","Modular retirement failed over into generic history");++count;receipt_ok=true;
        request.canonical_record_key="modular:other";
        need(!observe_route(binding,request,actual,error)&&observed==2&&fallbacks==1,"Modular receipt accepted another target action");++count;
        need(!retire_route(binding,request,error)&&retired==2&&fallbacks==2,"Modular retirement accepted another target action");++count;
    }
    request.action=action::Action::EditModularBuilding;observed=retired=fallbacks=current_reads=0;
    need(observe_route(binding,request,actual,error)&&observed==1&&!current_reads&&!fallbacks,"Forward Modular observer route changed");++count;
    need(retire_route(binding,request,error)&&retired==1&&!fallbacks,"Forward Modular retirement route changed");++count;
    request.action=action::Action::PlaceBuilding;request.canonical_record_key="modular:"+request.target_action_uuid;
    need(!observe_route(binding,request,actual,error)&&observed==1&&fallbacks==1,"Unrelated action entered Modular observer by key alone");++count;
    need(!retire_route(binding,request,error)&&retired==1&&fallbacks==2,"Unrelated action entered Modular retirement by key alone");++count;
    return count;
}
}
namespace modular_right_fixture {
struct Runtime {int memory=0;unsigned thread=7;};
struct Pending {ModularIntent intent;std::string id;};
Runtime runtime;std::atomic<Runtime*> published{&runtime};std::array<uintptr_t,3> originals{};std::optional<Pending> queued;
unsigned captures=0,original_calls=0,capture_calls=0,rejected=0;int32_t native_mode=1;bool read_ok=true,connected=true,started=true,capture_ok=true,throw_capture=false;std::string cause;
unsigned GetCurrentThreadId(){return 7;}
void reject(const std::string& error){++rejected;cause=error;}
namespace net::session_wire {bool connected(){return modular_right_fixture::connected;}}
namespace typed_action_runtime {nlohmann::json status(){return nlohmann::json{{"started",started}};}}
namespace q {
bool at(int,uintptr_t,size_t offset,int32_t& value,std::string& error){need(offset==0x168,"RightPress changed its original mode offset");if(!read_ok){error="original mode read failed";return false;}value=native_mode;return true;}
bool fail(std::string& error,const std::string& value){error=value;return false;}
}
bool mode(Runtime& r,uintptr_t receiver,bool prefab,Pending& out,std::string& e,bool right){
#include "modular_capture_mode_actual.inc"
    return true;
}
bool capture_input(Runtime& r,uintptr_t receiver,uintptr_t cursor,const void* position,bool prefab,Pending& out,std::string& e,bool right){
    ++capture_calls;need(receiver==11&&cursor==22&&position==reinterpret_cast<void*>(33)&&!prefab&&right,"RightPress capture lost native arguments or removal direction");
    if(throw_capture)throw std::runtime_error("original capture exception");
    if(!capture_ok||queued){e="original capture admission failed";return false;}
    if(!mode(r,receiver,prefab,out,e,right))return false;out.id="one-captured-action";return true;
}
void original(void* receiver,void* cursor,const void* position){need(receiver==reinterpret_cast<void*>(11)&&cursor==reinterpret_cast<void*>(22)&&position==reinterpret_cast<void*>(33),"RightPress original lost native arguments");++original_calls;}
#include "modular_right_entry_actual.inc"
void press(){right_entry(reinterpret_cast<void*>(11),reinterpret_cast<void*>(22),reinterpret_cast<void*>(33));}
void reset(){published.store(&runtime);runtime.thread=7;originals[2]=uintptr_t(&original);queued.reset();captures=original_calls=capture_calls=rejected=0;native_mode=1;read_ok=connected=started=capture_ok=true;throw_capture=false;cause.clear();}
unsigned run(){unsigned count=0;reset();
    press();need(queued&&queued->intent.mode==2&&queued->id=="one-captured-action"&&captures==1&&capture_calls==1&&!original_calls&&!rejected,"Joined RightPress did not capture exactly one normal removal");++count;
    press();need(captures==1&&queued->id=="one-captured-action"&&rejected==1&&cause=="original capture admission failed"&&!original_calls,"RightPress replayed the original after queued admission failed");++count;
    reset();connected=false;press();need(original_calls==1&&!captures&&!capture_calls&&!rejected,"Disconnected RightPress did not call its original once");++count;
    reset();started=false;press();need(original_calls==1&&!captures&&!capture_calls&&!rejected,"Inactive Controller RightPress did not call its original once");++count;
    for(int32_t value:{0,2,3,4}){reset();native_mode=value;press();need(!original_calls&&!capture_calls&&!captures&&!rejected,"Inert original right mode created a multiplayer action");++count;}
    reset();runtime.thread=8;press();need(rejected==1&&!original_calls&&!capture_calls&&cause.find("Mainthread")!=std::string::npos,"RightPress crossed Mainthread admission");++count;
    reset();read_ok=false;press();need(rejected==1&&cause=="original mode read failed"&&!original_calls&&!capture_calls,"RightPress hid native mode read failure");++count;
    for(int32_t value:{-1,5}){reset();native_mode=value;press();need(rejected==1&&!original_calls&&!capture_calls,"RightPress accepted an invalid native mode");++count;}
    reset();capture_ok=false;press();need(rejected==1&&cause=="original capture admission failed"&&!original_calls&&!captures&&!queued,"RightPress fell back to local mutation after capture rejection");++count;
    reset();throw_capture=true;press();need(rejected==1&&cause=="Modular RightPress capture: original capture exception"&&!original_calls&&!captures,"RightPress hid capture exception or invoked original");++count;
    reset();originals[2]=0;press();need(rejected==1&&!capture_calls&&!captures,"RightPress captured without a retained original");++count;
    reset();published.store(nullptr);press();need(rejected==1&&!capture_calls&&!captures,"RightPress captured without a published runtime");++count;
    reset();Pending input;std::string error;
    for(int32_t value=0;value<5;++value){native_mode=value;need(mode(runtime,11,value==0,input,error,false)&&input.intent.mode==value,"RightPress changed a normal left/Prefab capture mode");++count;}
    native_mode=1;need(mode(runtime,11,false,input,error,true)&&input.intent.mode==2,"RightPress failed its mode-1 to removal mapping");++count;
    for(int32_t value:{0,2,3,4}){native_mode=value;error.clear();need(!mode(runtime,11,value==0,input,error,true)&&error=="Modular native cursor mode differs","RightPress capture accepted another live cursor mode");++count;}
    reset();return count;
}
}

unsigned modular_transaction_cases(){namespace transaction=mt2mp::game::modular_transaction;unsigned count=0;std::string error;
    count+=modular_record_diagnostic_cases();
    count+=modular_bounds_cases();
    count+=modular_target_cases();
    count+=modular_receipt_route_fixture::run();
    count+=modular_right_fixture::run();
    for(uint8_t mode=0;mode<5;++mode){ModularTestPort port;transaction::Receipt receipt;ModularIntent input;input.mode=mode;need(transaction::execute(port,input,receipt,error)&&receipt.confirmed&&receipt.graph_committed&&receipt.edit_closed,error);++count;
        std::vector<std::string> expected=mode==0?std::vector<std::string>{"ready","purchase","begin","prefab","release","graph","clearance","cash","destroy","commit","end","readback"}:mode<3?std::vector<std::string>{"ready","begin","graph","inverse","can","release","edit","commit","end","readback"}:std::vector<std::string>{"ready","begin","graph","inverse","edit","release","commit","end","readback"};need(port.order==expected,"Modular original operation order differs");++count;
        need(receipt.calls==(mode?7u:10u)&&receipt.returned==receipt.calls,"Modular receipt count differs from the normal-input acceptance contract");++count;
        const auto prior=port.order;need(!transaction::execute(port,input,receipt,error)&&port.order==prior,"Modular attempted transaction replayed");++count;
        for(const auto& failed:expected){port={};port.fail_at=failed;receipt={};need(!transaction::execute(port,input,receipt,error)&&error=="original "+failed+" cause",error);++count;
            if(failed=="ready")need(!receipt.attempted,"Pure admission failure marked mutation");else need(receipt.attempted,"Attempted original failure lost its uncertainty");++count;
            const auto ends=std::count(port.order.begin(),port.order.end(),"end");need(ends<=1,"EndEdit retried after uncertainty");++count;
        }
    }
    ModularTestPort port;transaction::Receipt receipt;ModularIntent input;input.mode=3;port.fail_at="edit";port.fail_cleanup=true;need(!transaction::execute(port,input,receipt,error)&&error=="original edit cause; cleanup: EndEdit cleanup cause"&&receipt.unknown,error);++count;
    port={};receipt={};port.throw_edit=true;need(!transaction::execute(port,input,receipt,error)&&error=="Modular transaction exception: edit exception cause"&&receipt.edit_closed,error);++count;
    port={};receipt={};input.mode=1;port.fail_at="can";port.cancel_enabled=true;need(!transaction::execute(port,input,receipt,error)&&port.cancel_called&&!receipt.unknown&&receipt.edit_closed&&error=="original can cause",error);++count;
    need(port.order==std::vector<std::string>{"ready","begin","graph","inverse","can","discard","end"},"Rejected native graph must be disposed before EndEdit");++count;
    const auto before_calls=port.order;need(!transaction::execute(port,input,receipt,error)&&port.order==before_calls,"Confirmed no-effect original was replayed");++count;
    namespace transition=mt2mp::game::modular_transition;auto original=object(old(30),DuplicateFamily::Building),changed=original;changed.record_sha=std::string(64,'e');auto untouched=object(old(31));std::map<std::string,DuplicateObject> before{{original.key,original},{untouched.key,untouched}},after{{changed.key,changed},{untouched.key,untouched}};ModularEffect effect;std::string hashed;
    auto hash=[&](const std::string& value,std::string& digest,std::string&){hashed=value;digest=std::string(64,'a');return true;};need(transition::difference(before,after,hash,effect,error)&&effect.changed==std::vector<std::array<DuplicateObject,2>>{{original,changed}}&&effect.removed.empty()&&effect.created.empty()&&hashed.find(untouched.key)!=std::string::npos&&hashed.find(original.key)==std::string::npos,error);++count;
    ModularState authored_before,authored_after;authored_before.props_sha=std::string(64,'a');authored_after.props_sha=std::string(64,'b');
    auto diagnostic=transition::authored_status(authored_before,authored_after,effect);
    need(diagnostic["changed"][original.key]==nlohmann::json::array({original.record_sha,changed.record_sha})&&diagnostic["before_sha"]==authored_before.props_sha&&diagnostic["after_sha"]==authored_after.props_sha,"Modular diagnostic lost a complete Record-only change");++count;
    need(diagnostic["created"].empty()&&diagnostic["removed"].empty()&&diagnostic["untouched_sha"]==effect.untouched_sha,"Modular diagnostic invented membership or lost untouched witness");++count;
    effect.removed={original};effect.created={changed};effect.changed.clear();diagnostic=transition::authored_status(authored_before,authored_after,effect);
    need(diagnostic["removed"][original.key]==original.record_sha&&diagnostic["created"][changed.key]==changed.record_sha&&diagnostic["changed"].empty(),"Modular diagnostic lost exact removed/created Record hashes");++count;
    return count;
}
}
