#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "vendor/nlohmann/json.hpp"
#include "src/game/world_selection.h"

namespace world_selection=mt2mp::game::world_selection;
struct Config {
    bool local_test=false;int peer_id=1;
    static Config& instance(){static Config value;return value;}
};
namespace net {
struct SteamMatchmaking {
    bool host=true,metadata_ok=true;
    std::vector<std::string> publications;
    static SteamMatchmaking& instance(){static SteamMatchmaking value;return value;}
    bool is_host()const{return host;}
    bool publish_lobby_data(const std::string& key,const std::string& value,std::string& error){
        if(key!="game_name")throw std::runtime_error("unexpected metadata key");
        publications.push_back(value);
        if(!metadata_ok){error="original metadata write failure";return false;}
        return true;
    }
};
}
bool member=true,current_host_world=false,load_attempted=false,choosing=false,publication_failed=false;
std::string selected_world,selected_file,current_host_world_error,publication_error,ui_status,cause;
uint64_t command_failures=0;
unsigned error_logs=0,info_logs=0,name_reads=0;
std::array<unsigned char,0x500> game_storage{};
std::string actual_folder="New World";
bool name_readable=true;
bool admitted(){return member;}
bool text(const void* p,std::string& out,size_t limit){
    if(p!=game_storage.data()+0x470||limit!=200)throw std::runtime_error("wrong actual folder ABI offset or bound");
    ++name_reads;if(!name_readable)return false;out=actual_folder;return true;
}
#define LOG_ERROR(...) (++error_logs)
#define LOG_INFO(...) (++info_logs)
#include "auto_world_failure_body.inc"

