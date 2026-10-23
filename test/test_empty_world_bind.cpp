#include <iostream>
#include "snapshot_building_key.h"
#include "developer_staff/construction_occurrence.h"
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <stdexcept>
#include "vendor/nlohmann/json.hpp"
using json=nlohmann::json;
using std::uint64_t;
namespace startup_timing {
struct Phase {Phase(const char*,const std::string&){} bool complete(bool result){return result;}};
template<class Call> bool run(const char*,const std::string&,Call&& call){return call();}
}
static constexpr const char* uuid="30d0c486-b499-4baf-9e12-36081ad2e1bf";
static std::string fixture_sha(64,'a');
static uint32_t GetCurrentThreadId(){return 7;}
namespace action {struct Binding{uint64_t native_epoch=0,native_generation=0;};}
namespace native {struct BuildingState{int value=1;};}
namespace building_mixed_identity::opened_file {struct CurrentWorld{
    void* game=nullptr;void* mode=nullptr;void* state=nullptr;void* world=nullptr;void* map=nullptr;
    uint64_t generation=0,world_epoch=0;std::string uuid,sha256;
};}
namespace native_open_runtime {
    using World=building_mixed_identity::opened_file::CurrentWorld;
    static std::vector<World> reads;static size_t cursor=0;static int fail_at=-1;
    bool read_current_world(World& out,std::string& error){
        if(int(cursor)==fail_at){error="actual current-world witness absent";++cursor;return false;}
        if(cursor>=reads.size()){error="unexpected witness read";return false;}
        out=reads[cursor++];return true;
    }
}
namespace mt2mp::build_actions::private_native::host {
class RootSceneryReadBatch {
public:
    static inline unsigned active=0,finalized=0;
    static inline bool fail_finish=false;
    RootSceneryReadBatch(){++active;}
    ~RootSceneryReadBatch(){--active;}
    static bool read_current(building_mixed_identity::opened_file::CurrentWorld& out,std::string& error){
        if(!active){error="complete bind Origin read outside readonly traversal";return false;}
        return native_open_runtime::read_current_world(out,error);
    }
    template<class World> static bool read_current(World& out,std::string& error){
        if(!active){error="complete bind Origin read outside readonly traversal";return false;}
        return current(out,error);
    }
    bool finish(std::string& error){++finalized;if(fail_finish){error="original complete bind closing World proof failed";return false;}return true;}
};
}
namespace remove_diagnostics {
    static json source;static std::string failure;
    namespace scene_scenery {
        std::string fixture_key;uint64_t fixture_uid=0;
        bool registered(const std::string& snapshot,const std::string& key,uint64_t uid){
            return snapshot==uuid&&!fixture_key.empty()&&key==fixture_key&&uid==fixture_uid;
        }
    }
    bool bindings(json& out,std::string& error){if(!failure.empty()){error=failure;return false;}out=source;return true;}
}
namespace scenery_kiss {bool read_action_prop(uint64_t uid,native::BuildingState& out,std::string& error){
    if(!mt2mp::build_actions::private_native::host::RootSceneryReadBatch::active){error="fresh property read outside outer bind traversal";return false;}
    if(uid!=9){error="fresh UID absent";return false;}out={};return true;
}}
namespace developer_staff::construction {
    struct LocalJob {std::string building_key;uintptr_t job=1;uint64_t building_uid=9;};
    inline std::vector<LocalJob> fixture_jobs;
    inline bool fixture_failure=false,fixture_table_bound=false;
    inline unsigned fixture_calls=0;
    bool capture_loaded(const std::string& snapshot,const action::Binding& binding,std::vector<LocalJob>& out,std::string& error){
        ++fixture_calls;if(fixture_failure){error="original loaded Job admission failed";return false;}
        if(snapshot!=uuid||binding.native_epoch!=41||binding.native_generation!=7||!fixture_table_bound||
           mt2mp::build_actions::private_native::host::RootSceneryReadBatch::active){error="loaded Jobs preceded exact snapshot publication or retained closed read scope";return false;}
        out=fixture_jobs;return true;
    }
}
namespace world_state {
    struct Origin{std::string snapshot_uuid,sha256;uint64_t world_epoch=0,generation=0;};
    struct SnapshotObject{std::string object_key,record_key;int state=0;};
    static int building_state(const native::BuildingState&){return 1;}
    struct Table{Origin origin;std::vector<SnapshotObject> objects;bool bound=false;
        struct Row{struct State{bool alive=true;} state;};std::map<std::string,Row> rows_;
        const auto& rows()const{return rows_;}
        bool bind_snapshot(const Origin& value,const std::vector<SnapshotObject>& values,uint64_t revision,std::string& error){
            if(revision||value.snapshot_uuid!=uuid||value.sha256.size()!=64||!value.world_epoch||!value.generation){error="real world-table origin invalid";return false;}
            if(!mt2mp::build_actions::private_native::host::RootSceneryReadBatch::finalized){error="World table publication preceded complete bind finalization";return false;}
            origin=value;objects=values;bound=true;rows_.clear();for(const auto& row:values)rows_[row.object_key]={};
            developer_staff::construction::fixture_table_bound=true;error.clear();return true;
        }
    };
}
struct Engine {world_state::Table world_;uint32_t thread_=0;
    action::Binding loaded_construction_binding_{};std::string loaded_construction_snapshot_;
    std::vector<developer_staff::construction::LocalJob> loaded_construction_;
    bool current(const action::Binding& binding,const std::string&,std::string& error){
        if(binding.native_epoch!=41||binding.native_generation!=7){error="current binding differs";return false;}return true;
    }
#include "empty_world_bind_body.inc"
};
static int assertions=0;
static void need(bool okay,const char* reason){++assertions;if(!okay)throw std::runtime_error(reason);}
static building_mixed_identity::opened_file::CurrentWorld witness(){
    return {reinterpret_cast<void*>(1),reinterpret_cast<void*>(2),reinterpret_cast<void*>(3),
        reinterpret_cast<void*>(4),reinterpret_cast<void*>(5),7,41,"local-origin-uuid",std::string(64,'b')};
}
static void reset(){
    developer_staff::construction::fixture_jobs.clear();developer_staff::construction::fixture_calls=0;
    developer_staff::construction::fixture_failure=developer_staff::construction::fixture_table_bound=false;
    mt2mp::build_actions::private_native::host::RootSceneryReadBatch::finalized=0;
    mt2mp::build_actions::private_native::host::RootSceneryReadBatch::fail_finish=false;
    native_open_runtime::reads={witness(),witness()};native_open_runtime::cursor=0;native_open_runtime::fail_at=-1;
    remove_diagnostics::scene_scenery::fixture_key.clear();remove_diagnostics::scene_scenery::fixture_uid=0;
    remove_diagnostics::failure.clear();remove_diagnostics::source={{"world_epoch",41},{"world_generation",7},
        {"baseline_snapshot_id",uuid},{"baseline_sha256",fixture_sha},{"bindings",json::array()}};
}
static bool run(Engine& engine,std::string& error){action::Binding binding;return engine.bind(uuid,binding,error);}
namespace source_fixture {
struct Binding {int32_t link_id=44;uint64_t local_uid=9;std::string record_key;};
struct Status {
    bool bindings_confirmed=true,world_witness_current=true,load_pending=false;
    std::string capture_uuid=uuid,snapshot_uuid=uuid,snapshot_sha256=fixture_sha,state="confirmed";
    uint64_t snapshot_bytes=400,session_epoch=41;
    std::vector<Binding> buildings{Binding{}};
};
Status captured,fixture_load,fixture_sealed;
int seals=0,imports=0;bool import_ok=true;
std::string imported_snapshot,seal_cause;
namespace building_capture_identity {
using Status=source_fixture::Status;
enum class SealResult {Pending,Confirmed,Failed};
SealResult outcome=SealResult::Confirmed;
Status status(){return captured;}
SealResult seal(const std::string& id,const std::string& digest,uint64_t bytes,uint64_t epoch,Status& out,std::string& error){
    ++seals;
    need(id==captured.capture_uuid&&digest==captured.snapshot_sha256&&bytes==captured.snapshot_bytes&&epoch==captured.session_epoch,"capture seal arguments changed");
    out=fixture_sealed;error=seal_cause;return outcome;
}
}
namespace normal_load_identity {Status status(){return fixture_load;}}
namespace snapshot_building_key {bool valid(const std::string& key){return mt2mp::game::snapshot_building_key::valid(key);}}
struct World {std::string uuid=::uuid,sha256=fixture_sha;};
int actual=1;
bool import_bindings(const std::vector<Binding>& rows,const std::string& snapshot,const std::string& digest,int world,json& out,std::string& error){
    ++imports;imported_snapshot=snapshot;
    need(world==actual&&digest==fixture_sha,"import lost actual world/digest");
    if(!import_ok){error="actual native building import failed";return false;}
    for(const auto& row:rows)out.push_back({{"key",row.record_key},{"uid",row.local_uid}});
    return true;
}
namespace scene_scenery {
unsigned append_calls=0;bool append_ok=true;json saved_rows=json::array();
bool append_saved(json& rows,std::string& error){
    ++append_calls;if(!append_ok){error="original Scene saved alias admission failed";return false;}
    for(const auto& row:saved_rows)rows.push_back(row);return true;
}
}
bool select([[maybe_unused]] const std::string& session_snapshot,const World& world,json& out,std::string& error){
#include "snapshot_binding_source_body.inc"
    out={{"snapshot",baseline_snapshot},{"sha",baseline_sha},{"bindings",list}};return true;
}
void reset(){scene_scenery::append_calls=0;scene_scenery::append_ok=true;scene_scenery::saved_rows=json::array();captured={};fixture_load={};fixture_sealed={};seals=imports=0;import_ok=true;seal_cause.clear();imported_snapshot.clear();building_capture_identity::outcome=building_capture_identity::SealResult::Confirmed;}
void checks(){
    json out;std::string error;World world;
    reset();captured.capture_uuid="11111111-1111-4111-8111-111111111111";seal_cause="native Game/Mode/GameState/World/Map receiver changed since begin";
    building_capture_identity::outcome=building_capture_identity::SealResult::Failed;
    need(select(uuid,world,out,error)&&seals==0&&imports==1&&imported_snapshot==uuid,"prior session capture blocked confirmed new load");
    need(out["snapshot"]==uuid&&out["bindings"][0]["key"]==fixture_sha+":world:building:44","new load canonical identity lost");
    reset();need(select(uuid,world,out,error)&&seals==1&&imports==1,"current session capture did not seal/import exactly once");
    reset();seal_cause="original current-world witness failure";building_capture_identity::outcome=building_capture_identity::SealResult::Failed;
    need(!select(uuid,world,out,error)&&error==seal_cause&&seals==1&&imports==0,"current capture seal failure silently fell back to load");
    reset();building_capture_identity::outcome=building_capture_identity::SealResult::Pending;
    need(!select(uuid,world,out,error)&&error=="fresh native capture world binding unavailable"&&imports==0,"pending current capture fabricated complete load authority");
    for(int kind=0;kind<5;++kind){
        reset();if(kind==0)fixture_sealed.bindings_confirmed=false;if(kind==1)fixture_sealed.world_witness_current=false;if(kind==2)fixture_sealed.capture_uuid="foreign";if(kind==3)fixture_sealed.snapshot_sha256=std::string(64,'b');if(kind==4)++fixture_sealed.snapshot_bytes;
        need(!select(uuid,world,out,error)&&imports==0&&error=="fresh native capture identity differs","current capture changed sealed identity accepted");
    }
    for(int kind=0;kind<3;++kind){
        reset();if(kind==0)captured.bindings_confirmed=false;if(kind==1)captured.world_witness_current=false;if(kind==2)captured.capture_uuid.clear();
        need(select(uuid,world,out,error)&&seals==0&&imports==1,"incomplete capture blocked independently confirmed load");
    }
    for(int kind=0;kind<4;++kind){
        reset();captured.capture_uuid="old";if(kind==0)fixture_load.state="failed";if(kind==1)fixture_load.load_pending=true;if(kind==2)fixture_load.snapshot_uuid="foreign";if(kind==3)fixture_load.snapshot_sha256=std::string(64,'b');
        need(!select(uuid,world,out,error)&&seals==0&&imports==0&&error=="current complete NativeSnapshot building binding unavailable","unverified current load admitted after stale capture");
    }
    reset();import_ok=false;need(!select(uuid,world,out,error)&&seals==1&&error=="actual native building import failed","current capture import cause lost");
    reset();captured.capture_uuid="old";import_ok=false;need(!select(uuid,world,out,error)&&seals==0&&error=="actual native building import failed","new load import cause lost");
    reset();captured.capture_uuid="old";fixture_load.buildings.clear();need(select(uuid,world,out,error)&&out["bindings"].empty()&&out["snapshot"]==uuid,"confirmed empty current load lost baseline");
    reset();scene_scenery::saved_rows.push_back({{"key","confirmed Scene alias"},{"uid",uint64_t(19)}});
    need(select(uuid,world,out,error)&&scene_scenery::append_calls==1&&out["bindings"].size()==2&&out["bindings"][1]["uid"]==19,"complete saved Scene aliases were not joined after original source import");
    reset();scene_scenery::append_ok=false;
    need(!select(uuid,world,out,error)&&scene_scenery::append_calls==1&&error=="original Scene saved alias admission failed","saved Scene admission failure lost its original cause");
    reset();captured.capture_uuid="old";fixture_load.buildings[0].link_id=-1;need(!select(uuid,world,out,error)&&imports==0&&error=="loaded native building linkID is negative","invalid fixture_load link admitted");
    reset();captured.capture_uuid="old";fixture_load.buildings[0].record_key="foreign";need(!select(uuid,world,out,error)&&imports==0&&error=="loaded native building record key differs from native linkID","foreign saved key admitted");
}
}

