#include "lobby_operations.h"
#include "mt2mp/core/logger.h"
#include <random>
#include <array>
namespace mt2mp::game::lobby_operations {
namespace {nlohmann::json request,response;std::string active,run_id;}
void configure(const std::string& run){run_id=run;}
bool pending(){return !active.empty();}
bool available(){return !run_id.empty();}
bool submit(const std::string& op,const nlohmann::json& args,std::string& id,std::string& error) {
    if(run_id.empty()){error="owned native executor run is unavailable";return false;}
    if(pending()){error="native lobby operation already pending id="+active;return false;}
    if(op!="capture_local_snapshot"&&op!="capture_snapshot"&&op!="load_snapshot"){error="native lobby operation is not admitted: "+op;return false;}
    if((op=="load_snapshot"&&(!args.is_object()||args.size()!=1||!args.contains("snapshot_id")))||
       (op!="load_snapshot"&&(!args.is_object()||!args.empty()))){error="native lobby operation arguments invalid";return false;}
    std::random_device random;std::array<unsigned char,16> bytes{};for(auto& byte:bytes)byte=static_cast<unsigned char>(random());
    bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;
    constexpr char hex[]="0123456789abcdef";id.clear();
    for(size_t i=0;i<bytes.size();++i){if(i==4||i==6||i==8||i==10)id+='-';id+=hex[bytes[i]>>4];id+=hex[bytes[i]&15];}
    active=id;request={{"schema_version",1},{"id",id},{"run_id",run_id},{"op",op},{"args",args}};
    LOG_INFO("[NativeLobby] Native operation queued op={} id={}",op,id);error.clear();return true;
}
bool take_request(nlohmann::json& out){if(request.is_null())return false;out=std::move(request);request=nullptr;return true;}
bool receive(const nlohmann::json& result){if(active.empty()||result.value("id",std::string{})!=active)return false;response=result;return true;}
bool take_response(const std::string& id,nlohmann::json& out){if(id!=active||response.is_null())return false;out=std::move(response);response=nullptr;active.clear();return true;}
}
