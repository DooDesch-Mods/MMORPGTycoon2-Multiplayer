#include "developer_staff/probe.h"
#include "developer_staff/job_census.h"
#include <iostream>
#include <stdexcept>
namespace staff=mt2mp::game::developer_staff;
namespace fixture {
unsigned checks=0,reads=0,censuses=0,copies=0;bool thread_ok=true,read_ok=true,jobs_ok=true,locked=false;
int changed=0;staff::Capture base;
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
bool read(const Read& copy,const Capture&,Census&,std::string& e){++fixture::censuses;if(!fixture::jobs_ok){e="original queue read failure";return false;}int value=0;return copy(1,&value,sizeof value,e);}
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
void reset(){using namespace fixture;reads=censuses=copies=0;thread_ok=read_ok=jobs_ok=true;changed=0;locked=false;base={};base.state.rows.resize(50);}
}
int main(){try{using namespace fixture;nlohmann::json out;std::string e;
    reset();need(staff::observe_fixture(out,e)&&out.at("observed")==true&&out.at("jobs").at("identity_proven")==false&&reads==2&&censuses==1&&copies==1,"actual observation did not bind both pool reads and diagnostic census");
    reset();locked=true;need(staff::observe_fixture(out,e)&&out.at("fixture_locked")==true&&out.at("fixture_cause")=="retained original failure","read-only census erased the retained mutation failure");
    reset();thread_ok=false;need(!staff::observe_fixture(out,e)&&out.at("observed")==false&&e=="original Mainthread guard"&&!reads&&!censuses,"thread failure was hidden or read unauthorized memory");
    reset();read_ok=false;need(!staff::observe_fixture(out,e)&&out.at("observed")==false&&e=="original pool read failure"&&!censuses,"first native pool read failure was hidden");
    reset();jobs_ok=false;need(!staff::observe_fixture(out,e)&&out.at("observed")==false&&e=="original queue read failure"&&reads==1,"queue error was hidden or success-shaped");
    for(int field=1;field<=8;++field){reset();changed=field;need(!staff::observe_fixture(out,e)&&out.at("observed")==false&&e.find("Origin/ledger/Clock/fixed50/local")!=std::string::npos,"outer native binding mutation produced observed success");}
    std::cout<<"PASS "<<checks<<" actual read-only census endpoint checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
