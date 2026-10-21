#include <memory>
namespace gizmo_state_protocol_fixture {
template<class Check>void run(Check& check){
    for(bool door:{false,true})for(int32_t command:{1,2,3}){if(!door&&command==2)continue;
        auto packet=std::make_unique<Message>();packet->session=5;packet->kind=Kind::HostResult;packet->revision=1;packet->required_mask=3;packet->outcome=Outcome::Confirmed;
        auto& req=packet->request;req.action=Action::EditDungeonGizmo;req.peer=2;req.sequence=1;req.action_uuid=action_id;req.snapshot_uuid=snapshot_id;
        auto& edit=req.dungeon_gizmo;edit={gizmo_key(),scene_key(),std::string(64,'a'),DungeonGizmoEdit::State,command,door};req.canonical_record_key=edit.key;
        auto& receipt=packet->evidence;receipt.before=receipt.after=money();receipt.retirement_confirmed=true;receipt.dungeon_gizmo_before=before();auto& initial=receipt.dungeon_gizmo_before;
        mt2mp::game::dungeon_gizmo_live::State live;
        for(auto& scene:initial.scenes){mt2mp::game::dungeon_gizmo_live::Scene dynamic;dynamic.key=scene.key;
            for(auto& row:scene.rows){row.value.state=0;row.value.triggered=false;mt2mp::game::dungeon_gizmo_live::Row current;current.key=row.key;
                for(auto& clip:current.clips)clip={uint32_t(0x40000000),uint32_t(0),false,uint32_t(0x3f800000)};dynamic.rows.push_back(current);}live.push_back(dynamic);}
        initial.live=std::move(live);initial.network=mt2mp::game::live_host_state::network::State{std::string(64,'b')};
        initial.network->nodes.push_back({7,0,{2,1,0,0},{0,0,0}});initial.network->managers=std::vector<mt2mp::game::live_host_state::network::ManagerRow>{{7,{0x3f800000,0x3f000000}}};
        check(dungeon_gizmo_project(edit,initial,receipt.dungeon_gizmo_after),"State original semantic projection");
        std::string error;std::vector<uint8_t> bytes;auto decoded=std::make_unique<Message>();
        check(dungeon_gizmo_wire_fits(req,initial,money(),error),"State complete pending Network/clip packet preflight: "+error);
        check(encode(*packet,bytes,error)&&decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==*packet,"State36 populated receipt roundtrip: "+error);
        auto wire=nlohmann::json::parse(bytes.begin(),bytes.end());check(wire.at("schema")==36,"new State uses36 without changing numeric35");
        wire["schema"]=35;auto text=wire.dump();check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),*decoded,error),"new State cannot enter old35 domain");
        const auto good=receipt.dungeon_gizmo_after;
        ++receipt.dungeon_gizmo_after.scenes[0].rows[0].value.gold;check(!encode(*packet,bytes,error),"State cannot alter protected contents");receipt.dungeon_gizmo_after=good;
        (*receipt.dungeon_gizmo_after.live)[0].rows[1].on_map=true;check(!encode(*packet,bytes,error),"State cannot alter another canonical Gizmo");receipt.dungeon_gizmo_after=good;
        receipt.dungeon_gizmo_after.network.reset();check(!encode(*packet,bytes,error),"State cannot omit complete Network effect witness");receipt.dungeon_gizmo_after=good;
        (*receipt.dungeon_gizmo_after.live)[0].unlocked^=0x40000000;check(!encode(*packet,bytes,error),"State cannot alter unrelated Shard lock bits");receipt.dungeon_gizmo_after=good;
        req.dungeon_gizmo.value=4;check(!encode(*packet,bytes,error),"State admits only original1/2/3 command values");
    }
}
}
