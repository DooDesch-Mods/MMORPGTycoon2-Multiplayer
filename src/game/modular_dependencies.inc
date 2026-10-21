namespace modular {
namespace {
struct Dependencies {duplicate_edit::HistoryServices services;monster_quest_retirement::Witness quests;std::array<std::string,2> sha;};
struct DependencyPlan {
    Dependencies original;duplicate_service_history::Removal services;std::vector<uintptr_t> quest_owners;std::set<std::string> related;
    std::vector<uintptr_t> quest_order;std::set<uint64_t> seen_services,seen_quests;
};
bool dependencies(Runtime& r,const action::Binding& binding,const Sample& sample_value,const monster_quest_retirement::Witness* previous,Dependencies& out,std::string& e){
    out={};if(!duplicate_edit::history_service_world(r,sample_value.props,out.services,e)||!npc_probe::world_quest_history::read(binding,net::current_snapshot_id(),out.quests,e,previous))return false;
    std::map<uint64_t,std::string> keys;for(const auto& [key,row]:sample_value.props.objects)if(row.family==Family::Building){const auto found=sample_value.props.handles.find(key);if(found==sample_value.props.handles.end()||!keys.emplace(found->second.uid,key).second)return q::fail(e,"Modular complete dependency Building canonical owner repeats");}
    return duplicate_history_effects::services(out.services.saved,out.services.shared,keys,out.sha[0],e)&&duplicate_history_effects::quests(out.quests,out.sha[1],e);
}
void dependency_effect(const Dependencies& before,const Dependencies& after,const std::set<std::string>& related,action::ModularEffect& effect){effect.dependency_sha={before.sha[0],before.sha[1],after.sha[0],after.sha[1]};effect.related_buildings.assign(related.begin(),related.end());}
bool dependency_plan(const Sample& before,uint64_t born,const Dependencies& values,DependencyPlan& out,std::string& e){out={};out.original=values;
    const auto owner=std::find_if(before.props.handles.begin(),before.props.handles.end(),[&](const auto& row){return row.second.uid==born;});if(owner==before.props.handles.end())return q::fail(e,"Modular dependency original Building full UID absent");
    if(!duplicate_service_history::removal(values.services.saved,{born},out.services,e))return false;out.quest_owners=duplicate_quest_history::affected(values.quests,{owner->second.object});
    for(auto uid:out.services.related_owners){const auto row=std::find_if(before.props.handles.begin(),before.props.handles.end(),[&](const auto& value){return value.second.uid==uid;});if(row==before.props.handles.end()||before.props.objects.at(row->first).family!=Family::Building)return q::fail(e,"Modular reciprocal Service owner lacks its current canonical Building");out.related.insert(row->first);}return true;
}
bool dependency_node(Runtime& r,uintptr_t node,uintptr_t ap,DependencyPlan& plan,std::string& e){uint64_t uid=0;if(!q::at(r.memory,node,0xb8,uid,e))return false;
    auto find=[&](uint64_t id,uintptr_t& pointer,std::string& why){return group_edit::find(r,id,pointer,why);};
    if(ap==r.image+0x1737830){const auto saved=plan.services.saved.find(uid);if(saved==plan.services.saved.end()||!plan.seen_services.insert(uid).second)return q::fail(e,"Modular original MakeService inverse owner is foreign/repeated");return duplicate_service_history::inverse(r.memory,r.image,node,saved->second,find,e);}
    const auto owner=std::find_if(plan.original.quests.owners.begin(),plan.original.quests.owners.end(),[&](const auto& row){return row.second.uid==uid&&std::find(plan.quest_owners.begin(),plan.quest_owners.end(),row.first)!=plan.quest_owners.end();});uintptr_t method=0;
    if(ap!=r.image+0x1713330||owner==plan.original.quests.owners.end()||!plan.seen_quests.insert(uid).second||!q::at(r.memory,ap,0x10,method,e)||method!=r.image+0x563f50)return q::fail(e,e.empty()?"Modular original SetNPCQuests inverse owner/method is foreign/repeated":e);
    duplicate_edit::HistoryQuestMemory memory(r);return monster_quest_retirement::inverse(memory,r.image,node,owner->second,e);
}
bool dependency_graph(Runtime& r,uintptr_t graph,DependencyPlan& plan,std::string& e){
    if(plan.seen_services.size()!=plan.services.saved.size()||plan.seen_quests.size()!=plan.quest_owners.size())return q::fail(e,"Modular graph lacks complete original Service/Quest inverse coverage");q::Bounds sorted;if(!q::bounds(r.memory,graph+0x30,100000,sorted,e))return false;plan.quest_order.clear();
    for(int32_t i=0;i<sorted.count;++i){uintptr_t node=0,ap=0;uint64_t uid=0;if(!q::at(r.memory,sorted.data,size_t(i)*8,node,e)||!q::at(r.memory,node,0,ap,e))return false;if(ap!=r.image+0x1713330)continue;if(!q::at(r.memory,node,0xb8,uid,e))return false;const auto owner=std::find_if(plan.original.quests.owners.begin(),plan.original.quests.owners.end(),[&](const auto& row){return row.second.uid==uid;});if(owner==plan.original.quests.owners.end())return q::fail(e,"Modular sorted Quest inverse owner absent");plan.quest_order.push_back(owner->first);}return q::unchanged(r.memory,graph+0x30,100000,sorted,e);
}
bool dependency_result(Runtime& r,bool undo,const DependencyPlan& plan,const Dependencies& before,const Dependencies& after,uintptr_t destroyed,std::string& e){
    std::set<std::string> changed;for(auto owner:plan.quest_owners)changed.insert(plan.original.quests.owners.at(owner).key);
    if(!duplicate_quest_history::unchanged_owners(before.quests,after.quests,changed,{},{},e))return false;
    if(undo)return duplicate_service_history::absent(plan.services,after.services.saved,e)&&duplicate_quest_history::removed(before.quests,after.quests,{destroyed},e);
    if(!duplicate_quest_history::restored(plan.original.quests,before.quests,after.quests,plan.quest_order,e))return false;
    auto find=[&](uint64_t uid,uintptr_t& pointer,std::string& why){return group_edit::find(r,uid,pointer,why);};
    for(const auto& [uid,saved]:plan.services.saved){uintptr_t object=0,owner=0;if(!find(uid,object,e)||!find(saved.owner_uid,owner,e)||!object||!owner||!duplicate_service_history::restored(r.memory,r.image,object,owner,saved,find,e))return q::fail(e,e.empty()?"Modular original Service restoration lacks exact full readback":e);}return true;
}
}
}
