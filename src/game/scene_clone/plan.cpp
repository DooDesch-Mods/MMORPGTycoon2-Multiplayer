#include "raw.h"
namespace mt2mp::game::scene_clone {
bool same_world(const dungeon_scene_identity_runtime::World& a,const dungeon_scene_identity_runtime::World& b){return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&a.generation==b.generation&&a.world_epoch==b.world_epoch&&a.uuid==b.uuid&&a.sha256==b.sha256;}
bool same_owner(const Pool& a,const Pool& b){const auto& x=a.scene;const auto& y=b.scene;return x.owner.key==y.owner.key&&x.owner.exterior_key==y.owner.exterior_key&&x.owner.type==y.owner.type&&x.owner.instance_index==y.owner.instance_index&&x.owner.shard==y.owner.shard&&x.owner.region==y.owner.region&&x.owner.dungeon==y.owner.dungeon&&x.owner.instance==y.owner.instance&&x.owner.exterior==y.owner.exterior&&x.owner.exterior_uid==y.owner.exterior_uid&&x.dungeon_uid==y.dungeon_uid&&x.exterior_region==y.exterior_region&&x.exterior_slot==y.exterior_slot&&a.managers==b.managers;}

bool retirements_ready(const Raw& before,const Mapping& mapping,const std::function<bool(uint64_t,uintptr_t&,std::string&)>& find,std::string& e){
    for(const auto& row:mapping.retired){const auto pool=std::find_if(before.pools.begin(),before.pools.end(),[&](const auto& value){return value.scene.owner.key==row.scene;});
        if(pool==before.pools.end()){e="Scene clone retirement scene absent from confirmed baseline";return false;}
        const auto member=std::find_if(pool->members.begin(),pool->members.end(),[&](const auto& value){return value.family==row.family&&value.child==row.child&&value.key==row.key;});
        if(member==pool->members.end()||!member->uid||!member->object){e="Scene clone retirement lacks exact original full UID/key";return false;}
        uintptr_t registered{};if(!find(member->uid,registered,e))return false;
        if(registered){e="Scene clone original member deletion is incomplete full_uid="+std::to_string(member->uid);return false;}
    }e.clear();return true;
}
bool plan(const Raw& before,const Raw& after,const std::string& operation,const Mapping& previous,const Mapping* authority,Mapping& out,Changes& changes,std::string& e){
    out={};changes={};const auto fail=[&](const char* cause){e=cause;return false;};
    if(!same_world(before.world,after.world))return fail("Scene clone raw baseline world lifetime changed");
    if(authority&&(!valid(*authority,e)||authority->operation!=operation))return fail("Scene clone Host mapping operation/shape differs");
    Mapping next;next.operation=operation;next.retired=authority?authority->retired:previous.retired;
    for(const auto& retired:previous.retired)if(std::find(next.retired.begin(),next.retired.end(),retired)==next.retired.end())return fail("Scene clone Host mapping forgot a confirmed retirement");
    uint32_t ordinal=0;size_t index=0;std::set<std::string> old_scenes,new_scenes;std::set<uint64_t> uids;std::set<uintptr_t> pointers;
    for(const auto& p:before.pools)if(!old_scenes.insert(p.scene.owner.key).second)return fail("Scene clone baseline scene duplicated");
    for(const auto& pool:after.pools){if(!new_scenes.insert(pool.scene.owner.key).second)return fail("Scene clone current scene duplicated");
        SceneCount counts;counts.scene=pool.scene.owner.key;
        for(const auto& m:pool.members){if(m.family==Family::Gizmo)++counts.gizmos;else if(m.family==Family::MonsterZone)++counts.zones;else if(m.family==Family::DirectMonster)++counts.monsters;else return fail("Scene clone raw family invalid");}next.scenes.push_back(counts);
        if(counts.zones!=pool.zones.zones.size())return fail("Scene clone raw Zone family roster incomplete");
        const auto old=std::find_if(before.pools.begin(),before.pools.end(),[&](const auto& p){return p.scene.owner.key==pool.scene.owner.key;});
        const bool born=old==before.pools.end();
        if(born&&pool.scene.owner.type!=2)return fail("Scene clone birth is not an original Instance scene");
        if(!born&&!same_owner(*old,pool))return fail("Scene clone surviving scene owner replaced without retirement");
        for(const auto& m:pool.members){
            if(!m.object||!m.uid||!uids.insert(m.uid).second||!pointers.insert(m.object).second)return fail("Scene clone raw member full UID/pointer missing or duplicated");
            Assignment row{pool.scene.owner.key,m.key,m.family,m.child,m.ordinal,m.origin};
            if(born){
                if(ordinal>=12288)return fail("Scene clone complete ordered birth batch exceeds budget");
                row.ordinal=ordinal++;row.origin=operation;row.key=birth_key(operation,m.family,row.ordinal);
                if(authority){if(index>=authority->members.size())return fail("Scene clone Host mapping omitted birth");row=authority->members[index];
                    if(row.origin.empty()||row.scene!=pool.scene.owner.key||row.family!=m.family||row.child!=m.child)return fail("Scene clone Host birth lacks original ordered provenance");}
                changes.births.push_back(row);
            }else{
                if(row.key.empty())return fail("Scene clone retained member lacks confirmed registry alias");
                const auto prior=std::find_if(old->members.begin(),old->members.end(),[&](const auto& v){return v.family==m.family&&v.object==m.object&&v.uid==m.uid&&v.key==m.key;});
                if(prior!=old->members.end()){row.origin=prior->origin;row.ordinal=prior->ordinal;}
                if(authority&&(index>=authority->members.size()||authority->members[index]!=row))return fail("Scene clone Host mapping changed a confirmed current alias");
            }
            next.members.push_back(std::move(row));++index;
        }
    }
    for(const auto& pool:before.pools)if(!new_scenes.contains(pool.scene.owner.key)){
        if(pool.scene.owner.type!=2)return fail("Scene clone retirement removed a template scene");
        for(const auto& m:pool.members){if(!m.uid||!m.object||m.key.empty())return fail("Scene clone retirement lacks confirmed canonical/full UID baseline");
            Assignment row{pool.scene.owner.key,m.key,m.family,m.child,m.ordinal,m.origin};changes.retired.push_back(row);
            const auto found=std::find(next.retired.begin(),next.retired.end(),row);
            if(authority&&found==next.retired.end())return fail("Scene clone Host mapping omitted local confirmed retirement");
            if(!authority&&found==next.retired.end())next.retired.push_back(std::move(row));
        }
    }
    if(!valid(next,e))return false;
    if(authority&&next!=*authority)return fail("Scene clone Host complete ordered current roster differs");
    out=std::move(next);e.clear();return true;
}
}
