namespace root_saved_fixture {
template<class Check>void run(Check&& check){
    namespace keys=mt2mp::game::scenery_scene_key;namespace saved=mt2mp::game::root_scenery_saved;
    std::string error;const std::string sha(64,'a');
    using Witness=mt2mp::build_actions::private_native::place::SceneryWitness;
    Witness witness;witness.context={7,400,300,200,true,true,true,false};witness.counts[8]=2;witness.counts[19]=1;
    for(auto [category,object,uid]:{std::tuple{8,uintptr_t(100),uint64_t(500)},std::tuple{8,uintptr_t(101),uint64_t(501)},std::tuple{19,uintptr_t(102),uint64_t(502)}}){
        mt2mp::build_actions::private_native::place::SceneryMember member;member.category=category;member.object=object;member.uid=uid;witness.members.push_back(member);}
    std::vector<saved::Row> rows;
    check(saved::append(sha,59,witness,rows,error)&&rows.size()==3,"original47category root saved producer");
    check(rows[0].record==keys::root_record(sha,59,8,0)&&rows[1].record==keys::root_record(sha,59,8,1)&&rows[2].record==keys::root_record(sha,59,19,0),"category-local child order is identity");
    check(keys::root_saved(rows[0].record)&&keys::valid(rows[0].record),"root saved canonical grammar");
    for(const auto& bad:{keys::root_record(sha,59,-1,0),keys::root_record(sha,59,47,0),keys::root_record(sha,59,8,-1),rows[0].record+"x",keys::root_record(sha,-1,8,0)})check(!keys::valid(bad),"root saved rejects malformed owner/category/child");
    for(int mode=0;mode<6;++mode){auto changed=witness;std::vector<saved::Row> untouched=rows;
        if(mode==0)changed.counts[8]=1;if(mode==1)changed.members[1].uid=changed.members[0].uid;
        if(mode==2)changed.members[1].object=changed.members[0].object;if(mode==3)std::swap(changed.members[0],changed.members[2]);
        if(mode==4)changed.context.main_thread=false;if(mode==5)changed.members[0].category=47;
        error.clear();check(!saved::append(sha,59,changed,untouched,error)&&untouched.size()==rows.size()&&!error.empty(),"invalid original saved membership is atomic");}
    reset();auto current=census();scenery_scene_snapshot_alias::Manifest aliases;aliases.present=true;
    check(prepare_saved(current,aliases,error)&&saved_admission->aliases.root_present&&saved_admission->aliases.rows.size()==2,"legacy6 first root admission from exact native file");
    aliases.root_present=true;error.clear();check(!prepare_saved(current,aliases,error)&&error=="Scene Scenery companion omits or adds an original saved member","revision7 missing members cannot be bootstrapped");
    for(const auto& row:current.scenery_saved)aliases.rows.push_back({row.record,row.record});
    check(prepare_saved(current,aliases,error),"complete root companion native admission");
    auto changed=current;changed.scenery_saved[1].uid=changed.scenery_saved[0].uid;check(!prepare_saved(changed,aliases,error),"foreign duplicate local UID fails admission");
    changed=current;changed.scenery_saved[1].manager=0;check(!prepare_saved(changed,aliases,error),"missing native owner fails admission");
    changed=current;changed.snapshot_census=false;check(!prepare_saved(changed,aliases,error),"current-only census cannot seed saved identities");
    reset();scenery_scene_snapshot_alias::Manifest captured;
    check(capture_saved(current,captured,error)&&captured.root_present&&captured.rows.size()==2,"first host capture seeds complete original saved roots");
    const std::string canonical=keys::root_record(std::string(64,'c'),59,8,0),action="action:12345678-1234-4234-8234-123456789abc";
    registry.emplace("first",binding(current,0,canonical));registry.emplace("second",binding(current,1,action,true));
    Bound retired=binding(current,1,keys::root_record(std::string(64,'d'),59,8,9));retired.uid=999;retired.object=999;retired.revoked=true;registry.emplace("retired",retired);
    check(capture_saved(current,captured,error)&&captured.rows[0].key==canonical&&captured.rows[1].key==action&&captured.retired.contains(retired.key),"resnapshot retains old canonical aliases/action aliases and retirement");
    changed=current;changed.scenery_saved.pop_back();check(!capture_saved(changed,captured,error)&&error=="Scene Scenery snapshot omits a current confirmed member","resnapshot cannot silently omit live canonical member");
    changed=current;changed.scenery_saved[1].uid=700;changed.scenery_saved[1].object=701;
    check(!capture_saved(changed,captured,error)&&error=="Scene Scenery snapshot member lacks its exact current alias/owner","initialized snapshot cannot invent a replacement birth");
    registry.emplace("duplicate",binding(current,0,action));check(!capture_saved(current,captured,error)&&error=="Scene Scenery snapshot full UID aliases multiple records","duplicate current aliases fail before snapshot publication");
    reset();check(prepare_saved(current,aliases,error),"root prepare before zero member authority");saved_admission->consumed=true;registry.clear();
    check(!capture_saved(current,captured,error),"confirmed root authority never silently reseeds omitted aliases");
    reset();current=census();fixture_world=current.world;fixture_members=current.scenery_saved;
    aliases={};aliases.present=true;aliases.root_present=true;
    for(size_t i=0;i<current.scenery_saved.size();++i)aliases.rows.push_back({current.scenery_saved[i].record,i==0?canonical:action});
    aliases.retired.insert(keys::root_record(std::string(64,'e'),59,8,9));
    check(prepare_saved(current,aliases,error),"empty-Building world complete root prepare");
    auto old=binding(current,0,canonical);old.snapshot="22222222-2222-4222-8222-222222222222";registry.emplace("old-snapshot",old);
    fixture_world.uuid="22222222-2222-4222-8222-222222222222";fixture_world.sha256=std::string(64,'f');
    json list=json::array();error.clear();check(append_saved(list,error)&&list.size()==2&&saved_admission->consumed,"actual root append admits native aliases without any Building anchor");
    check(registry.at("old-snapshot").revoked,"old same-world snapshot alias cannot compete with current full UID");
    const auto new_id=registry_id(net::current_snapshot_id(),canonical);
    check(registry.at(new_id).scene_owner=="world"&&registry.at(new_id).uid==500&&!registry.at(new_id).revoked,"strict current consumers receive complete root canonical native UID");
    check(list[0].at("record_key")==canonical&&list[0].at("native_record_key")==current.scenery_saved[0].record&&list[1].at("record_key")==action&&list[1].at("native_record_key")==current.scenery_saved[1].record,"canonical identities retain distinct current native snapshot records");
    check(!registered(net::current_snapshot_id(),canonical,500),"Root Scenery remains owned by ordinary World table rather than Dungeon bypass");
    const auto tombstone=registry_id(net::current_snapshot_id(),*aliases.retired.begin());
    check(registry.at(tombstone).revoked&&registry.at(tombstone).scene_owner=="retired"&&registry.at(tombstone).uid==0,"resnapshot root retirement key retains no process UID on wire");
    check(append_saved(list,error)&&list.size()==2,"actual root saved admission is consumed once");
    for(const std::string failure:{"world","owner","prop","final"}){
        reset();fixture_world=current.world;fixture_members=current.scenery_saved;check(prepare_saved(current,aliases,error),"root failure preparation");
        fixture_read_failure=failure;list=json::array();error.clear();
        check(!append_saved(list,error)&&list.empty()&&registry.empty()&&!saved_admission->consumed&&error.starts_with("original"),"actual admission preserves upstream cause without partial registry publication");
    }
    reset();
}
}
