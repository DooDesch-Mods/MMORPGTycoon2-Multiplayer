    for(int32_t type:{1,2}){
        Memory memory;memory.init(type);
        constexpr uintptr_t object=0x21000000,manager=0x21002000,children=0x21004000;
        constexpr uintptr_t outer_region=0x21006000,outer_manager=0x21008000,outer_children=0x2100a000;
        memory.put(object,0,fixture_image+0x16d93c0);memory.put(object,0x170,fixture_shard);memory.put(object,0x178,fixture_region);
        memory.put(fixture_region,0x950,manager);memory.put(manager,0,fixture_image+0x17183c0);
        memory.put(manager,0x450,fixture_shard);memory.put(manager,0x458,fixture_region);
        memory.put(manager,0x408,fixture_image+0x172d7c0);memory.put(manager,0x410,children);
        memory.put(manager,0x418,int32_t(1));memory.put(manager,0x41c,int32_t(1));memory.put(children,0,object);
        memory.put(fixture_exterior,0x178,outer_region);memory.put(outer_region,0x950,outer_manager);
        memory.put(outer_manager,0x410,outer_children);memory.put(outer_manager,0x418,int32_t(1));
        memory.put(outer_manager,0x41c,int32_t(1));memory.put(outer_children,0,fixture_exterior);
        const auto path=[&](Memory& bytes,std::string& suffix,std::string& error,uintptr_t expected=0,bool mutate=false){
            if(!expected)expected=manager+0x408;
            auto at=[&](auto p,auto o,auto& v,auto& e){return bytes.read(p,o,v,e);};
            auto typed=[&](auto p,auto s,auto t,auto& e){return bytes.typed(p,s,t,e);};
            auto root_slot=[&](uintptr_t p,int32_t child,int32_t& id,uintptr_t array_pointer,std::string& e){
                if(p!=fixture_exterior||child!=0||array_pointer!=outer_manager+0x408){e="fixture current root exterior differs";return false;}
                id=67;
                if(mutate)bytes.put(manager,0x450,fixture_root);
                return true;
            };
            return mt2mp::game::dungeon_saved_building::path(fixture_image,fixture_root-0x1c0,object,0,expected,at,typed,root_slot,suffix,error);
        };
        std::string suffix,error;
        check(path(memory,suffix,error),error.c_str());
        const auto expected="inline:region{67}/buildingManager/building[0]"+
            std::string(type==1?"/template":"/instance[7]")+"/buildingManager/building[0]";
        check(suffix==expected,"contained saved Building path lost exterior or saved Instance index");
        const auto canonical=std::string(64,'a')+":world:building:"+suffix;
        check(mt2mp::game::snapshot_building_key::valid(canonical),"contained inline saved Building key rejected");
        check(!path(memory,suffix,error,manager+0x409),"foreign serialized Building array accepted");
        auto changed=memory;changed.put(children,0,fixture_exterior);
        check(!path(changed,suffix,error),"foreign contained Building child accepted");
        changed=memory;changed.put(outer_children,0,object);
        check(!path(changed,suffix,error),"foreign root exterior child accepted");
        changed=memory;changed.put(manager,0x458,outer_region);
        check(!path(changed,suffix,error),"foreign contained Building manager accepted");
        changed=memory;
        check(!path(changed,suffix,error,manager+0x408,true),"manager mutation during original root admission accepted");
        for(const auto& bad:{"/templateX/buildingManager/building[0]","/instance[-1]/buildingManager/building[0]",
                            "/instance[07]/buildingManager/building[0]","/template/buildingManager/building[00]",
                            "/template/buildingManager/building[0]/template/buildingManager/building[0]"}){
            check(!mt2mp::game::snapshot_building_key::valid(std::string(64,'a')+
                ":world:building:inline:region{67}/buildingManager/building[0]"+bad),"malformed contained inline saved key accepted");
        }
    }
