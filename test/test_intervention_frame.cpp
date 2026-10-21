#include "live_host_state/frame_fragments.h"
#include "live_host_state/frame_apply_acks.h"
#include "live_host_state/day_rollover/economy/codec.h"
#include "intervention_actor_lifecycle.h"
#include "intervention_actor_adopt.h"
#include "mt2mp/core/hash.h"
#include <iostream>
#include <stdexcept>
namespace live=mt2mp::game::live_host_state;namespace economy=live::day::economy;
namespace staff=mt2mp::game::developer_staff;namespace bug=mt2mp::game::bug_authority;namespace quest=mt2mp::subscriber_quest_state;
namespace actors=mt2mp::game::interventions::actors;namespace intervention=mt2mp::game::interventions;namespace gm=mt2mp::game::gm_ticket;
using Json=nlohmann::json;
namespace {
unsigned checks=0;const std::string digest(64,'a');
void need(bool okay,const std::string& cause){++checks;if(!okay)throw std::runtime_error(cause);}
#include "frame_fragment_fixture.inc"
actors::Row actor(int32_t id){
    actors::Row out;out.target={intervention::Family::Boss,digest+":world:boss:"+std::to_string(id)};out.slot=1;out.type=2;out.level=3;out.pose.region=67;out.pose.orientation={0,0,0,1};out.pose.position={1,2,3};out.pose.scale={1,1,1};
    out.values.fill(std::bit_cast<uint32_t>(100.0f));out.maxima=out.values;out.action=46;out.lifecycle=actors::Boss{3,3,std::bit_cast<uint32_t>(600.0f),{1,2,3}};
    out.clips[46]={std::bit_cast<uint32_t>(1.0f),std::bit_cast<uint32_t>(-0.25f),true,std::bit_cast<uint32_t>(0.5f)};
    for(size_t n=0;n<4;++n)out.status_effects[n]={int32_t(n),int32_t(n+2),std::bit_cast<uint32_t>(1.0f),std::bit_cast<uint32_t>(2.0f),std::bit_cast<uint32_t>(3.0f),intervention::Target{intervention::Family::Toon,"subscriber:17"},"character:1:2/ability/0"};
    out.abilities={{"character:1:2/ability/0",2,std::bit_cast<uint32_t>(3.5f)}};return out;
}
actors::Row dungeon_actor(){
    auto row=actor(1);row.target={intervention::Family::Monster,"action:70000000-0000-4000-8000-000000000001"};
    row.dungeon_scene="action:70000000-0000-4000-8000-000000000002/instance/0";
    row.lifecycle=actors::ZoneMonster{3,2,true,std::bit_cast<uint32_t>(30.0f),{1,2,3}};return row;
}
economy::State populated(){auto out=fixture();auto& management=out.day.frame.management.emplace();gm::Publication publication;publication.queue.kind=mt2mp::game::release_graph::Kind::GameMasterTickets;publication.queue.records.push_back({SIZE_MAX,0,{1,0,"vsObjectArray<mmoTicket>"},{}});management.gm=publication;management.policies.emplace();management.rules.emplace();management.rules->respawn_seconds=60;
    management.subscriber_interventions=mt2mp::game::subscriber_intervention::State{{17,{2,std::bit_cast<uint32_t>(60.0f),true,true,uint32_t(3)}}};management.intervention_actors=actors::State{actor(1),dungeon_actor()};
    std::sort(management.intervention_actors->begin(),management.intervention_actors->end(),[](const auto& a,const auto& b){return a.target.key<b.target.key;});
    actors::lifecycle::Host journal;std::string error;const auto& frame=out.day.frame;const actors::lifecycle::journal::Origin origin{frame.session,frame.epoch,frame.source_generation,frame.snapshot};need(journal.bind(origin,2,error),error);
    need(actors::lifecycle::append(journal,origin,{actor(1).target,actors::lifecycle::Effect::CharacterDie,actors::lifecycle::Source::BossBaseDeath},error),error);
    need(actors::lifecycle::append(journal,origin,{actor(1).target,actors::lifecycle::Effect::OnSpawn,actors::lifecycle::Source::BossWait},error),error);
    need(actors::lifecycle::append(journal,origin,{actor(1).target,actors::lifecycle::Effect::OnSpawn,actors::lifecycle::Source::BossRespawn},error),error);management.actor_lifecycle=journal.pending();return out;
}
int depth(const std::vector<uint8_t>& bytes){int result=0;const auto value=Json::parse(bytes,[&](int n,Json::parse_event_t,Json&){result=std::max(result,n);return true;});need(value.is_object(),"Frame24 encoded JSON root is not an object");return result;}
}
namespace actual_route {
using namespace live;
namespace action_session=mt2mp::action_session;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
struct Delegate{actors::lifecycle::Host* host=nullptr;actors::lifecycle::journal::Origin origin;unsigned calls=0;bool acknowledge(uint64_t peer,uint64_t frame,std::string& error){++calls;if(!host)return fail(error,"Frame24 ACK actual journal owner absent");return host->acknowledge(origin,uint32_t(peer),frame,error);}};
struct Economy{unsigned calls=0;bool acknowledge_subscribers(uint8_t,uint8_t,uint64_t,const Json&,std::string& error){++calls;error.clear();return true;}};
struct Bridge{
    bool suspended_=false,locked_=false,day_=false;Context context_;mt2mp::action_session::Binding binding_;uint64_t common_epoch_=0,source_generation_=0,host_revision_=0;fragments::Assembly fragments_;FrameApplyAcks apply_acks_;Json received_frame_transport_;
    Economy provider;Economy* economy_=&provider;Delegate* marketing_port_=nullptr;Delegate* management_port_=nullptr;Delegate* party_port_=nullptr;unsigned received=0;std::vector<uint8_t> received_bytes;economy::State state;
    explicit Bridge(const Frame& frame):context_{frame.session,2,1,3,mt2mp::action_session::Role::Client,frame.snapshot},binding_{1,frame.source_generation},common_epoch_(frame.epoch),source_generation_(frame.source_generation),host_revision_(frame.revision){}
    bool acknowledge_vips(uint64_t,uint64_t,std::string& error){error.clear();return true;}
    bool receive(uint64_t sender,const uint8_t* packet,size_t size,std::string& error){if(sender!=1)return fail(error,"Frame24 actual receiver rejects non-Host source");if(!economy::decode(packet,size,state,error))return false;++received;received_bytes.assign(packet,packet+size);return true;}
    bool route_receive(uint64_t,const uint8_t*,size_t,bool&,std::string&);
};
#include "frame_fragment_route.inc"
}
namespace {
void routes(bool control){auto value=populated();auto& frame=value.day.frame;
    if(control){intervention::ControlProof state;state.peer=2;state.lease="20000000-0000-4000-8000-000000000002";state.controlled=true;state.moving=true;state.sequence=123;state.jump=4;state.selected=intervention::Target{intervention::Family::Toon,"subscriber:17"};frame.control=mt2mp::control_input::ReplicaState{{actor(1).target,state}};
        auto& party=frame.party.emplace();const std::string exterior="action:20000000-0000-4000-8000-000000000004";
        const auto scene=exterior+"/instance/0";party.pools.push_back({exterior,1,{0}});
        mt2mp::game::dungeon_instance_live::State instance;instance.scene=scene;instance.exterior=exterior;instance.index=0;party.instances.push_back(instance);
        auto& mapping=party.scenes.emplace();mapping.operation="20000000-0000-4000-8000-000000000005";mapping.scenes.push_back({scene,1,0,0});
        const auto key=mt2mp::game::scene_clone::birth_key(mapping.operation,mt2mp::game::scene_clone::Family::Gizmo,0);
        mapping.members.push_back({scene,key,mt2mp::game::scene_clone::Family::Gizmo,0,0,mapping.operation});
        mapping.scenes[0].buildings=1;
        const auto building_key=mt2mp::game::scene_clone::birth_key(mapping.operation,mt2mp::game::scene_clone::Family::Building,1);
        mapping.members.push_back({scene,building_key,mt2mp::game::scene_clone::Family::Building,0,1,mapping.operation});
        mapping.scenes[0].scenery=1;
        const auto scenery_key=mt2mp::game::scene_clone::birth_key(mapping.operation,mt2mp::game::scene_clone::Family::Scenery,2);
        mapping.members.push_back({mapping.scenes[0].scene,scenery_key,mt2mp::game::scene_clone::Family::Scenery,0,2,mapping.operation});
    }std::string error;std::vector<uint8_t> bytes;live::Frame decoded;live::day::Target day;economy::State decoded_economy;
    need(live::encode(frame,bytes,error)&&live::decode(bytes.data(),bytes.size(),decoded,error)&&decoded==frame,error);const auto frame_depth=depth(bytes);need(Json::parse(bytes).at("schema")== (control?26:24),"complete Actor/Subscriber/lifecycle Frame must select24");need(frame_depth<=live::max_frame_json_depth,"populated Actor original status/user/Item Frame grammar exceeds existing proven budget");
    need(live::day::encode(value.day,bytes,error)&&live::day::decode(bytes.data(),bytes.size(),day,error)&&day==value.day,error);const auto day_depth=depth(bytes);need(day_depth==frame_depth+1,"Actor24 Day nesting differs from actual wrapper");
    need(economy::encode(value,bytes,error)&&economy::decode(bytes.data(),bytes.size(),decoded_economy,error)&&decoded_economy==value,error);const auto economy_depth=depth(bytes);need(economy_depth==day_depth+1&&economy_depth<=live::max_economy_json_depth,"Actor24 full Economy exceeds actual wrapper depth budget");
    actual_route::Bridge direct(frame);bool handled=false;need(bytes.size()<=live::max_frame_payload&&direct.route_receive(1,bytes.data(),bytes.size(),handled,error)&&handled&&direct.received==1&&direct.received_bytes==bytes&&direct.state==value,"populated Actor24 direct actual Channel9 route: "+error);
    if(control){auto bad=Json::parse(bytes);bad["day"]["frame"]["control"][0]["state"]["peer"]=0;const auto text=bad.dump();need(!economy::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded_economy,error),"Control Frame ownerless controlled actor admitted");}
    auto wire=Json::parse(bytes);for(const char* key:{"intervention_actors","subscriber_interventions","actor_lifecycle"}){auto bad=wire;bad["day"]["frame"]["management"].erase(key);const auto text=bad.dump();need(!economy::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded_economy,error),std::string("partial24 accepted missing=")+key);}
    for(int32_t id=2;id<=90;++id)frame.management->intervention_actors->push_back(actor(id));std::sort(frame.management->intervention_actors->begin(),frame.management->intervention_actors->end(),[](const auto& a,const auto& b){return a.target.key<b.target.key;});
    live::fragments::Packets packets;need(economy::encode(value,bytes,error)&&live::fragments::encode(frame,bytes,mt2mp::sha256,packets,error)&&packets.size()>1,"populated Actor24 fragmented setup: "+error);
    actual_route::Bridge fragmented(frame);for(size_t n=0;n<packets.size();++n){const auto& packet=packets[n];need(fragmented.route_receive(1,packet.data(),packet.size(),handled,error)&&handled,"populated Actor24 fragmented actual Channel9 route: "+error);need(fragmented.received==(n+1==packets.size()?1u:0u),"partial Actor24 lifecycle reached native receiver");}
    need(fragmented.received_bytes==bytes&&fragmented.state==value,"Actor24 fragmented bytes/full original fields changed");need(depth(bytes)==economy_depth,"Actor24 roster extent unexpectedly changes grammar depth");
    auto malformed=Json::parse(bytes);malformed["day"]["frame"]["management"]["intervention_actors"][0]["status_effects"][0][5]["local_uid"]=1;const auto text=malformed.dump();need(!economy::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded_economy,error),"Actor24 raw peer UID bypassed nested strict owner codec");
    std::cout<<"Actor24 populated depths="<<frame_depth<<'/'<<day_depth<<'/'<<economy_depth<<" fragmented_bytes="<<bytes.size()<<" chunks="<<packets.size()<<'\n';
}
void dungeon_projection(){
    const auto value=populated();std::vector<uint8_t> bytes;std::string error;economy::State decoded;
    need(economy::encode(value,bytes,error)&&economy::decode(bytes.data(),bytes.size(),decoded,error),error);
    const auto& target=*decoded.day.frame.management->intervention_actors;
    const auto direct=std::find_if(target.begin(),target.end(),[](const auto& row){return !row.dungeon_scene.empty();});
    need(direct!=target.end()&&*direct==dungeon_actor(),"full Economy/Frame lost populated direct Dungeon actor fields");
    struct Receipt{bool attempted=false,unknown=false;};
    struct Backend{
        actors::State current;unsigned transforms=0;bool fail_after=false;
        bool sample(actors::State& out,std::string&){out=current;return true;}
        bool begin(const actors::State&,const actors::State&,Receipt&,std::string&){return true;}
        bool lifecycle(const actors::Row&,const actors::Row&,Receipt&,std::string&){return true;}
        bool ready(const actors::Row&,const actors::Row&,bool,Receipt&,std::string& e){e="unexpected ready change in Dungeon motion projection";return false;}
        bool on_map(const actors::Row&,const actors::Row&,bool,Receipt&,std::string& e){e="unexpected map change in Dungeon motion projection";return false;}
        bool transform(const actors::Row& row,const actors::Row& wanted,Receipt& receipt,std::string&){
            const auto found=std::find_if(current.begin(),current.end(),[&](const auto& value){return value.target==row.target;});
            if(found==current.end())return false;receipt.attempted=true;++transforms;found->pose=wanted.pose;return true;
        }
        bool scalars(const actors::Row& row,const actors::Row& wanted,Receipt&,std::string& e){
            auto copy=row;copy.pose=wanted.pose;if(copy!=wanted){e="unexpected non-motion field changed in Dungeon projection";return false;}return true;
        }
        bool animation(const actors::Row&,const actors::Row&,size_t,Receipt&,std::string& e){e="unexpected clip change in Dungeon motion projection";return false;}
        bool present(const actors::Row& row,const actors::Row&,Receipt&,std::string& e){if(fail_after&&!row.dungeon_scene.empty()){e="fixture original presentation readback failed";return false;}return true;}
        bool finish(const actors::State&,const actors::State& after,Receipt&,std::string&){return after==current;}
    } backend;backend.current=target;
    const auto slot=size_t(direct-target.begin());backend.current[slot].pose.position={8,9,10};backend.current[slot].pose.orientation={0,1,0,0};
    Receipt receipt;need(actors::adopt(backend,target,receipt,error)&&backend.current==target&&backend.transforms==1&&receipt.attempted&&!receipt.unknown,
        "decoded full Frame did not apply exact direct Dungeon pose through existing Actor adopter: "+error);
    backend.current=target;backend.current[slot].pose.position={8,9,10};backend.fail_after=true;receipt={};
    need(!actors::adopt(backend,target,receipt,error)&&receipt.attempted&&receipt.unknown&&error=="fixture original presentation readback failed",
        "direct Dungeon frame partial original effect reported rendered success or lost cause");
    auto bad=Json::parse(bytes);auto& rows=bad["day"]["frame"]["management"]["intervention_actors"];
    rows[slot].erase("dungeon_scene");const auto missing=bad.dump();
    need(!economy::decode(reinterpret_cast<const uint8_t*>(missing.data()),missing.size(),decoded,error),"full Frame admitted direct Monster without explicit current Dungeon scope");
}
void actual_ack(){const auto value=populated();const auto& frame=value.day.frame;std::string error;actors::lifecycle::Host journal;const actors::lifecycle::journal::Origin origin{frame.session,frame.epoch,frame.source_generation,frame.snapshot};need(journal.bind(origin,2,error),error);
    for(const auto& row:frame.management->actor_lifecycle->rows)need(actors::lifecycle::append(journal,origin,actors::lifecycle::event(row),error),error);
    need(journal.sent(origin,frame.revision,journal.pending(),error),error);const auto prior=journal.pending();
    actual_route::Delegate management;management.host=&journal;management.origin=origin;actual_route::Bridge host(frame);host.context_.role=mt2mp::action_session::Role::Host;host.context_.local_peer=1;host.management_port_=&management;host.apply_acks_.sent(frame.revision,2,std::chrono::steady_clock::now());
    auto ack=Json{{"kind","host_state_subscriber_ack"},{"schema",1},{"session",frame.session},{"epoch",frame.epoch},{"snapshot_uuid",frame.snapshot},{"source_generation",frame.source_generation},{"frame_revision",frame.revision},{"rows",Json::array()}};
    const auto text=ack.dump();bool handled=false;need(host.route_receive(2,reinterpret_cast<const uint8_t*>(text.data()),text.size(),handled,error)&&handled,error);
    need(journal.pending().after==prior.rows.back().sequence&&journal.pending().rows.empty()&&management.calls==1&&host.provider.calls==1&&host.apply_acks_.pending()==0,"actual whole Frame ACK did not cover exact original Actor lifecycle invocations");
    need(host.route_receive(2,reinterpret_cast<const uint8_t*>(text.data()),text.size(),handled,error)&&journal.pending().rows.empty(),"repeated actual Frame ACK reintroduced a native lifecycle event");
    ack["frame_revision"]=frame.revision+1;const auto future=ack.dump();need(!host.route_receive(2,reinterpret_cast<const uint8_t*>(future.data()),future.size(),handled,error)&&journal.pending().rows.empty(),"unsent Actor24 Frame ACK retired unknown original effects");
}
}
int main()try{routes(false);routes(true);dungeon_projection();actual_ack();std::cout<<checks<<" populated Actor24 original status/Items/lifecycle direct and fragmented route/ACK assertions passed\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
