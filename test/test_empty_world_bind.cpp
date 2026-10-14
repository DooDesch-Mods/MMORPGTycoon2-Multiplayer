#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>
#include "vendor/nlohmann/json.hpp"
using json=nlohmann::json;
using std::uint64_t;
static constexpr const char* uuid="30d0c486-b499-4baf-9e12-36081ad2e1bf";
static std::string sha(64,'a');
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
namespace remove_diagnostics {
    static json source;static std::string failure;
    bool bindings(json& out,std::string& error){if(!failure.empty()){error=failure;return false;}out=source;return true;}
}
namespace scenery_kiss {bool read_action_prop(uint64_t uid,native::BuildingState& out,std::string& error){
    if(uid!=9){error="fresh UID absent";return false;}out={};return true;
}}
namespace world_state {
    struct Origin{std::string snapshot_uuid,sha256;uint64_t world_epoch=0,generation=0;};
    struct SnapshotObject{std::string object_key,record_key;int state=0;};
    static int building_state(const native::BuildingState&){return 1;}
    struct Table{Origin origin;std::vector<SnapshotObject> objects;bool bound=false;
        bool bind_snapshot(const Origin& value,const std::vector<SnapshotObject>& rows,uint64_t revision,std::string& error){
            if(revision||value.snapshot_uuid!=uuid||value.sha256.size()!=64||!value.world_epoch||!value.generation){error="real world-table origin invalid";return false;}
            origin=value;objects=rows;bound=true;error.clear();return true;
        }
    };
}
struct Engine {world_state::Table world_;uint32_t thread_=0;
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
    native_open_runtime::reads={witness(),witness()};native_open_runtime::cursor=0;native_open_runtime::fail_at=-1;
    remove_diagnostics::failure.clear();remove_diagnostics::source={{"world_epoch",41},{"world_generation",7},
        {"baseline_snapshot_id",uuid},{"baseline_sha256",sha},{"bindings",json::array()}};
}
static bool run(Engine& engine,std::string& error){action::Binding binding;return engine.bind(uuid,binding,error);}
int main(){
    std::string error;Engine engine;
    reset();need(run(engine,error),"confirmed zero-building Host capture rejected");
    need(engine.world_.bound&&engine.world_.objects.empty()&&engine.world_.origin.sha256==sha,"empty baseline origin lost");
    reset();engine={};remove_diagnostics::source["bindings"].push_back({{"snapshot_id",uuid},{"record_key",sha+":world:building:1"},{"local_uid",9},{"revoked",false}});
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
    reset();engine={};remove_diagnostics::source["bindings"].push_back({{"snapshot_id",uuid},{"record_key",sha+":world:building:1"},{"local_uid",9},{"revoked",true}});
    need(!run(engine,error)&&!engine.world_.bound,"revoked-only baseline accepted");
    reset();engine={};remove_diagnostics::source.erase("baseline_sha256");
    need(!run(engine,error)&&!engine.world_.bound,"missing SHA authority accepted");
    return assertions<12?1:0;
}
