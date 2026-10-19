namespace mt2mp::game::building_npc_lifecycle {
namespace saved=npc_saved_identity_runtime;
namespace detail=saved::building_child_detail;
bool capture_birth(const Lease& lease,const Parent& parent,const OwnerResolver& resolver,Receipt& out,std::string& error){
    std::lock_guard lock(saved::gate);return detail::capture(lease,parent,resolver,true,out,error);
}
bool capture_before(const Lease& lease,const Parent& parent,const OwnerResolver& resolver,Receipt& out,std::string& error){
    std::lock_guard lock(saved::gate);return detail::capture(lease,parent,resolver,false,out,error);
}
bool validate_make_npc(const Lease& lease,uintptr_t node,Receipt& before,MakeNPCInverse& out,std::string& error){
    out={};std::lock_guard lock(saved::gate);saved::opened::CurrentWorld world;saved::ChildBinding* bound=nullptr;
    if(!detail::lease(lease,world,error)||!detail::binding(lease,before,bound,error))return false;
    if(!before.present||bound->receipt.original_record_sha!=before.original_record_sha||
       (bound->retired&&(before.inverse_child_uid!=before.child.row.local_uid||bound->receipt.inverse_child_uid!=before.inverse_child_uid)))
        return saved::q::fail(error,"Building child MakeNPC inverse requires its complete before receipt and retained retirement inverse");
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return saved::q::at(saved::memory,p,offset,value,cause);};
    auto text=[&](uintptr_t p,size_t offset,std::string& value,std::string& cause){
        return npc_quest_records::read_string(saved::memory,p,offset,"MakeNPC name",value,cause);};
    if(!building_npc_owner::make_npc_inverse(at,text,saved::image,node,before,out,error)||
       !building_npc_owner::inverse_uid_set(at,node+8,out.produced_uid,error)||
       !building_npc_owner::inverse_uid_set(at,node+0x30,out.required_character_uid,error))return false;
    if(before.inverse_child_uid&&before.inverse_child_uid!=out.produced_uid)
        return saved::q::fail(error,"Building child NPC original graph repeats a conflicting MakeNPC inverse");
    before.inverse_child_uid=out.produced_uid;bound->receipt.inverse_child_uid=out.produced_uid;error.clear();return true;
}
bool confirm_retired(const Lease& lease,const Receipt& before,std::string& error){
    std::lock_guard lock(saved::gate);saved::opened::CurrentWorld world;saved::ChildBinding* bound=nullptr;
    if(!detail::lease(lease,world,error)||!detail::binding(lease,before,bound,error))return false;
    if(bound->retired)return saved::q::fail(error,"Building child NPC retirement was already confirmed");
    if(before.present){
        if(before.inverse_child_uid!=before.child.row.local_uid||bound->receipt.inverse_child_uid!=before.inverse_child_uid)
            return saved::q::fail(error,"Building child NPC retirement lacks the validated original MakeNPC inverse");
        detail::Lookup find;uintptr_t region=0;npc_census::State rows;
        if(!find.initialize(error)||!detail::census(world,before.parent.saved_region,region,rows,error)||
           !building_npc_owner::absent(find,rows,before.child.row,error))return false;
        std::vector<uintptr_t> quests;for(const auto& local:before.original_quests.local)quests.push_back(local.quest);
        npc_quest_retirement::State retired;
        if(!npc_quest_retirement::read(saved::memory,saved::image,quests,retired,error))return false;
        if(!retired.all_absent())return saved::q::fail(error,"Building child NPC original queued deletion retains an owned Quest");
    }
    bound->retired=true;error.clear();return true;
}
bool confirm_restored(const Lease& lease,const Receipt& before,const Parent& parent,const OwnerResolver& resolver,Receipt& out,std::string& error){
    out={};std::lock_guard lock(saved::gate);saved::opened::CurrentWorld world;saved::ChildBinding* bound=nullptr;
    if(!detail::lease(lease,world,error)||!detail::binding(lease,before,bound,error))return false;
    if(!bound->retired||parent.key!=before.parent.key||parent.uid!=before.parent.uid||parent.saved_region!=before.parent.saved_region)
        return saved::q::fail(error,"Building child NPC restoration lacks its retired canonical parent/full UID receipt");
    npc_census::State rows;npc_census::Row row;bool present=false;detail::Lookup find;
    if(!detail::read(lease,parent,world,rows,row,present,find,error))return false;
    if(present!=before.present)return saved::q::fail(error,"Original Building restoration changed the presence of its owned child NPC");
    if(present){
        if(bound->receipt.inverse_child_uid!=before.inverse_child_uid||
           !building_npc_owner::restored_uid(parent,before.inverse_child_uid,before.child.row,row,error))return false;
        if(row.type!=before.child.row.type||row.character_type!=before.child.row.character_type||row.character_slot!=before.child.row.character_slot||
           row.level!=before.child.row.level||row.name!=before.original_display_name||row.position!=before.child.row.position||
           row.rotation!=before.child.row.rotation||row.scale!=before.child.row.scale)
            return saved::q::fail(error,"Original MakeNPC restored fields differ from its validated saved payload");
    }
    Receipt next;next.lease=lease;next.parent=parent;next.present=present;
    if(present){next.child={before.child.key,parent.saved_region,row};if(!detail::witness(world,resolver,next,error))return false;
        next.inverse_child_uid=before.inverse_child_uid;
        auto pool=saved::pools.find(parent.saved_region);
        if(pool==saved::pools.end())return saved::q::fail(error,"Building child NPC restored Region has no retained snapshot pool");
        npc_saved_identity::Record* original=nullptr;
        for(auto& record:pool->second.pool.records){const auto key=npc_saved_identity::key(saved::sha,parent.saved_region,record);
            if(key==before.child.key){
                if(original||record.pointer!=before.child.row.local_pointer||record.uid!=before.child.row.local_uid||record.type!=row.type)
                    return saved::q::fail(error,"Building child NPC saved identity restore source was replaced");original=&record;
            }else if(record.pointer==row.local_pointer||record.uid==row.local_uid)
                return saved::q::fail(error,"Building child NPC restored original collides with another saved identity");
        }
        if(snapshot_npc_key::valid(before.child.key)&&!original)return saved::q::fail(error,"Building child NPC restored saved key has no original serialized record");
        saved::opened::CurrentWorld after;npc_census::State final_rows;npc_census::Row final_row;bool final_present=false;
        if(!detail::read(lease,parent,after,final_rows,final_row,final_present,find,error)||!saved::same_world(world,after)||!final_present||final_row!=row)
            return saved::q::fail(error,error.empty()?"Building child NPC restored owner changed during original capture":error);
        if(original){original->pointer=row.local_pointer;original->uid=row.local_uid;}
    }
    bound->receipt=next;bound->retired=false;out=std::move(next);error.clear();return true;
}
bool resolve(const std::string& key,uint64_t uid,npc_saved_identity::Current& out,std::string& error){
    out={};if(!uid||(!building_npc_key::valid(key)&&!snapshot_npc_key::valid(key)))return saved::q::fail(error,"Building child NPC resolution requires its canonical key and current full UID");
    std::vector<npc_saved_identity::Current> rows;if(!saved::read_current(rows,error))return false;
    for(const auto& row:rows)if(row.key==key){if(row.row.local_uid!=uid)return saved::q::fail(error,"Building child NPC restored canonical owner full UID differs");out=row;error.clear();return true;}
    return saved::q::fail(error,"Building child NPC canonical owner is absent from its complete current Region census");
}
bool current_aliases(std::vector<npc_saved_identity::Current>& out,std::string& error){
    out.clear();std::vector<npc_saved_identity::Current> rows;if(!saved::read_current(rows,error))return false;
    std::lock_guard lock(saved::gate);
    for(const auto& [key,bound]:saved::building_children){(void)key;const auto& receipt=bound.receipt;if(bound.retired||!receipt.present)continue;
        for(const auto& row:rows)if(row.key==receipt.child.key){
            if(row.row.local_pointer!=receipt.child.row.local_pointer||row.row.local_uid!=receipt.child.row.local_uid)
                return saved::q::fail(error,"Building child NPC current alias census changed its retained local identity");
            out.push_back(row);break;
        }
    }error.clear();return true;
}
}
