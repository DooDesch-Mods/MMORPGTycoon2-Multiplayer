#include "developer_staff/probe.h"
#include "developer_staff/job_census.h"
#include <iostream>
#include <stdexcept>
namespace staff=mt2mp::game::developer_staff;
namespace fixture {
unsigned checks=0,reads=0,censuses=0,copies=0;bool thread_ok=true,read_ok=true,jobs_ok=true,locked=false;
unsigned movement_reads=0;bool movement_ok=true;int changed=0;unsigned target_reads=0;bool targets_ok=true,second_queue_ok=true,change_queue=false;staff::Capture base;
void need(bool v,const char* e){++checks;if(!v)throw std::runtime_error(e);}
}
namespace mt2mp::game::developer_staff {
bool same_origin(const Origin& a,const Origin& b){return a.uuid==b.uuid&&a.world_epoch==b.world_epoch&&a.generation==b.generation;}
bool read_process(Capture& out,std::string& e){++fixture::reads;if(!fixture::read_ok){e="original pool read failure";return false;}out=fixture::base;
    if(fixture::reads==2)switch(fixture::changed){case 1:++out.pin.origin.world_epoch;break;case 2:++out.pin.ledger.line_counts[19];break;
    case 3:out.pin.time_scale=3;break;case 4:out.pin.pause_scale=0.5f;break;case 5:++out.image;break;case 6:++out.thread;break;
    case 7:out.state.rows[0].active=true;break;case 8:++out.local.lounge;break;default:break;}
    return true;}
nlohmann::json encode(const Capture&){return {{"current_native",true}};}
nlohmann::json encode(const Receipt& r){return {{"cause",r.cause},{"unknown",r.unknown}};}
namespace job_census {
bool read(const Read& copy,const Capture&,Census& out,std::string& e){++fixture::censuses;if(fixture::censuses==2&&!fixture::second_queue_ok){e="original second queue failure";return false;}if(fixture::censuses==2&&fixture::change_queue)++out.capacity;if(!fixture::jobs_ok){e="original queue read failure";return false;}int value=0;return copy(1,&value,sizeof value,e);}
bool observe_building_targets(const Read&,const Capture&,const Census&,nlohmann::json& out,std::string& e){++fixture::target_reads;if(!fixture::targets_ok){e="original target binding failure";return false;}out={{"observed",true},{"building_key","action:actual-test-key"},{"job_identity_proven",false}};return true;}
bool observe_movement(const Read&,const Capture&,const Census&,nlohmann::json& out,std::string& e){++fixture::movement_reads;if(!fixture::movement_ok){e="original movement failure";return false;}out={{"observed",true},{"actors",50}};return true;}
nlohmann::json encode(const Census&){return {{"diagnostic_only",true},{"identity_proven",false}};}
}
namespace {
struct Memory {bool copy(uintptr_t,void*,size_t,std::string&){++fixture::copies;return true;}} memory;
struct Probe {std::map<std::string,Receipt> receipts;bool locked(){return fixture::locked;}std::string cause(){return "retained original failure";}const auto& retained(){return receipts;}} probe;
bool thread(std::string& e){if(!fixture::thread_ok)e="original Mainthread guard";return fixture::thread_ok;}
bool fail(std::string& e,const std::string& why){e=why;return false;}
}
#include "developer_job_observe.inc"
}
namespace {
void reset(){using namespace fixture;reads=censuses=copies=target_reads=movement_reads=0;movement_ok=true;targets_ok=second_queue_ok=true;change_queue=false;thread_ok=read_ok=jobs_ok=true;changed=0;locked=false;base={};base.state.rows.resize(50);}
}
int main(){try{using namespace fixture;nlohmann::json out;std::string e;
    reset();need(staff::observe_fixture(out,e)&&out.at("observed")==true&&out.at("jobs").at("identity_proven")==false&&reads==2&&censuses==2&&copies==2&&target_reads==1&&movement_reads==1&&out.at("developer_movement").at("actors")==50&&out.at("job_building_targets").at("building_key")=="action:actual-test-key","actual observation did not bind both pool reads and diagnostic census");
    reset();locked=true;need(staff::observe_fixture(out,e)&&out.at("fixture_locked")==true&&out.at("fixture_cause")=="retained original failure","read-only census erased the retained mutation failure");
    reset();thread_ok=false;need(!staff::observe_fixture(out,e)&&out.at("observed")==false&&e=="original Mainthread guard"&&!reads&&!censuses,"thread failure was hidden or read unauthorized memory");
    reset();read_ok=false;need(!staff::observe_fixture(out,e)&&out.at("observed")==false&&e=="original pool read failure"&&!censuses,"first native pool read failure was hidden");
    reset();jobs_ok=false;need(!staff::observe_fixture(out,e)&&out.at("observed")==false&&e=="original queue read failure"&&reads==1,"queue error was hidden or success-shaped");
    reset();targets_ok=false;need(!staff::observe_fixture(out,e)&&out.at("observed")==false&&e=="original target binding failure"&&reads==1&&censuses==1,"target binding failure was hidden or returned success");
    reset();second_queue_ok=false;need(!staff::observe_fixture(out,e)&&out.at("observed")==false&&e=="original second queue failure"&&reads==1,"second queue read failure was hidden");
    reset();change_queue=true;need(!staff::observe_fixture(out,e)&&out.at("observed")==false&&e.find("actual queue/actor graph")!=std::string::npos&&reads==1,"changed queue graph returned success");
    reset();movement_ok=false;need(!staff::observe_fixture(out,e)&&out.at("observed")==false&&e=="original movement failure"&&reads==1&&censuses==1,"movement failure was hidden or returned success");
    for(int field=1;field<=8;++field){reset();changed=field;need(!staff::observe_fixture(out,e)&&out.at("observed")==false&&e.find("Origin/ledger/Clock/fixed50/local")!=std::string::npos,"outer native binding mutation produced observed success");}
    std::cout<<"PASS "<<checks<<" actual read-only census endpoint checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
