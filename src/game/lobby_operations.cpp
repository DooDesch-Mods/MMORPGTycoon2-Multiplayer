#include "lobby_operations.h"
#include "mt2mp/core/logger.h"
#include <random>
#include <array>
#include <set>
#include "mt2mp/net/snapshot_world_path.h"
namespace mt2mp::game::lobby_operations {
namespace {nlohmann::json request,response;std::string active,run_id;bool(*idle_reader)()=nullptr;}
void configure(const std::string& run,bool(*idle)()){run_id=run;idle_reader=idle;}
bool pending(){return !active.empty();}
bool available(){return !run_id.empty();}
bool executor_idle(){return available()&&!pending()&&idle_reader&&idle_reader();}
std::string fresh_id(){
    std::random_device random;std::array<unsigned char,16> bytes{};for(auto& byte:bytes)byte=static_cast<unsigned char>(random());
    bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;
    constexpr char hex[]="0123456789abcdef";std::string id;
    for(size_t i=0;i<bytes.size();++i){if(i==4||i==6||i==8||i==10)id+='-';id+=hex[bytes[i]>>4];id+=hex[bytes[i]&15];}
    return id;
}
bool submit(const std::string& op,const nlohmann::json& args,std::string& id,std::string& error) {
    if(run_id.empty()){error="owned native executor run is unavailable";return false;}
    if(pending()){error="native lobby operation already pending id="+active;return false;}
    if(op!="capture_local_snapshot"&&op!="capture_snapshot"&&op!="capture_recovery_snapshot"&&op!="load_snapshot"&&op!="save_native"){error="native lobby operation is not admitted: "+op;return false;}
    if((op=="load_snapshot"&&(!args.is_object()||args.size()!=1||(!args.contains("snapshot_id")&&!args.contains("selected_file"))))||
       (op!="load_snapshot"&&op!="save_native"&&(!args.is_object()||!args.empty()))||
       (op=="save_native"&&(!args.is_object()||args.size()!=6||!args.contains("kind")||!args.contains("option")||
           !args.contains("filename")||!args.contains("mode")||!args.contains("session")||!args.contains("world_epoch")))){error="native lobby operation arguments invalid";return false;}
    id=fresh_id();
    active=id;request={{"schema_version",1},{"id",id},{"run_id",run_id},{"op",op},{"args",args}};
    LOG_INFO("[NativeLobby] Native operation queued op={} id={}",op,id);error.clear();return true;
}
bool submit_export_save(const nlohmann::json& args,const std::string& id,std::string& error){
    const std::string op="save_native";
    if(run_id.empty()){error="owned native executor run is unavailable";return false;}
    if(pending()){error="native lobby operation already pending id="+active;return false;}
    if(op!="capture_local_snapshot"&&op!="capture_snapshot"&&op!="load_snapshot"&&op!="save_native"){error="native lobby operation is not admitted: "+op;return false;}
    if((op=="load_snapshot"&&(!args.is_object()||args.size()!=1||(!args.contains("snapshot_id")&&!args.contains("selected_file"))))||
       (op!="load_snapshot"&&op!="save_native"&&(!args.is_object()||!args.empty()))||
       (op=="save_native"&&(!args.is_object()||args.size()!=6||!args.contains("kind")||!args.contains("option")||
           !args.contains("filename")||!args.contains("mode")||!args.contains("session")||!args.contains("world_epoch")))){error="native lobby operation arguments invalid";return false;}
    static std::set<std::string> consumed;
    if(!net::snapshot_world_path::valid_uuid(id)||!consumed.insert(id).second){error="Client export capture nonce invalid or previously consumed";return false;}
    active=id;request={{"schema_version",1},{"id",id},{"run_id",run_id},{"op",op},{"args",args}};
    LOG_INFO("[NativeLobby] Client export capture queued id={}",id);error.clear();return true;
}
bool take_request(nlohmann::json& out){if(request.is_null())return false;out=std::move(request);request=nullptr;return true;}
bool receive(const nlohmann::json& result){if(active.empty()||result.value("id",std::string{})!=active)return false;response=result;return true;}
bool release_for_leave(bool& ready,std::string& error){
    ready=false;error.clear();
    if(active.empty()){ready=true;return true;}
    if(!request.is_null()){
        if(request.value("id",std::string{})!=active||!response.is_null()){
            error="native Leave queued operation identity/state differs";return false;
        }
        LOG_INFO("[NativeLobby] Native operation cancelled before execution for Leave id={} op={}",active,request.value("op",std::string{}));
        request=nullptr;active.clear();ready=true;return true;
    }
    if(response.is_null())return true;
    if(response.value("id",std::string{})!=active){error="native Leave completed operation identity differs";return false;}
    if(!response.value("ok",false))
        LOG_ERROR("[NativeLobby] Native operation completed with failure before Leave id={} cause={}",active,
            response.value("error",nlohmann::json::object()).value("message",std::string("native operation failed")));
    else LOG_INFO("[NativeLobby] Native operation completed before Leave id={}",active);
    response=nullptr;active.clear();ready=true;return true;
}
bool take_response(const std::string& id,nlohmann::json& out){if(id!=active||response.is_null())return false;out=std::move(response);response=nullptr;active.clear();return true;}
}