namespace lobby_prepare {
unsigned current_calls=0,selected_calls=0,start_calls=0;
bool prepare_ok=true,start_ok=true,requested=false;
nlohmann::json status(){return {{"start_requested",requested}};}
std::string prepared_path;
bool prepare_current(std::string& error){++current_calls;if(!prepare_ok)error="current capture failure";return prepare_ok;}
bool prepare(const std::string& path,std::string& error){++selected_calls;prepared_path=path;if(!prepare_ok)error="selected load failure";return prepare_ok;}
bool start(std::string& error){++start_calls;requested=start_ok;if(!start_ok)error="start admission failure";return start_ok;}
}
namespace lobby_operations {bool idle=true;bool executor_idle(){return idle;}}
#include "auto_world_adopt_body.inc"
void start_command(const std::string& cmd){
#include "auto_world_start_body.inc"
}
void check(bool value,const char* cause_text){if(!value)throw std::runtime_error(cause_text);}
void reset(){
    lobby_operations::idle=true;Config::instance()=Config{};net::SteamMatchmaking::instance()=net::SteamMatchmaking{};
    member=true;current_host_world=false;load_attempted=false;choosing=false;publication_failed=false;
    selected_world.clear();selected_file.clear();current_host_world_error.clear();publication_error.clear();ui_status.clear();cause.clear();
    command_failures=0;error_logs=info_logs=name_reads=0;actual_folder="New World";name_readable=true;
    lobby_prepare::current_calls=lobby_prepare::selected_calls=lobby_prepare::start_calls=0;
    lobby_prepare::prepare_ok=lobby_prepare::start_ok=true;lobby_prepare::requested=false;lobby_prepare::prepared_path.clear();
}
int main(){
    const nlohmann::json idle={{"state",0}},preparing={{"state",2}};
    std::string error;
    reset();lobby_operations::idle=false;
    check(adopt_current_host_world(game_storage.data(),true,false,idle,error)&&name_reads==0&&lobby_prepare::current_calls==0&&!load_attempted,"pending diagnostic load admitted automatic capture");
    check(adopt_current_host_world(game_storage.data(),true,false,idle,error)&&lobby_prepare::start_calls==0,"pending diagnostic observation retried automatic start");
    lobby_operations::idle=true;
    check(adopt_current_host_world(game_storage.data(),true,false,idle,error)&&lobby_prepare::current_calls==1&&lobby_prepare::start_calls==1,"retired diagnostic load did not resume automatic start");
    reset();choosing=true;selected_file="save/old/autosave.vrz";
    check(adopt_current_host_world(game_storage.data(),true,false,idle,error),"active Host automatic start failed");
    check(current_host_world&&selected_world=="New World"&&selected_file.empty()&&!choosing,"new unsaved source not adopted");
    check(load_attempted&&lobby_prepare::current_calls==1&&lobby_prepare::start_calls==1,"new Host world did not request capture and start together");
    check(net::SteamMatchmaking::instance().publications==std::vector<std::string>{"New World"},"metadata missing or wrong");
    check(adopt_current_host_world(game_storage.data(),true,false,idle,error)&&name_reads==1,"repeat observation replayed adoption");
    actual_folder="InternalSnapshot";
    check(adopt_current_host_world(game_storage.data(),true,false,preparing,error)&&selected_world=="New World","preparation replaced source");
    check(adopt_current_host_world(game_storage.data(),false,true,preparing,error)&&current_host_world,"internal Title transition cleared source");
    check(lobby_prepare::current_calls==1&&lobby_prepare::start_calls==1,"observation replayed start");

    reset();member=false;
    check(adopt_current_host_world(game_storage.data(),true,false,idle,error)&&name_reads==0,"no-lobby world adopted");
    member=true;net::SteamMatchmaking::instance().host=false;
    check(adopt_current_host_world(game_storage.data(),true,false,idle,error)&&name_reads==0,"Client world adopted");
    Config::instance().local_test=true;Config::instance().peer_id=2;
    check(adopt_current_host_world(game_storage.data(),true,false,idle,error)&&name_reads==0,"local Client world adopted");
    Config::instance().peer_id=1;
    check(adopt_current_host_world(game_storage.data(),true,false,idle,error)&&lobby_prepare::start_calls==1&&net::SteamMatchmaking::instance().publications.empty(),"local Host did not start or attempted Steam metadata");

    reset();name_readable=false;
    check(!adopt_current_host_world(game_storage.data(),true,false,idle,error)&&error=="active Host world name is unreadable or invalid","native folder failure lost");
    check(!adopt_current_host_world(game_storage.data(),true,false,idle,error)&&error_logs==1,"same read failure logged repeatedly");
    name_readable=true;actual_folder="../invalid";
    check(!adopt_current_host_world(game_storage.data(),true,false,idle,error)&&!current_host_world,"invalid native folder accepted");
    actual_folder="Recovered";
    check(adopt_current_host_world(game_storage.data(),true,false,idle,error)&&current_host_world_error.empty()&&info_logs==2,"name recovery lost");

    reset();net::SteamMatchmaking::instance().metadata_ok=false;
    check(!adopt_current_host_world(game_storage.data(),true,false,idle,error)&&publication_failed&&!current_host_world&&error=="original metadata write failure","metadata failure lost");
    check(!adopt_current_host_world(game_storage.data(),true,false,idle,error)&&net::SteamMatchmaking::instance().publications.size()==1&&lobby_prepare::current_calls==0,"metadata failure retried or captured");

    reset();lobby_prepare::prepare_ok=false;
    check(!adopt_current_host_world(game_storage.data(),true,false,idle,error)&&cause=="current capture failure"&&load_attempted,"automatic capture failure hidden");
    check(adopt_current_host_world(game_storage.data(),true,false,idle,error)&&lobby_prepare::current_calls==1&&lobby_prepare::start_calls==0,"automatic capture failure replayed");
    reset();lobby_prepare::start_ok=false;
    check(!adopt_current_host_world(game_storage.data(),true,false,idle,error)&&cause=="start admission failure","automatic Start failure hidden");
    check(adopt_current_host_world(game_storage.data(),true,false,idle,error)&&lobby_prepare::current_calls==1&&lobby_prepare::start_calls==1,"automatic Start failure replayed");

    reset();selected_file="save/Selected/periodic.vrz";start_command("MT2MPStart");
    check(load_attempted&&lobby_prepare::selected_calls==1&&lobby_prepare::start_calls==1&&lobby_prepare::prepared_path==selected_file,"one action did not load selected world and latch Start");
    start_command("MT2MPPrepare");
    check(lobby_prepare::selected_calls==1&&lobby_prepare::start_calls==1,"compatibility alias replayed work");
    reset();selected_file="save/Selected/periodic.vrz";start_command("MT2MPPrepare");
    check(lobby_prepare::selected_calls==1&&lobby_prepare::start_calls==1,"Prepare alias did not start");
    reset();start_command("MT2MPStart");check(command_failures==1&&lobby_prepare::start_calls==0,"empty source started");
    reset();current_host_world=true;member=false;start_command("MT2MPStart");check(lobby_prepare::current_calls==0,"no-lobby Start prepared");
    reset();current_host_world=true;net::SteamMatchmaking::instance().host=false;start_command("MT2MPStart");check(lobby_prepare::current_calls==0,"Client Start prepared");
    reset();current_host_world=true;lobby_prepare::prepare_ok=false;start_command("MT2MPStart");
    check(load_attempted&&lobby_prepare::start_calls==0&&cause=="current capture failure","prepare failure hidden or Start continued");
    start_command("MT2MPStart");check(lobby_prepare::current_calls==1,"failed request replayed");
    std::cout<<"PASS actual single-action Host load/start and auto-world adoption: roles, one attempt, metadata and native failure causes\n";
}
