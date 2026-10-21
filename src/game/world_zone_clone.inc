bool clone_world_same(const std::map<uintptr_t,WorldZonePool>& left,const std::map<uintptr_t,WorldZonePool>& right){
    if(left.size()!=right.size())return false;
    for(const auto& [manager,pool]:left){const auto found=right.find(manager);if(found==right.end())return false;const auto& other=found->second;
        if(pool.scene!=other.scene||pool.root!=other.root||pool.region!=other.region||pool.manager!=other.manager||pool.rows!=other.rows)return false;}
    return true;
}
bool clone_world_alias(const scene_clone::Pool& pool,const scene_clone::Member& member,std::string& key,std::string& error){
    const auto found=world_zone_pools.find(pool.managers[1]);
    if(found==world_zone_pools.end())return q::fail(error,"Scene clone non-Monster Zone manager lacks confirmed identities");
    const auto& saved=found->second;const auto known=saved.rows.find(member.uid);
    if(saved.scene!=pool.scene.owner.key||saved.root!=pool.scene.owner.shard||saved.region!=pool.scene.owner.region||
       known==saved.rows.end()||known->second.object!=member.object||known->second.type!=scene_clone::zone_type(member.family))
        return q::fail(error,"Scene clone non-Monster Zone exact full UID/type/scene alias differs");
    key=known->second.key;return true;
}
bool preview_world_zones(const scene_clone::Raw& raw,const scene_clone::Mapping& mapping,
                         std::map<uintptr_t,WorldZonePool>& next,std::string& error,const std::string& template_exterior,const scene_clone::Raw* restoration,const scene_clone::Raw* held){
    next=world_zone_pools;
    const auto allowed_template=[&](const dungeon_scene_roster::Scene& scene){return !template_exterior.empty()&&scene.owner.type==1&&scene.owner.exterior_key==template_exterior;};
    const auto restored_scene=[&](const std::string& key){return restoration&&std::any_of(restoration->pools.begin(),restoration->pools.end(),[&](const auto& p){return p.scene.owner.key==key&&p.scene.owner.exterior_key==template_exterior;});};
    for(const auto& [manager,old]:world_zone_pools){if(old.scene=="root")continue;
        const auto live=std::find_if(raw.pools.begin(),raw.pools.end(),[&](const auto& pool){return pool.managers[1]==manager;});
        if(live!=raw.pools.end())continue;
        const auto owner=scene_pools.find(manager);
        if(owner==scene_pools.end()||(owner->second.scene.owner.type!=2&&!allowed_template(owner->second.scene)))return q::fail(error,"Scene clone cannot retire an unconfirmed non-Monster Zone scene");
        for(const auto& [uid,zone]:old.rows){uintptr_t found=0;const auto exception=exact_uid(uid,found);
            if(exception||(scene_clone::held_member(held,old.scene,zone.object,uid)?found!=zone.object:found!=0))return q::fail(error,"Scene clone non-Monster Zone original deletion incomplete exception="+std::to_string(exception));
            const auto row=std::find_if(mapping.retired.begin(),mapping.retired.end(),[&](const auto& value){return value.scene==old.scene&&value.key==zone.key&&scene_clone::zone_type(value.family)==zone.type;});
            if(row==mapping.retired.end())return q::fail(error,"Scene clone non-Monster retirement omitted exact canonical key/type");
        }
        next.erase(manager);
    }
    std::set<std::string> keys;for(const auto& [manager,pool]:next){(void)manager;for(const auto& [uid,row]:pool.rows){(void)uid;if(!keys.insert(row.key).second)return q::fail(error,"Scene clone non-Monster existing key duplicated");}}
    for(const auto& pool:raw.pools){const auto manager=pool.managers[1];const auto old=world_zone_pools.find(manager);const bool born=old==world_zone_pools.end();
        if(born&&(scene_pools.contains(manager)||(pool.scene.owner.type!=2&&!allowed_template(pool.scene))))return q::fail(error,"Scene clone non-Monster new owner is not a new native Instance");
        WorldZonePool fresh{pool.scene.owner.key,pool.scene.owner.shard,pool.scene.owner.region,manager,{}};
        for(const auto& member:pool.members){const int32_t type=scene_clone::zone_type(member.family);if(type<0)continue;
            const auto assigned=std::find_if(mapping.members.begin(),mapping.members.end(),[&](const auto& row){return row.scene==pool.scene.owner.key&&row.family==member.family&&row.child==member.child;});
            if(assigned==mapping.members.end())return q::fail(error,"Scene clone non-Monster current ordered assignment absent");
            std::string key;
            if(born){if((assigned->origin.empty()&&!restored_scene(pool.scene.owner.key))||!keys.insert(assigned->key).second)return q::fail(error,"Scene clone non-Monster original birth provenance absent or key reused");key=assigned->key;}
            else if(!clone_world_alias(pool,member,key,error)||key!=assigned->key)return q::fail(error,error.empty()?"Scene clone non-Monster retained key changed":error);
            if(!fresh.rows.emplace(member.uid,world_zone_runtime::Current{key,fresh.scene,member.object,fresh.root,fresh.region,manager,member.uid,type}).second)
                return q::fail(error,"Scene clone non-Monster current full UID repeated");
        }
        if(!born&&fresh.rows.size()!=old->second.rows.size())return q::fail(error,"Scene clone surviving non-Monster scene lost an admitted member");
        next[manager]=std::move(fresh);
    }
    return true;
}
