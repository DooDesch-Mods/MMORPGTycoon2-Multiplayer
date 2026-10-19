#include "coop_cursor/core.h"
#include "coop_cursor/runtime.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mt2mp::game::coop_cursor;
unsigned checks=0;
void require(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
int main()try{
    const Context host{42,1,3,"11111111-1111-4111-8111-111111111111","world"};
    auto client=host;client.local_peer=2;
    Core a,b;a.configure(host);b.configure(client);
    const Sample position{std::array<float,3>{8020,70.5f,8550}};
    Packet packet,decoded;std::string error;
    require(a.publish(position,packet,error),"native world sample not published");
    auto text=encode(packet).dump();
    require(decode(reinterpret_cast<const uint8_t*>(text.data()),uint32_t(text.size()),decoded,error),"actual packet roundtrip failed");
    require(decoded.sample==position&&decoded.session==42&&decoded.sequence==1,"packet changed world position or identity");
    require(b.receive(1,decoded,100,error)==Reception::Applied,"physical Host update rejected");
    require(b.visible(1,101)==position.position,"world position absent");
    require(b.receive(1,decoded,200,error)==Reception::Stale,"duplicate update reapplied");
    require(b.visible(1,1600)==std::nullopt,"stale cursor retained");
    require(b.receive(2,decoded,200,error)==Reception::Rejected,"self sender admitted");
    require(b.receive(3,decoded,200,error)==Reception::Rejected,"absent sender admitted");
    auto other=decoded;other.session=43;
    require(b.receive(1,other,200,error)==Reception::OtherWorld,"foreign session admitted");
    other=decoded;other.snapshot="22222222-2222-4222-8222-222222222222";
    require(b.receive(1,other,200,error)==Reception::OtherWorld,"foreign snapshot admitted");
    require(a.publish({},packet,error)&&b.receive(1,packet,300,error)==Reception::Applied&&!b.visible(1,301),"hide was not applied");
    auto dungeon=host;dungeon.scene="dungeon:37";a.configure(dungeon);
    require(a.publish(position,packet,error)&&b.receive(1,packet,400,error)==Reception::Applied&&!b.visible(1,401),"cursor leaked into another scene");
    client.scene=dungeon.scene;b.configure(client);
    require(b.visible(1,401)==position.position,"matching scene did not expose current cursor");
    client.peer_mask=2;b.configure(client);
    require(!b.visible(1,401),"invalid participant context retained cursor");
    client.peer_mask=3;b.configure(client);
    require(!b.visible(1,401),"retired context resurrected old cursor");
    require(b.receive(1,packet,402,error)==Reception::Stale,"temporary context loss forgot the receive sequence");
    const auto previous_sequence=packet.sequence;
    a.configure({});a.configure(host);
    require(a.publish(position,packet,error)&&packet.sequence>previous_sequence,"temporary native unavailability reset the outbound sequence");
    Sample invalid{std::array<float,3>{0,std::numeric_limits<float>::infinity(),0}};
    require(!a.publish(invalid,packet,error),"nonfinite native sample published");
    const auto rejects=[&](nlohmann::json value){const auto raw=value.dump();return !decode(reinterpret_cast<const uint8_t*>(raw.data()),uint32_t(raw.size()),decoded,error);};
    auto value=encode(packet);value["revision"]=2;require(rejects(value),"unknown revision admitted");
    value=encode(packet);value["position"]={1,2};require(rejects(value),"screen pair admitted as world point");
    value=encode(packet);value["position"]={1,2,true};require(rejects(value),"boolean position admitted");
    value=encode(packet);value["position"]={1,2,100000001};require(rejects(value),"oversize position admitted");
    value=encode(packet);value["sequence"]=-1;require(rejects(value),"negative sequence admitted");
    value=encode(packet);value["sender"]=1;require(rejects(value),"payload sender field admitted");
    require(!decode(nullptr,0,decoded,error),"empty packet admitted");
    Context active=client;active.scene="world";
    unsigned sends=0,shows=0,hides=0;bool read_ok=true,hide_ok=true;
    Runtime runtime({[&](Context& c,std::string&){c=active;return true;},
        [&](Sample& sample,std::string& e){sample=position;if(!read_ok)e="native cursor unavailable";return read_ok;},
        [&](uint8_t ch,const uint8_t*,uint32_t,std::string&){require(ch==13,"wrong cursor channel");++sends;return true;},
        [&](uint64_t peer,const std::array<float,3>& point,std::string&){require(peer==1&&point==*position.position,"marker changed physical peer/world position");++shows;return true;},
        [&](uint64_t,std::string& e){++hides;if(!hide_ok)e="native marker still owned";return hide_ok;}});
    require(runtime.tick(1000)&&runtime.tick(1050)&&sends==1,"cursor publication was not bounded to10Hz");
    a.configure(host);require(a.publish(position,packet,error),"fresh Host packet absent");text=encode(packet).dump();
    require(runtime.receive(1,reinterpret_cast<const uint8_t*>(text.data()),uint32_t(text.size()),1050)==Reception::Applied,"runtime rejected actual packet");
    require(runtime.tick(1060)&&shows==1&&runtime.tick(1070)&&shows==1,"unchanged marker was submitted repeatedly");
    active.scene="dungeon:37";auto entered=host;entered.scene=active.scene;a.configure(entered);
    require(a.publish(position,packet,error),"scene entry sample not published");text=encode(packet).dump();
    require(runtime.receive(1,reinterpret_cast<const uint8_t*>(text.data()),uint32_t(text.size()),1071)==Reception::Applied,"scene entry packet not received");
    require(runtime.tick(1072)&&shows==2,"native scene change did not resubmit an unchanged marker after provider scene clearing");
    hide_ok=false;require(!runtime.retire(1080)&&runtime.status().display_failed==1,"failed native hide reported success");
    hide_ok=true;require(runtime.retire(1090)&&hides==2,"pending native marker cleanup was lost");
    read_ok=false;require(!runtime.tick(1200)&&runtime.status().read_failed==1&&runtime.status().cause.find("native cursor unavailable")!=std::string::npos,"native read failure was suppressed");
    std::cout<<checks<<" co-op cursor protocol/lifetime checks passed\n";return 0;
}catch(const std::exception& cause){std::cerr<<cause.what()<<'\n';return 1;}