#include "identity_refund_split_cases.inc"

int main(){try{
    std::string error;Engine engine;
    reset();mt2mp::build_actions::private_native::host::RootSceneryReadBatch::fail_finish=true;
    need(!run(engine,error)&&!engine.world_.bound&&error=="original complete bind closing World proof failed","actual outer bind must finalize empty-world Origin before table publication");
    need(mt2mp::build_actions::private_native::host::RootSceneryReadBatch::active==0,"failed actual bind retains no readonly scope");
    reset();engine={};remove_diagnostics::source["bindings"].push_back({{"snapshot_id",uuid},{"record_key",fixture_sha+":world:building:1"},{"local_uid",9},{"revoked",false}});
    mt2mp::build_actions::private_native::host::RootSceneryReadBatch::fail_finish=true;
    need(!run(engine,error)&&!engine.world_.bound&&error=="original complete bind closing World proof failed","actual outer bind cannot publish fresh per-object rows after final Origin failure");
    reset();engine={};need(run(engine,error)&&mt2mp::build_actions::private_native::host::RootSceneryReadBatch::finalized==1&&mt2mp::build_actions::private_native::host::RootSceneryReadBatch::active==0,"actual bind finalizes exactly once and leaves no scope after successful publication");
    reset();need(run(engine,error),"confirmed zero-building Host capture rejected");
    need(engine.world_.bound&&engine.world_.objects.empty()&&engine.world_.origin.sha256==fixture_sha,"empty baseline origin lost");
    reset();engine={};remove_diagnostics::source["bindings"].push_back({{"snapshot_id",uuid},{"record_key",fixture_sha+":world:building:1"},{"local_uid",9},{"revoked",false}});
    need(run(engine,error),"nonempty original building path rejected");need(engine.world_.objects.size()==1,"nonempty row lost");
    reset();engine={};remove_diagnostics::source["baseline_snapshot_id"]="00000000-0000-0000-0000-000000000000";
    need(!run(engine,error)&&!engine.world_.bound,"stale snapshot UUID accepted");
    reset();engine={};remove_diagnostics::source["world_epoch"]=42;
    need(!run(engine,error)&&error.find("epoch")!=std::string::npos,"stale epoch accepted");
    reset();engine={};remove_diagnostics::source["world_generation"]=8;
    need(!run(engine,error)&&error.find("generation")!=std::string::npos,"stale generation accepted");
    reset();engine={};native_open_runtime::fail_at=0;
    need(!run(engine,error)&&error=="actual current-world witness absent","missing witness cause lost");
    reset();engine={};native_open_runtime::reads[1].map=reinterpret_cast<void*>(99);
    need(!run(engine,error)&&error.find("world changed")!=std::string::npos,"foreign map accepted");
    reset();engine={};remove_diagnostics::failure="native snapshot seal failed";
    need(!run(engine,error)&&error==remove_diagnostics::failure,"binding source failure suppressed");
    reset();engine={};remove_diagnostics::source["bindings"].push_back({{"snapshot_id",uuid},{"record_key",fixture_sha+":world:building:1"},{"local_uid",9},{"revoked",true}});
    need(!run(engine,error)&&!engine.world_.bound,"revoked-only baseline accepted");
    reset();engine={};remove_diagnostics::source.erase("baseline_sha256");
    need(!run(engine,error)&&!engine.world_.bound,"missing SHA authority accepted");
    reset();engine={};remove_diagnostics::scene_scenery::fixture_key="action:33333333-3333-4333-8333-333333333333";
    remove_diagnostics::scene_scenery::fixture_uid=19;
    remove_diagnostics::source["bindings"].push_back({{"snapshot_id",uuid},{"record_key",remove_diagnostics::scene_scenery::fixture_key},{"local_uid",19},{"revoked",false}});
    need(run(engine,error)&&engine.world_.bound&&engine.world_.objects.empty(),"confirmed Scene Scenery alias became a fake root world row");
    reset();engine={};remove_diagnostics::scene_scenery::fixture_key="action:33333333-3333-4333-8333-333333333333";
    remove_diagnostics::scene_scenery::fixture_uid=20;
    remove_diagnostics::source["bindings"].push_back({{"snapshot_id",uuid},{"record_key",remove_diagnostics::scene_scenery::fixture_key},{"local_uid",19},{"revoked",false}});
    need(!run(engine,error)&&error=="fresh UID absent"&&!engine.world_.bound,"unconfirmed Scene full UID bypassed the original root native reader");
    reset();engine={};const std::string root_record=fixture_sha+":world:scenery:inline:region{59}/sceneryManager/type[8]/scenery[0]";
    const std::string canonical_root=std::string(64,'c')+":world:scenery:inline:region{59}/sceneryManager/type[8]/scenery[0]";
    remove_diagnostics::source["bindings"].push_back({{"snapshot_id",uuid},{"record_key",canonical_root},{"native_record_key",root_record},{"local_uid",9},{"revoked",false}});
    need(run(engine,error)&&engine.world_.objects.size()==1&&engine.world_.objects[0].object_key==canonical_root&&engine.world_.objects[0].record_key==root_record&&engine.world_.origin.sha256==fixture_sha,"actual bind lost canonical Root key/current saved record separation");
    reset();engine={};const std::string action_root="action:33333333-3333-4333-8333-333333333333";
    remove_diagnostics::source["bindings"].push_back({{"snapshot_id",uuid},{"record_key",action_root},{"native_record_key",root_record},{"local_uid",9},{"revoked",false}});
    need(run(engine,error)&&engine.world_.objects[0].object_key==action_root&&engine.world_.objects[0].record_key==root_record&&engine.world_.origin.sha256==fixture_sha,"actual Root action alias bypassed or lost current native file authority");
    reset();engine={};const auto loaded_key=fixture_sha+":world:building:1";
    remove_diagnostics::source["bindings"].push_back({{"snapshot_id",uuid},{"record_key",loaded_key},{"local_uid",9},{"revoked",false}});
    developer_staff::construction::fixture_jobs={{loaded_key,500,9}};
    need(run(engine,error)&&engine.loaded_construction_.size()==1&&engine.loaded_construction_[0].building_key==loaded_key&&
         engine.loaded_construction_snapshot_==uuid&&engine.loaded_construction_binding_.native_generation==7&&developer_staff::construction::fixture_calls==1,
         "actual snapshot bind lost one-shot loaded construction ownership");
    reset();engine={};developer_staff::construction::fixture_jobs={{loaded_key,500,9}};
    need(!run(engine,error)&&engine.loaded_construction_.empty()&&error=="Loaded construction lacks an admitted alive snapshot Building row",
         "loaded construction accepted a target absent from the actual admitted world table");
    reset();engine={};developer_staff::construction::fixture_failure=true;
    need(!run(engine,error)&&engine.loaded_construction_.empty()&&error=="original loaded Job admission failed",
         "actual loaded admission failure was suppressed");
    source_fixture::checks();
    identity_refund_fixture::checks();
    std::cout << "PASS " << assertions << " actual binding/identity assertions\n";
    return assertions<12?1:0;
}catch(const std::exception& failure){std::cerr << failure.what() << "\n";return 1;}
}
