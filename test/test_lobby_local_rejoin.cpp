#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
using json=nlohmann::json;
int checks=0;
void need(bool value,const char* detail){++checks;if(!value)throw std::runtime_error(detail);}
struct Config {bool local_test=true,is_host=false;static Config& instance(){static Config value;return value;}};
bool local_admitted=false,pending=false,host_world_pick_requested=false,current_host_world=false,chat_draft_clear_failed=false;
uint64_t generation=0,last_leave_confirmed=0,chat_last_sequence=0;
std::string cause,chat_error,ui_status,selected_world;
int failures=0,logs=0;
bool fail(const std::string& error){cause=error;++failures;return false;}
#define LOG_INFO(...) (++logs)
namespace net {
uint64_t peer=2,session=3;uint8_t mask=3;
bool connected=true,host_left=false,join_ok=true;int joins=0;
std::string snapshot="fresh-shared";
namespace session_wire {
uint64_t local_peer(){return peer;}uint64_t session_id(){return session;}
uint8_t peer_mask(){return mask;}bool connected(){return net::connected;}
}
std::string current_snapshot_id(){return snapshot;}
bool local_host_left(const std::string&){return host_left;}
bool join_local_lobby(std::string& error){++joins;if(!join_ok){error="original local hello queue failure";return false;}return true;}
namespace chat {
struct Runtime {int admissions=0;static Runtime& instance(){static Runtime value;return value;}
void set_lobby_admitted(bool value){need(value,"auto admission must not retire Chat");++admissions;}};
}
}
namespace building_mixed_identity::opened_file {
struct Status {bool origin_confirmed=true,completed_origin=true,current_origin_available=true;std::string uuid="fresh-shared";uint64_t generation=7,origin_world_epoch=8;};
struct CurrentWorld {std::string uuid="fresh-shared";uint64_t generation=7,world_epoch=8;};
Status actual;CurrentWorld current;int reads=0;bool read_ok=true,change_membership=false;
Status status(){return actual;}
bool read_completed_world(uint64_t expected_generation,CurrentWorld& out,std::string& error){
    ++reads;need(expected_generation==actual.generation,"fresh read must use exact origin generation");
    if(!read_ok){error="original native world witness failure";return false;}
    out=current;if(change_membership)++net::session;return true;
}
}
#include "lobby_local_rejoin_bodies.inc"
json preparation,shared;
void reset(){
    Config::instance()={};local_admitted=pending=host_world_pick_requested=current_host_world=chat_draft_clear_failed=false;
    generation=last_leave_confirmed=chat_last_sequence=0;cause.clear();chat_error="old";ui_status.clear();selected_world.clear();
    failures=logs=0;net::peer=2;net::session=3;net::mask=3;net::connected=true;net::host_left=false;net::join_ok=true;net::joins=0;net::snapshot="fresh-shared";
    net::chat::Runtime::instance().admissions=0;
    using namespace building_mixed_identity::opened_file;actual={};current={};reads=0;read_ok=true;change_membership=false;
    preparation={{"state",0},{"leaving",false},{"error",""}};
    shared={{"phase","active"},{"world_authority_valid",true},{"id","fresh-shared"},{"first_failure",""}};
}
bool run(bool ingame=true){std::string error;return adopt_local_shared_world(ingame,preparation,shared,error);}
int main()try{
    reset();need(run(),"verified current Client world is admitted");
    need(local_admitted&&!Config::instance().is_host&&net::joins==1&&generation==1&&net::chat::Runtime::instance().admissions==1,"existing admission sets role/Chat/generation once");
    need(building_mixed_identity::opened_file::reads==1&&logs==1,"native witness is freshly read once");
    need(run()&&net::joins==1&&building_mixed_identity::opened_file::reads==1,"subsequent frames do not replay admission");
    reset();need(run(false)&&!local_admitted&&net::joins==0,"initial Title does not auto-admit");
    need(admit_local(false)&&local_admitted&&net::joins==1,"initial manual Client admission remains functional");
    reset();Config::instance().local_test=false;need(run()&&!local_admitted&&net::joins==0,"Steam remains unchanged");
    reset();Config::instance().is_host=true;need(run()&&!local_admitted&&net::joins==0,"Host is not auto-admitted");
    for(int guard=0;guard<12;++guard){reset();
        switch(guard){case 0:pending=true;break;case 1:last_leave_confirmed=1;break;case 2:cause="prior failure";break;
        case 3:preparation["state"]=1;break;case 4:preparation["leaving"]=true;break;case 5:preparation["error"]="prepare failed";break;
        case 6:shared["phase"]="failed";break;case 7:shared["world_authority_valid"]=false;break;case 8:shared["first_failure"]="transfer failed";break;
        case 9:net::host_left=true;break;case 10:shared["id"]="stale";break;case 11:shared["id"]="";break;}
        need(run()&&!local_admitted&&net::joins==0,"pending/leave/failure/stale snapshot must not admit");
    }
    for(int guard=0;guard<7;++guard){reset();
        switch(guard){case 0:net::peer=1;break;case 1:net::peer=5;break;case 2:net::session=0;break;case 3:net::mask=2;break;
        case 4:net::mask=1;break;case 5:net::connected=false;break;case 6:net::peer=0;break;}
        need(run()&&!local_admitted&&net::joins==0,"invalid current physical membership must not admit");
    }
    for(int guard=0;guard<6;++guard){reset();auto& origin=building_mixed_identity::opened_file::actual;
        switch(guard){case 0:origin.origin_confirmed=false;break;case 1:origin.completed_origin=false;break;case 2:origin.current_origin_available=false;break;
        case 3:origin.uuid="stale";break;case 4:origin.generation=0;break;case 5:origin.origin_world_epoch=0;break;}
        need(!run()&&!local_admitted&&net::joins==0&&!cause.empty(),"stale/unconfirmed origin fails visibly before admission");
    }
    for(int guard=0;guard<5;++guard){reset();using namespace building_mixed_identity::opened_file;
        switch(guard){case 0:read_ok=false;break;case 1:current.uuid="stale";break;case 2:++current.generation;break;
        case 3:++current.world_epoch;break;case 4:change_membership=true;break;}
        need(!run()&&!local_admitted&&net::joins==0,"fresh origin/membership mismatch blocks admission");
        if(guard==0)need(cause=="original native world witness failure","native read failure keeps original cause");
    }
    reset();net::join_ok=false;need(!run()&&!local_admitted&&cause=="original local hello queue failure","local admission preserves failed send cause");
    need(run()&&net::joins==1&&!local_admitted&&net::chat::Runtime::instance().admissions==0,"failed admission is not replayed next frame");
    std::cout<<checks<<" actual-body checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
