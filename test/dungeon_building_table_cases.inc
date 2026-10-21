unsigned dungeon_building_table_cases(){
    namespace table=mt2mp::world_state;
    const std::string operation="11111111-1111-4111-8111-111111111111";
    const table::Origin origin{"22222222-2222-4222-8222-222222222222",std::string(64,'a'),3,4};
    const std::string exterior="action:"+operation;
    const std::string child="action:33333333-3333-4333-8333-333333333333";
    const std::string other="action:44444444-4444-4444-8444-444444444444";
    table::Building building;building.type="DungeonExterior";building.variant="Original";building.alive=true;
    std::vector<table::LocalBinding> created{
        {origin,exterior,0x100000055ULL,building},{origin,child,0x200000055ULL,building}};
    created[1].state.type="SmallInn";
    std::string error;unsigned checks=0;
    table::Table world;
    need(world.bind_snapshot(origin,{{other,origin.sha256+":world:building:1",created[1].state}},0,error),error);
    const auto original=world.rows();
    auto reject_birth=[&](auto change){
        auto rows=created;change(rows);
        need(world.commit_dungeon_census(origin,1,operation,exterior,{},rows,error)==table::Admission::Rejected&&
             world.rows()==original&&world.revision()==0,"invalid complete Dungeon birth partly published");
        ++checks;
    };
    reject_birth([](auto& rows){rows[1].local_uid=rows[0].local_uid;});
    reject_birth([](auto& rows){rows[1].local_uid=0;});
    reject_birth([](auto& rows){rows[1].object_key=rows[0].object_key;});
    reject_birth([](auto& rows){rows[1].origin.generation++;});
    reject_birth([](auto& rows){rows[1].state.alive=false;});
    reject_birth([](auto& rows){rows[1].state.scale[0]=std::numeric_limits<float>::infinity();});
    reject_birth([&](auto& rows){rows[1].object_key=other;});
    reject_birth([](auto& rows){rows.clear();});
    need(world.commit_dungeon_census(origin,1,operation,exterior,{},created,error)==table::Admission::New,error);++checks;
    need(world.rows().at(other)==original.at(other)&&world.rows().size()==3,"Dungeon birth changed unrelated World row");++checks;
    const auto born=world.rows();
    auto rejected=[&](auto before,auto after){
        need(world.commit_dungeon_census(origin,2,operation,exterior,before,after,error)==table::Admission::Rejected&&
             world.rows()==born&&world.revision()==1,"invalid Dungeon effect partly published");++checks;
    };
    auto changed=created;changed[1].state.x=1;rejected(created,changed);
    changed=created;changed[1].local_uid=0x300000055ULL;rejected(created,changed);
    changed=created;changed[0].state.variant="different";rejected(changed,created);
    changed=created;changed[1].state.alive=false;rejected(created,changed);
    auto moved=created;moved[0].state.x=9;
    need(world.commit_dungeon_census(origin,2,operation,exterior,created,moved,error)==table::Admission::New,error);++checks;
    need(world.rows().at(child).state==created[1].state&&world.rows().at(exterior).state==moved[0].state,
         "Dungeon Move changed another complete member");++checks;
    need(world.commit_dungeon_census(origin,3,operation,exterior,moved,{},error)==table::Admission::New,error);++checks;
    need(!world.rows().at(exterior).state.alive&&!world.rows().at(child).state.alive&&
         world.rows().at(other)==original.at(other),"whole Dungeon retirement lost a member or unrelated row");++checks;
    const auto retired=world.rows();
    changed=moved;changed[1].state.variant="corrupt";
    need(world.commit_dungeon_census(origin,4,operation,exterior,{},changed,error)==table::Admission::Rejected&&
         world.rows()==retired,"Dungeon restoration accepted changed child payload");++checks;
    auto restored=moved;restored[0].local_uid=0x400000055ULL;restored[1].local_uid=0x500000055ULL;
    need(world.commit_dungeon_census(origin,4,operation,exterior,{},restored,error)==table::Admission::New,error);++checks;
    need(world.rows().at(exterior).state==moved[0].state&&world.rows().at(child).state==moved[1].state,
         "Dungeon restoration did not retain canonical typed values");++checks;
    const auto stable=world.rows();
    need(world.commit_dungeon_census(origin,4,operation,exterior,restored,{},error)==table::Admission::Rejected&&
         world.rows()==stable,"Dungeon stale revision changed World");++checks;
    auto foreign=origin;++foreign.world_epoch;
    need(world.commit_dungeon_census(foreign,5,operation,exterior,restored,{},error)==table::Admission::Rejected&&
         world.rows()==stable,"Dungeon foreign World admitted");++checks;
    // A natural Instance is already SceneClone-admitted but may first enter this
    // typed table in the next authored action.
    auto natural=restored;
    natural.push_back({origin,"action:55555555-5555-4555-8555-555555555555",0x600000055ULL,created[1].state});
    need(world.commit_dungeon_census(origin,5,operation,exterior,natural,natural,error)==table::Admission::New&&
         world.rows().size()==4,error);++checks;
    return checks;
}
