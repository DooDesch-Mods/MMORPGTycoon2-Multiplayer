#include "../src/net/software_sync.cpp"
#include <iostream>

using json=nlohmann::json;
namespace {
mt2mp::net::SidecarHandler handler;
std::vector<json> sent;
bool queue_ok=true, read_ok=true;
mt2mp::game::native::SoftwareCache native_pair{700,40};
uint64_t native_revision=1, writes=0;
}
namespace mt2mp {
Config& Config::instance() { static Config instance; return instance; }
Logger& Logger::instance() { static Logger instance; return instance; }
void Logger::log(LogCategory,LogLevel level,const char*,int,const std::string& text) { std::cout<<int(level)<<" "<<text<<'\n'; }
}
namespace mt2mp::net {
SidecarClient& SidecarClient::instance() { static auto* instance=new SidecarClient; return *instance; }
void SidecarClient::register_handler(uint8_t channel,SidecarHandler callback) {
    if(channel!=7) throw std::runtime_error("unexpected fixture channel");
    handler=callback;
}
bool SidecarClient::send(uint8_t channel,const uint8_t* data,uint32_t size,uint64_t) {
    if(channel!=7) throw std::runtime_error("unexpected fixture send channel");
    if(!queue_ok) return false;
    sent.push_back(json::parse(data,data+size)); return true;
}
bool local_session_connected() { return true; }
}
namespace mt2mp::game::native {
bool in_game() { return true; }
bool read_software_state(SoftwareState& state,std::string& error) {
    if(!read_ok) { error="fixture native read failure"; return false; }
    state.rating=native_pair.rating; state.fun=native_pair.fun; return true;
}
bool apply_software_cache(uint64_t,uint64_t revision,const SoftwareCache& before,const SoftwareCache& value,SoftwareCacheReceipt& receipt,std::string& error) {
    receipt.before=receipt.after=native_pair;
    if(revision==native_revision&&value.rating==native_pair.rating&&value.fun==native_pair.fun) { receipt.already_applied=true; return true; }
    if(revision<=native_revision||before.rating!=native_pair.rating||before.fun!=native_pair.fun) { error="fixture native before/revision guard"; return false; }
    native_pair=value; native_revision=revision; ++writes; receipt.after=value; receipt.written=true; return true;
}
}
void require(bool condition,const char* cause) { if(!condition) throw std::runtime_error(cause); }
void receive(uint64_t sender,json value) {
    const auto bytes=value.dump(); handler(sender,reinterpret_cast<const uint8_t*>(bytes.data()),uint32_t(bytes.size()));
}
void start(bool host) {
    mt2mp::Config::instance().is_host=host; sent.clear(); queue_ok=read_ok=true;
    native_pair={700,40}; native_revision=1; writes=0;
    mt2mp::net::begin_software_sync(123,native_pair);
}
json ready() { return {{"type","software_ready"},{"epoch",123},{"revision",1},{"values",{{"rating",700},{"fun",40}}}}; }
json update() { return {{"type","software_state"},{"epoch",123},{"revision",2},{"previous_revision",1},
    {"before",{{"rating",700},{"fun",40}}},{"values",{{"rating",600},{"fun",39}}}}; }
int main() {
    try {
        using namespace mt2mp::net;
        initialize_software_sync();
        start(true); receive(2,ready()); native_pair={600,39}; tick_software_sync();
        require(sent.size()==1&&sent[0].at("revision")==2,"host publishes changed native pair once");
        last_send={}; tick_software_sync(); require(sent.size()==1,"pending native acknowledgement prevents replay");
        receive(2,{{"type","software_applied"},{"epoch",123},{"revision",2},{"values",{{"rating",600},{"fun",39}}}});
        require(peers.at(2).acknowledged==2&&!peers.at(2).pending,"host verifies matching native acknowledgement");
        native_pair={800,38}; last_send={}; tick_software_sync();
        require(sent.size()==2&&sent[1].at("before").at("rating")==600,"next update uses acknowledged pair");
        start(false); receive(1,update()); require(writes==1&&native_pair.rating==600,"client applies native state once");
        receive(1,update()); require(writes==1&&sent.size()==3,"matching duplicate acknowledges without writing");
        auto wrong=update(); wrong["values"]["rating"]=500; receive(1,wrong);
        require(!failure.empty()&&writes==1,"conflicting duplicate rejected");
        for(const auto& field: {"epoch","revision","previous_revision"}) {
            start(false); wrong=update(); wrong[field]=1.5; receive(1,wrong);
            require(!failure.empty()&&!writes,"float protocol identity rejected");
        }
        start(false); wrong=update(); wrong["epoch"]=124; receive(1,wrong); require(!failure.empty()&&!writes,"other world epoch rejected");
        start(false); receive(3,update()); require(!failure.empty()&&!writes,"nonhost state rejected");
        start(false); wrong=update(); wrong["previous_revision"]=3; receive(1,wrong); require(!failure.empty()&&!writes,"out of order state rejected");
        start(false); wrong=update(); wrong["before"]["rating"]=500; receive(1,wrong); require(!failure.empty()&&!writes,"native before mismatch rejects mutation");
        start(true); receive(2,ready()); read_ok=false; tick_software_sync(); require(failure=="fixture native read failure","native read cause retained");
        start(true); receive(2,ready()); native_pair={600,39}; queue_ok=false; tick_software_sync(); require(!failure.empty()&&!peers.at(2).pending,"failed send does not invent pending delivery");
        start(true); wrong=ready(); wrong["values"]["fun"]=39; receive(2,wrong); require(!failure.empty()&&peers.empty(),"initial acknowledgement mismatch rejected");
        std::cout<<"passed: native state sequencing, before guards, dedupe, epoch, acknowledgements and failure causes\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<"error: software stream fixture: "<<error.what()<<'\n'; return 1; }
}
