#pragma once
#include "native_fixture.h"
#include "../../src/game/time_session.h"
#include <vector>
#include <memory>
namespace time_mouse_tests {
namespace wire=mt2mp::game::time_session;
struct ControllerPeer {
    NativeReadFixture memory;
    std::unique_ptr<wire::Controller> controller_owner=std::make_unique<wire::Controller>();
    wire::Controller& controller=*controller_owner;
    bool host=false,read_fails=false,set_unknown=false;
    uint64_t epoch=71;int native_sets=0,presentations=0,submissions=0;
    static uint32_t thread(void* user){return static_cast<ControllerPeer*>(user)->memory.thread;}
    static bool read_clock(void* user,uint64_t epoch,wire::Clock& out,std::string& cause){auto& self=*static_cast<ControllerPeer*>(user);
        if(epoch!=self.epoch){cause="fixture actual epoch differs";return false;}
        if(self.read_fails){cause="fixture original host Clock read failure";return false;}
        out={self.memory.get<float>(clock+0x58),self.memory.get<float>(clock+0x5c)};return true;}
    static wire::Effect set_clock(void* user,uint64_t epoch,float scale,std::string& cause){auto& self=*static_cast<ControllerPeer*>(user);
        if(!self.host||epoch!=self.epoch){cause="fixture host Clock authority absent";return wire::Effect::RejectedNoEffect;}
        ++self.native_sets;if(self.set_unknown){cause="fixture original native setter unknown";return wire::Effect::Unknown;}
        self.memory.put(clock+0x58,scale);return wire::Effect::Confirmed;}
    static wire::Effect apply(void* user,uint64_t epoch,const wire::Snapshot& state,wire::PresentationReadback& out,std::string& cause){auto& self=*static_cast<ControllerPeer*>(user);
        if(self.host||epoch!=self.epoch){cause="fixture client presentation binding differs";return wire::Effect::RejectedNoEffect;}
        ++self.presentations;out={state.revision,state.clock,self.memory.get<float>(clock+0x58),true};return wire::Effect::Confirmed;}
    explicit ControllerPeer(bool is_host):host(is_host){auto result=controller.start({host?wire::Role::Host:wire::Role::Client,epoch,uint8_t(host?1:2),3,17},{this,thread,read_clock,set_clock,apply});
        if(!result.okay())throw std::runtime_error("fixture actual Controller start: "+result.cause);}
    wire::Report submit(wire::Intent intent){++submissions;return controller.submit(intent);}
};
inline wire::Message serialized(const wire::Message& source){std::array<uint8_t,64> packet{};std::string cause;wire::Message out;
    if(!wire::encode(source,packet,cause)||!wire::decode(packet.data(),packet.size(),out,cause))throw std::runtime_error("fixture actual packet serialization: "+cause);return out;}
inline std::vector<wire::Outbound> drain(ControllerPeer& peer){std::vector<wire::Outbound> out;
    for(size_t count=0;count<=wire::outbox_size;++count){wire::Outbound packet;bool available=false;auto report=peer.controller.pop(packet,available);
        if(!report.okay())throw std::runtime_error("fixture actual Controller pop: "+report.cause);if(!available)return out;packet.message=serialized(packet.message);out.push_back(packet);}
    throw std::runtime_error("fixture actual Controller outbox exceeds bound");}
inline void initial_host_state(ControllerPeer& host,ControllerPeer& client){auto report=host.controller.poll_host();if(!report.okay())throw std::runtime_error(report.cause);
    for(const auto& packet:drain(host)){if(packet.message.kind!=wire::Kind::State)throw std::runtime_error("fixture initial Host packet is not State");auto received=client.controller.receive(1,packet.message);if(!received.okay())throw std::runtime_error(received.cause);}
    for(const auto& packet:drain(client)){auto received=host.controller.receive(2,packet.message);if(!received.okay())throw std::runtime_error(received.cause);}}
}
