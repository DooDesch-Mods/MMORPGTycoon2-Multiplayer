namespace gizmo_rename_protocol_fixture {
namespace a=mt2mp::action_session;
template<class Check>void run(Check& check){std::string error;std::vector<uint8_t> bytes;
    auto message=std::make_unique<a::Message>(sample(a::PropTextKind::Name));
    const std::string scene="action:33333333-3333-4333-8333-333333333333/template";
    const std::string target="action:44444444-4444-4444-8444-444444444444:dungeon:gizmo:0";
    message->request.canonical_record_key=target;message->request.prop_text.gizmo_scene=scene;
    a::PropTextState before;before.gizmos.emplace();before.gizmos->protected_sha256=std::string(64,'a');before.gizmos->global_random={1,2,3,4};
    a::DungeonGizmoRecord first;first.name="Old name";first.type="container";first.variant="base";first.rotation={0,0,0,1};first.scale={1,1,1};first.position={1,2,3};first.gold=3;
    a::DungeonGizmoRecord other=first;other.name="Untouched chest";
    before.gizmos->scenes={{scene,{{target,first},{"action:44444444-4444-4444-8444-444444444444:dungeon:gizmo:1",other}}}};
    auto after=before;after.gizmos->scenes[0].rows[0].value.name=message->request.prop_text.text;
    message->evidence.prop_text_before=before;message->evidence.prop_text_after=after;
    check(a::encode(*message,bytes,error),error);auto decoded=std::make_unique<a::Message>();
    check(a::decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==*message,error);
    const auto document=nlohmann::json::parse(bytes);check(document.at("schema")==36,"Gizmo Rename requires exact schema36");
    auto request=std::make_unique<a::Message>(*message);request->kind=a::Kind::Request;request->revision=request->required_mask=0;request->outcome=a::Outcome::Pending;request->evidence={};
    check(a::encode(*request,bytes,error)&&a::decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==*request,error);
    const auto rejected=[&](auto change){auto bad=std::make_unique<a::Message>(*message);change(*bad);return !a::encode(*bad,bytes,error);};
    check(rejected([](auto& bad){bad.evidence.prop_text_after->gizmos->scenes[0].rows[1].value.name="foreign";}),"Rename accepted another Gizmo name change");
    check(rejected([](auto& bad){++bad.evidence.prop_text_after->gizmos->scenes[0].rows[0].value.gold;}),"Rename accepted changed inventory");
    check(rejected([](auto& bad){bad.evidence.prop_text_after->gizmos->scenes[0].rows.pop_back();}),"Rename accepted incomplete scene membership");
    check(rejected([](auto& bad){bad.evidence.prop_text_after->gizmos->global_random[0]++;}),"Rename accepted unrelated RNG change");
    check(rejected([](auto& bad){bad.evidence.undo_confirmed=true;}),"Rename invented original Undo support");
    check(rejected([](auto& bad){bad.request.prop_text.gizmo_scene="action:55555555-5555-4555-8555-555555555555/template";}),"Rename accepted foreign scene");
    check(rejected([](auto& bad){bad.evidence.prop_text_after->object.alive=true;}),"Rename accepted fake Building owner");
    check(rejected([](auto& bad){bad.request.prop_text.kind=a::PropTextKind::Description;}),"Gizmo accepted absent Description command");
    check(rejected([](auto& bad){bad.evidence.prop_text_after->gizmos.reset();}),"Rename accepted missing full current result");
    const auto rejects_json=[&](nlohmann::json bad){const auto wire=bad.dump();return !a::decode(reinterpret_cast<const uint8_t*>(wire.data()),wire.size(),*decoded,error);};
    auto bad=document;bad["schema"]=35;check(rejects_json(bad),"Gizmo Rename admitted on old Building/Scenery schema35");
    bad=document;bad["request"]["prop_text"].erase("gizmo_scene");check(rejects_json(bad),"schema36 Rename omitted current scene");
    bad=document;bad["evidence"]["prop_text_after"]["foreign"]=true;check(rejects_json(bad),"Rename accepted an unknown proof field");
    auto ordinary=std::make_unique<a::Message>(sample(a::PropTextKind::Name));check(a::encode(*ordinary,bytes,error),error);
    check(nlohmann::json::parse(bytes).at("schema")==35,"Existing ordinary PropText schema35 changed");
    bad=nlohmann::json::parse(bytes);bad["schema"]=36;check(rejects_json(bad),"ordinary PropText admitted as Gizmo schema36 without its fields");
}
}
