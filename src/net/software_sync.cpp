#include "mt2mp/net/software_sync.h"
#include "mt2mp/net/software_snapshot.h"
#include "mt2mp/net/local_session.h"
#include "mt2mp/net/sidecar_client.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include <chrono>
#include <map>
#include <limits>
#include <stdexcept>

namespace mt2mp::net {
using json=nlohmann::json;
using Cache=game::native::SoftwareCache;
using Clock=std::chrono::steady_clock;
namespace {
struct Peer {
    uint64_t acknowledged=1, pending=0;
    Cache actual{}, sent{};
    Clock::time_point dispatched{};
    bool stalled=false;
};
std::map<uint64_t,Peer> peers;
uint64_t epoch=0, revision=0, applied_updates=0;
Cache initial{}, published{};
std::string failure;
Clock::time_point last_send{};
bool equal(const Cache& a,const Cache& b) { return a.rating==b.rating&&a.fun==b.fun; }
json cache(const Cache& value) { return {{"rating",value.rating},{"fun",value.fun}}; }
Cache parse_cache(const json& value) {
    if(!value.is_object()||value.size()!=2) throw std::runtime_error("software pair requires exactly rating and fun");
    auto initial_form=value; initial_form["revision"]=1;
    return parse_initial_software_cache(initial_form);
}
uint64_t integer(const json& value,const char* field) {
    const auto& number=value.at(field);
    if(!number.is_number_unsigned()) throw std::runtime_error(std::string("software unsigned integer required: ")+field);
    const auto result=number.get<uint64_t>();
    if(!result) throw std::runtime_error(std::string("software nonzero integer required: ")+field);
    return result;
}
void send(const json& value,uint64_t target) {
    const auto bytes=value.dump();
    if(!session_wire::send(7,reinterpret_cast<const uint8_t*>(bytes.data()),uint32_t(bytes.size()),target))
        throw std::runtime_error("software state transport queue rejected");
}
void fail(const std::string& cause) {
    if(failure.empty()) {
        failure=cause;
        LOG_NET_ERROR("[SoftwareSync] Failed epoch={} revision={} cause={}",epoch,revision,cause);
    }
}
void receive(uint64_t sender,const uint8_t* bytes,uint32_t length) {
    try {
        if(!failure.empty()) return;
        if(!epoch||length>2048||!session_wire::connected()) throw std::runtime_error("software message requires active epoch, session and <=2048 bytes");
        const auto value=json::parse(bytes,bytes+length);
        if(integer(value,"epoch")!=epoch) throw std::runtime_error("software message epoch differs");
        const auto type=value.at("type").get<std::string>();
        const auto incoming=integer(value,"revision");
        if(Config::instance().is_host) {
            if(sender<2||sender>4) throw std::runtime_error("software acknowledgement peer outside 2..4");
            if(type=="software_ready") {
                const auto actual=parse_cache(value.at("values"));
                if(incoming!=1||!equal(actual,initial)) throw std::runtime_error("software ready differs from initial native pair");
                const auto existing=peers.find(sender);
                if(existing!=peers.end()) {
                    if(existing->second.acknowledged!=1||existing->second.pending) throw std::runtime_error("software ready replay after updates");
                    return;
                }
                peers.emplace(sender,Peer{1,0,actual});
                LOG_NET_INFO("[SoftwareSync] Native initial pair acknowledged peer={} epoch={}",sender,epoch);
            } else if(type=="software_applied") {
                auto found=peers.find(sender);
                if(found==peers.end()) throw std::runtime_error("software acknowledgement before native ready");
                auto& peer=found->second;
                const auto actual=parse_cache(value.at("values"));
                if(!peer.pending&&incoming==peer.acknowledged&&equal(actual,peer.actual)) return;
                if(incoming!=peer.pending||!equal(actual,peer.sent)) throw std::runtime_error("software acknowledgement revision or native readback differs");
                peer.acknowledged=incoming; peer.actual=actual; peer.pending=0;
                if(peer.stalled) LOG_NET_INFO("[SoftwareSync] Application recovered peer={} revision={}",sender,incoming);
                peer.stalled=false;
                LOG_NET_INFO("[SoftwareSync] Native update acknowledged peer={} revision={}",sender,incoming);
            } else throw std::runtime_error("host requires software ready or applied acknowledgement");
        } else {
            if(sender!=1||type!="software_state") throw std::runtime_error("client software state must come from host");
            const auto before=parse_cache(value.at("before")), target=parse_cache(value.at("values"));
            const auto previous=integer(value,"previous_revision");
            if(incoming!=revision&&previous!=revision) throw std::runtime_error("software previous revision differs");
            if(incoming<revision||incoming==revision&&!equal(target,published)) throw std::runtime_error("software stale or conflicting state");
            game::native::SoftwareCacheReceipt receipt;
            std::string error;
            if(!game::native::apply_software_cache(epoch,incoming,before,target,receipt,error)) throw std::runtime_error(error);
            if(!equal(receipt.after,target)) throw std::runtime_error("software native update readback differs");
            if(receipt.written) ++applied_updates;
            revision=incoming; published=target;
            send({{"type","software_applied"},{"epoch",epoch},{"revision",revision},{"values",cache(receipt.after)}},1);
            LOG_NET_INFO("[SoftwareSync] Native state applied revision={} written={} rating={} fun={}",revision,receipt.written,target.rating,target.fun);
        }
    } catch(const std::exception& error) { fail(error.what()); }
}
}
void initialize_software_sync() { session_wire::register_handler(7,receive); }
void retire_software_sync() {
    if(epoch)LOG_NET_INFO("[SoftwareSync] Retired epoch={} after native lobby Leave",epoch);
    epoch=revision=applied_updates=0;initial={};published={};peers.clear();failure.clear();last_send={};
}
void begin_software_sync(uint64_t world_epoch,const Cache& source) {
    if(!world_epoch) throw std::runtime_error("software stream requires nonzero epoch");
    parse_cache(cache(source));
    epoch=world_epoch; revision=1; initial=published=source; applied_updates=0; peers.clear(); failure.clear(); last_send={};
    if(!Config::instance().is_host)
        send({{"type","software_ready"},{"epoch",epoch},{"revision",revision},{"values",cache(source)}},1);
}
void tick_software_sync() {
    try {
        if(!epoch||!failure.empty()||!session_wire::connected()||!Config::instance().is_host||!game::native::in_game()) return;
        game::native::SoftwareState actual;
        std::string error;
        if(!game::native::read_software_state(actual,error)) throw std::runtime_error(error);
        const Cache current{actual.rating,actual.fun};
        if(!equal(current,published)) {
            if(revision==std::numeric_limits<uint64_t>::max()) throw std::runtime_error("software revision exhausted");
            ++revision; published=current;
            LOG_NET_INFO("[SoftwareSync] Host state advanced revision={} rating={} fun={}",revision,current.rating,current.fun);
        }
        const auto now=Clock::now();
        if(now-last_send<std::chrono::seconds(1)) return;
        last_send=now;
        for(auto& [id,peer]:peers) {
            if(peer.pending) {
                if(!peer.stalled&&now-peer.dispatched>std::chrono::seconds(10)) {
                    peer.stalled=true;
                    LOG_NET_WARN("[SoftwareSync] Native application pending peer={} revision={} age_ms={}; no replay",id,peer.pending,std::chrono::duration_cast<std::chrono::milliseconds>(now-peer.dispatched).count());
                }
                continue;
            }
            if(peer.acknowledged==revision) continue;
            send({{"type","software_state"},{"epoch",epoch},{"revision",revision},{"previous_revision",peer.acknowledged},
                {"before",cache(peer.actual)},{"values",cache(published)}},id);
            peer.pending=revision; peer.sent=published; peer.dispatched=now;
        }
    } catch(const std::exception& error) { fail(error.what()); }
}
json software_sync_status() {
    json result={{"epoch",epoch},{"revision",revision},{"applied_updates",applied_updates},{"error",failure},{"published",cache(published)},{"peers",json::array()}};
    for(const auto& [id,peer]:peers) result["peers"].push_back({{"peer",id},{"acknowledged_revision",peer.acknowledged},{"pending_revision",peer.pending},{"stalled",peer.stalled}});
    return result;
}
}
