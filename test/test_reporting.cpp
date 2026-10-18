#include "mt2mp/core/reporting.h"
#include "mt2mp/core/logger.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <condition_variable>
#include <mutex>
using namespace mt2mp;
using json=nlohmann::json;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::mutex upload_mutex;
std::condition_variable upload_cv;
bool upload_entered=false,upload_released=false;
unsigned uploads=0;
json fake_upload(const json& value){
    std::unique_lock lock(upload_mutex);++uploads;upload_entered=true;upload_cv.notify_all();
    if(!upload_cv.wait_for(lock,std::chrono::seconds(5),[]{return upload_released;}))throw std::runtime_error("test upload release timeout");
    return {{"report_id",value.at("report_id")},{"stored_at","2026-10-17T00:00:00Z"}};
}
json wait_ready(){
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(std::chrono::steady_clock::now()<until){auto state=reporting::status();if(state.at("phase")=="ready")return state;std::this_thread::yield();}
    throw std::runtime_error("report preparation timed out required<5s");
}
int main(int argc,char** argv){
    try{
        require(argc==2,"test profile path required");const std::filesystem::path profile=std::filesystem::path(argv[1])/reporting::random_id();
        const auto id=reporting::random_id();require(reporting::valid_id(id),"UUIDv4 generation invalid");
        const auto safe=reporting::redact("token=secret123 peer=76561198000000000 ip=192.168.1.3 path=C:\\Users\\Alice\\file");
        for(const auto* secret:{"secret123","76561198000000000","192.168.1.3","Alice"})require(safe.find(secret)==std::string::npos,"private data survived redaction");
        require(reporting::redact("Authorization: Bearer header-secret").find("header-secret")==std::string::npos,"authorization bearer secret survived redaction");
        const auto numeric=reporting::redact("peer76561198000000000 lobby=1234567890123456789");
        require(numeric.find("76561198000000000")==std::string::npos&&numeric.find("1234567890123456789")==std::string::npos,"embedded account or lobby number survived redaction");
        reporting::initialize(profile,json::object());reporting::bind_session(id,2);
        for(unsigned i=0;i<400;++i)reporting::event("action.confirmed",{{"revision",uint64_t(i)},{"secret","must not enter bundle"}},reporting::random_id());
        reporting::open_report();const auto ready=wait_ready();
        require(ready.at("event_count")==256,"event ring is not bounded");
        const auto path=profile/"reports"/(ready.at("report_id").get<std::string>()+".json");
        reporting::shutdown();
        std::ifstream input(path);const auto value=json::parse(input);
        require(value.at("diagnostic_session")==id&&value.at("peer_slot")==2,"shared diagnostic identity lost");
        require(value.at("consent")==false,"local capture granted upload consent");
        require(value.at("dropped_events")==144,"ring overflow is not reported");
        require(value.dump().find("must not enter bundle")==std::string::npos,"arbitrary event fields entered bundle");
        require(std::filesystem::file_size(path)<256*1024,"bundle size exceeded");
        reporting::initialize(profile,json::object());const auto recovered=reporting::status();
        require(recovered.at("report_id")==ready.at("report_id"),"unsent report did not survive restart");
        reporting::shutdown();
        const auto old_log=profile/"mt2mp.log";
        {std::ofstream output(old_log,std::ios::binary);output<<std::string(6*1024*1024,'x');}
        Logger::instance().init(profile.string());Logger::instance().shutdown();
        require(std::filesystem::file_size(profile/"mt2mp.previous.log")==4*1024*1024,"previous log is not bounded");
        reporting::test::set_upload(fake_upload);
        reporting::initialize(profile,json::object());reporting::automatic_reports(true);reporting::bind_session(reporting::random_id(),1);
        {std::unique_lock lock(upload_mutex);require(upload_cv.wait_for(lock,std::chrono::seconds(3),[]{return upload_entered;}),"automatic report did not enter test transport");}
        reporting::bind_session(reporting::random_id(),1);reporting::bind_session(reporting::random_id(),1);
        reporting::automatic_reports(false);
        require(!reporting::status().at("automatic").get<bool>()&&reporting::status().at("automatic_inflight").get<bool>(),"opt-out did not distinguish in-flight upload");
        {std::lock_guard lock(upload_mutex);upload_released=true;upload_cv.notify_all();}
        reporting::shutdown();
        require(uploads==1,"opt-out allowed queued automatic upload to start");
        std::cout<<"PASS identity, redaction, allowlist,256-event cap, local consent and restart recovery\n";return 0;
    }catch(const std::exception& error){reporting::shutdown();std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
