quest_state::Toon populated(size_t count){
    quest_state::Toon state;
    for(size_t i=0;i<count;++i){QuestOwner owner;owner.key="npc"+std::to_string(i);owner.npc=600000+i*0x100;owner.quests={{1,700000+i*0x100},{2,800000+i*0x100}};owner_catalog.push_back(owner);
        quest_state::Progress row;row.npc={quest_state::LinkState::Live,owner.key,-1};row.quest={quest_state::LinkState::Live,owner.key,1};state.progress.push_back(row);
    }
    return state;
}
void noop_cases(NativePort& port,std::string& e){
    auto o=setup();auto old=populated(6);auto next=old;next.progress[1].quest.sequence_id=2;init_toon(50000,old);
    need(port.begin_quest_stores(o,e)&&port.store_quest_state(o,0,old,next,e),"one changed link among six retained rows");
    need(ensures==0&&binds==1&&retires==0&&callbacks==1,"only genuine changed Quest link executes native helper");
    need(collections==2,"one fresh owner collection after actual callback");
    need(toons[50000].progress[1].quest==next.progress[1].quest,"actual changed link published");port.end_quest_stores();

    o=setup();old=populated(6);next=old;next.progress.erase(next.progress.begin());init_toon(50000,old);const auto retained=progress_pointers[1];
    need(port.begin_quest_stores(o,e)&&port.store_quest_state(o,0,old,next,e),"retirement shifts live row order");
    need(retires==1&&ensures==0&&binds==0&&progress_pointers[0]==retained,"shifted retained rows use fresh slots without redundant helpers");port.end_quest_stores();

    o=setup();next=populated(4);old=next;old.progress.pop_back();init_toon(50000,old);
    need(port.begin_quest_stores(o,e)&&port.store_quest_state(o,0,old,next,e),"append missing Progress row");
    need(ensures==1&&binds==1&&retires==0&&progress_pointers.size()==4,"new row keeps original ensure and bind effects");port.end_quest_stores();

    o=setup();old=populated(3);next=old;std::swap(next.progress[0],next.progress[1]);init_toon(50000,old);
    need(port.begin_quest_stores(o,e)&&!port.store_quest_state(o,0,old,next,e),"unsupported reordered existing membership rejected");
    need(e=="Quest apply ordered Progress membership differs"&&ensures==1&&binds==0&&stores==0,"original ordered-membership failure and effect order retained");port.end_quest_stores();

    o=setup();old=populated(5);next=old;next.progress[0].quest.sequence_id=2;init_toon(50000,old);change_later_link=true;
    need(port.begin_quest_stores(o,e)&&port.store_quest_state(o,0,old,next,e),"earlier callback changes later previously-equal row");
    need(binds==2&&ensures==0,"fresh later row proof restores changed live link instead of using stale before");port.end_quest_stores();

    o=setup();old=populated(4);next=old;next.weapon_ilvl=3;next.progress[0].quest.sequence_id=2;init_toon(50000,old);
    need(port.begin_quest_stores(o,e)&&port.store_quest_state(o,0,old,next,e),"weapon callbacks precede mixed Progress proof");
    need(callbacks==3&&binds==1&&ensures==0&&collections==3,"recollect owners after Property.Load and Equip before any row bypass");port.end_quest_stores();

    o=setup();old=populated(4);next=old;next.progress[1].quest={};init_toon(50000,old);
    need(port.begin_quest_stores(o,e)&&port.store_quest_state(o,0,old,next,e),"genuine link clear");
    need(binds==1&&ensures==0&&!toons[50000].progress[1].quest.npc_key.size(),"only genuine clear reaches original binding helper");port.end_quest_stores();

    for(int kind=0;kind<3;++kind){o=setup();old=populated(3);next=old;next.progress[1].quest.sequence_id=2;init_toon(50000,old);
        if(kind==0)put(progress_pointers[0],0x18,uintptr_t(0));
        if(kind==1)put(progress_pointers[0],0x70,uintptr_t(0));
        if(kind==2)put(uintptr_t(700000+0x1000000),8,int32_t(0));
        need(port.begin_quest_stores(o,e)&&!port.store_quest_state(o,0,old,next,e),"selected helper-only guard failure");
        const char* errors[]={"Progress row/NPC Weak addresspoints differ","Quest link Instance/Weak addresspoints differ","Quest link Proxy refcount not positive"};
        need(e==errors[kind]&&callbacks==0&&stores==0,"helper-only selected row cause retained before effects");port.end_quest_stores();
    }
    o=setup();old=populated(3);next=old;next.progress[0].quest.sequence_id=2;init_toon(50000,old);helper_failure="bind";
    need(port.begin_quest_stores(o,e)&&!port.store_quest_state(o,0,old,next,e)&&e=="original bind failure","genuine helper failure preserved");
    need(binds==1&&ensures==0&&stores==0,"failed genuine helper stops later stores");port.end_quest_stores();

    o=setup();old=populated(3);next=old;next.progress[0].quest.sequence_id=2;init_toon(50000,old);callback_owner_failure=true;
    need(port.begin_quest_stores(o,e)&&!port.store_quest_state(o,0,old,next,e)&&e=="original collection failure","owner changes after callback are not hidden by old identity snapshot");
    need(binds==1&&collections==2,"owner failure aborts before later-row helper effects");port.end_quest_stores();
    need(toons[50000].progress[0].quest==next.progress[0].quest,"failure retains earlier confirmed original binding effect");
    o=setup();old=populated(6);next=old;next.progress[1].quest.sequence_id=2;init_toon(50000,old);strict_reader=true;
    need(port.begin_quest_stores(o,e)&&port.store_quest_state(o,0,old,next,e),"actual reader accepts mixed retained rows");
    need(reads==3&&binds==1&&ensures==0,"one baseline, one post-callback and one final full Toon read");port.end_quest_stores();

    o=setup();old=populated(5);next=old;next.progress[0].quest.sequence_id=2;init_toon(50000,old);strict_reader=true;change_later_link=true;
    need(port.begin_quest_stores(o,e)&&port.store_quest_state(o,0,old,next,e),"actual reader sees callback change to later retained row");
    need(reads==3&&binds==2&&ensures==0,"later current native Weak proof triggers required second bind");port.end_quest_stores();

    o=setup();old=populated(4);next=old;next.weapon_ilvl=3;next.progress[0].quest.sequence_id=2;init_toon(50000,old);strict_reader=true;
    need(port.begin_quest_stores(o,e)&&port.store_quest_state(o,0,old,next,e),"actual reader validates after weapon callbacks");
    need(reads==4&&callbacks==3&&binds==1,"post-weapon and post-bind full validation retained");port.end_quest_stores();

    o=setup();old=populated(5);next=old;next.progress.erase(next.progress.begin());init_toon(50000,old);strict_reader=true;
    need(port.begin_quest_stores(o,e)&&port.store_quest_state(o,0,old,next,e),"actual reader validates shifted array after retirement");
    need(reads==3&&retires==1&&binds==0&&ensures==0,"retirement boundary validated once");port.end_quest_stores();

    o=setup();next=populated(4);old=next;old.progress.pop_back();init_toon(50000,old);strict_reader=true;
    need(port.begin_quest_stores(o,e)&&port.store_quest_state(o,0,old,next,e),"actual reader validates appended array after ensure");
    need(reads==3&&ensures==1&&binds==1,"append boundary validation and original helper counts retained");port.end_quest_stores();

    const char* faults[]={"capacity","extent","duplicate","scalar","completed","monsters","header","order"};
    const char* causes[]={"Quest visitor array AP, pointer, count or capacity differs","Quest visitor array AP, pointer, count or capacity differs","Quest visitor progress AP, pointer or duplicate differs","Quest visitor Toon scalar domain differs","Quest visitor completed count below discovery sentinel","Quest visitor instance count or boolean differs","Quest visitor array header changed during read","Quest visitor progress pointer changed during read"};
    for(size_t i=0;i<4;++i){
        o=setup();old=populated(4);next=old;next.progress[0].quest.sequence_id=2;init_toon(50000,old);strict_reader=true;
        if(i==0)put(uintptr_t(50000),0xb04,int32_t(100001));
        if(i==1)put(uintptr_t(50000),0xaf8,UINTPTR_MAX-uintptr_t(7));
        if(i==2)put(uintptr_t(7000),8,progress_pointers.front());
        if(i==3)put(uintptr_t(50000),0x6f8,int32_t(-1));
        need(port.begin_quest_stores(o,e)&&!port.store_quest_state(o,0,old,next,e),"initial full reader rejects structural/domain failure before first selected proof");
        need(e.starts_with(causes[i])&&reads==1&&callbacks==0&&stores==0,"initial full reader original cause and no effects retained");port.end_quest_stores();
    }
    for(size_t i=0;i<std::size(faults);++i){
        o=setup();old=populated(4);next=old;next.progress[0].quest.sequence_id=2;init_toon(50000,old);strict_reader=true;validation_fault=faults[i];
        need(port.begin_quest_stores(o,e)&&!port.store_quest_state(o,0,old,next,e),"post-callback full reader rejects original structural/domain failure");
        need(e.starts_with(causes[i])&&reads==2&&binds==1&&ensures==0&&stores==0,"original reader failure stops next row before CAS or helper");port.end_quest_stores();
    }

    // Only identities cross scalar rows. Every complete Toon and Progress value
    // is still sampled before and after its original ordered CAS operations.
    o=setup();old=populated(3);next=old;
    for(auto& row:next.progress){row.completed=2;row.monsters_defeated=3;row.visited=row.action_performed=row.complete=true;}
    need(port.begin_quest_stores(o,e),"begin unchanged-link Progress scalar span");
    std::vector<uintptr_t> expected_progress_cas;
    for(size_t i=0;i<3;++i){init_toon(50000+i*10000,old);strict_reader=true;
        for(auto pointer:progress_pointers)for(auto offset:{0x50,0xa8,0xac})expected_progress_cas.push_back(pointer+offset);
        need(port.store_quest_state(o,i,old,next,e),"unchanged links admit fresh Progress scalar changes");
    }
    need(collections==1&&reads==6&&census_reads==15&&callbacks==0&&ensures==0&&binds==0&&retires==0,"one admission, selected Subscriber slots, and full per-Toon before/final reads");
    need(stores==27&&cas_addresses==expected_progress_cas,"all Progress scalar CAS addresses and ordering retained");
    need(port.quest_owners_ready_,"Progress scalar rows retain identity span");port.end_quest_stores();

    o=setup();old=populated(2);next=old;next.progress[0].completed=2;next.progress[1].monsters_defeated=3;
    init_toon(50000,old);strict_reader=true;need(port.begin_quest_stores(o,e)&&port.store_quest_state(o,0,old,next,e),"prime Progress scalar live-value failure");
    init_toon(60000,old);put(progress_pointers[1],0xa8,int32_t(99));const auto prior_stores=stores;
    need(!port.store_quest_state(o,1,old,next,e)&&e=="Quest apply prior Toon state changed"&&stores==prior_stores&&collections==1,"span does not hide changed live Progress values");port.end_quest_stores();

    o=setup();old=populated(2);next=old;next.progress[0].completed=2;next.progress[1].monsters_defeated=3;
    init_toon(50000,old);strict_reader=true;fail_store=2;
    need(port.begin_quest_stores(o,e)&&!port.store_quest_state(o,0,old,next,e)&&e=="original CAS failure"&&stores==2,"Progress CAS original failure stops later writes");
    int32_t scalar=0;need(at(progress_pointers[0],0x50,scalar,e)&&scalar==2,"earlier Progress CAS remains applied after later failure");
    need(at(progress_pointers[1],0xa8,scalar,e)&&scalar==0,"failed Progress CAS preserves old live value");port.end_quest_stores();

    o=setup();old=populated(2);next=old;next.progress[0].completed=2;init_toon(50000,old);strict_reader=true;final_bad=true;
    need(port.begin_quest_stores(o,e)&&!port.store_quest_state(o,0,old,next,e)&&e=="Quest apply complete Toon state readback differs"&&stores==1,"complete final mismatch still rejects Progress scalar span");port.end_quest_stores();

    o=setup();old=populated(2);next=old;next.progress[0].completed=2;init_toon(50000,old);strict_reader=true;
    put(o.history.pin.clock,0x5c,float(0));
    need(port.begin_quest_stores(o,e)&&port.store_quest_state(o,0,old,next,e),"Progress scalar path restores actual pause");
    need(callbacks==1&&!active_during_callback&&!port.quest_owners_ready_,"Progress scalar identity span ends before actual pause callback");
    init_toon(60000,old);need(port.store_quest_state(o,1,old,next,e)&&collections==2,"Progress scalar path reacquires after pause callback");port.end_quest_stores();

    o=setup();old=populated(2);next=old;next.progress[0].completed=2;init_toon(50000,old);strict_reader=true;
    need(port.begin_quest_stores(o,e)&&port.store_quest_state(o,0,old,next,e),"prime before genuine link change");
    auto changed_link=old;changed_link.progress[1].quest.sequence_id=2;init_toon(60000,old);
    need(port.store_quest_state(o,1,old,changed_link,e)&&binds==1&&callbacks==1&&!active_during_callback&&!port.quest_owners_ready_,"real changed link retains original helper outside scalar span");
    init_toon(70000,old);need(port.store_quest_state(o,2,old,next,e)&&collections==3,"Progress scalar row reacquires after genuine link callback");port.end_quest_stores();

    o=setup();old=populated(2);next=old;next.progress[0].completed=2;init_toon(50000,old);strict_reader=true;
    need(port.store_quest_state(o,0,old,next,e),"standalone Progress scalar row");init_toon(60000,old);
    need(port.store_quest_state(o,1,old,next,e)&&collections==2&&census_reads==18&&!port.quest_owners_ready_,"standalone Progress stores retain original full Subscriber admission");

    o=setup();old=populated(1);old.progress[0].quest={quest_state::LinkState::Expired,"",-1};next=old;next.progress[0].completed=2;init_toon(50000,old);strict_reader=true;
    put(progress_pointers[0],0xa0,uintptr_t(9000000));put(uintptr_t(9000000),0,uintptr_t(0));put(uintptr_t(9000000),8,int32_t(1));
    need(port.begin_quest_stores(o,e)&&port.store_quest_state(o,0,old,next,e)&&reads==3&&callbacks==0&&port.quest_owners_ready_,"unchanged expired Quest keeps original extra lifetime read");port.end_quest_stores();

    for(size_t count:{1u,2u,8u,16u}){
        o=setup();old=populated(count);next=old;
        for(auto& row:next.progress){row.completed=2;row.monsters_defeated=3;row.visited=row.action_performed=row.complete=true;}
        init_toon(50000,old);strict_reader=true;
        need(port.begin_quest_stores(o,e)&&port.store_quest_state_original(o,0,old,next,e),"retained original scalar Progress body");
        const auto original_reads=pointer_reads,original_headers=pointer_headers;
        const auto original_cas=cas_addresses;const auto original_events=events;const auto original_bytes=bytes;
        need(original_reads==3*count*count+3*count,"original exact pointer/AP read count");port.end_quest_stores();
        o=setup();old=populated(count);init_toon(50000,old);strict_reader=true;
        need(port.begin_quest_stores(o,e)&&port.store_quest_state(o,0,old,next,e),"selected scalar Progress body");
        need(pointer_reads==4*count&&original_reads-pointer_reads==3*count*count-count,"selected exact pointer/AP read reduction");
        need(pointer_headers==count+1&&original_headers-pointer_headers==count&&reads==2&&callbacks==0,"selected headers and full before/final retained; duplicate membership headers removed");
        need(cas_addresses==original_cas&&events==original_events&&bytes==original_bytes,"same ordered effects and every final byte");
        std::cout<<"Progress rows="<<count<<" pointer/AP reads "<<original_reads<<" -> "<<pointer_reads<<"; headers="<<pointer_headers<<"\n";
        const auto selected_reads=pointer_reads,selected_headers=pointer_headers;
        port.end_quest_stores();
        o=setup();old=populated(count);init_toon(50000,old);strict_reader=true;
        need(port.begin_quest_stores(o,e)&&port.store_quest_state_membership_previous(o,0,old,next,e),"immediate prior actual membership body");
        need(pointer_reads==count*count+5*count&&pointer_headers==2*count+1,"immediate prior exact membership workload");
        need(pointer_reads-selected_reads==count*(count+1)&&4*(pointer_headers-selected_headers)==4*count,"exact removed pointer/AP and header reads");
        need(cas_addresses==original_cas&&events==original_events&&bytes==original_bytes&&reads==2&&callbacks==0,"immediate prior and candidate preserve values, effects and full reads");
        std::cout<<"Membership rows="<<count<<" pointer/AP reads "<<pointer_reads<<" -> "<<selected_reads<<"; removed total reads="<<count*(count+5)<<"\n";
        port.end_quest_stores();
    }
    for(const auto& fault:{"ap","data","count","capacity","slot","class","unreadable"}){
        o=setup();old=populated(3);next=old;next.progress[0].completed=2;init_toon(50000,old);strict_reader=true;pointer_fault=fault;
        need(port.begin_quest_stores(o,e)&&!port.store_quest_state(o,0,old,next,e),"fresh selected header/slot/AP rejects drift after full entry read");
        const auto expected=pointer_fault=="ap"?"Quest apply Progress array header differs":pointer_fault=="class"?"original class guard":pointer_fault=="unreadable"?"original unreadable fixture field":"Quest apply ordered Progress membership differs";
        need(e==expected&&stores==0&&callbacks==0&&reads==1,"selected drift retains cause before first effect");port.end_quest_stores();
    }
    o=setup();old=populated(8);next=old;next.progress[0].completed=2;init_toon(50000,old);strict_reader=true;
    need(port.store_quest_state(o,0,old,next,e)&&pointer_reads==3*8*8+3*8,"standalone path retains full pointer reader count");

    for(const auto& fault:{"later_slot","later_class"}){
        o=setup();old=populated(3);next=old;for(auto& row:next.progress)row.completed=2;
        init_toon(50000,old);strict_reader=true;pointer_fault=fault;
        need(port.begin_quest_stores(o,e)&&!port.store_quest_state(o,0,old,next,e),"later selected proof stays fresh after earlier scalar CAS");
        need(e==(pointer_fault=="later_slot"?"Quest apply ordered Progress membership differs":"original class guard")&&stores==1&&callbacks==0,"later drift aborts before second row mutation");
        int32_t first=0,later=0;
        need(at(progress_pointers[0],0x50,first,e)&&at(progress_pointers[1],0x50,later,e)&&first==2&&later==0,"earlier confirmed CAS retained and later row unchanged");port.end_quest_stores();
    }

}
