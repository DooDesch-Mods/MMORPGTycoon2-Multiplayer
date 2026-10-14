namespace quest_visitors {
namespace visitor = mt2mp::npc_quest_visitor_reader;

bool npc_key(npc_census::Memory& memory,uintptr_t image,uintptr_t npc,std::string& key,std::string& error) {
    uintptr_t ap=0;uint64_t uid=0;int32_t type=-1;
    if(!npc_census::field(memory,npc,0,ap,error)||!npc_census::field(memory,npc,0x18,uid,error)||
       !npc_census::field(memory,npc,0x80c,type,error))return false;
    if(ap!=image+0x1742980||!uid||type!=0)
        return fail(error,"Visitor Quest diagnostic NPC class, UID or type differs");
    key="npc-local:"+std::to_string(uid);return true;
}

bool identity(npc_census::Memory& memory,uintptr_t image,visitor::Kind kind,uintptr_t target,
              std::string& key,std::string& error) {
    if(kind==visitor::Kind::Npc)return npc_key(memory,image,target,key,error);
    uintptr_t ap=0,npc=0,proxy=0,owner_proxy=0,weak_target=0,entries=0;
    int32_t sequence=-1,count=-1,capacity=-1;
    if(!npc_census::field(memory,target,0,ap,error)||!npc_census::field(memory,target,0x84,sequence,error)||
       !npc_census::field(memory,target,0xb8,npc,error)||!npc_census::field(memory,target,0xc0,proxy,error))return false;
    if(ap!=image+0x174ac10||sequence<0||!npc||!proxy)
        return fail(error,"Visitor Quest diagnostic live Quest or owner differs");
    if(!npc_census::field(memory,proxy,0,weak_target,error))return false;
    if(weak_target!=npc+0x20)
        return fail(error,"Visitor Quest diagnostic owner Weaklink expired or differs");
    if(!npc_key(memory,image,npc,key,error)||!npc_census::field(memory,npc,0x28,owner_proxy,error)||
       !npc_census::field(memory,npc,0x868,entries,error)||!npc_census::field(memory,npc,0x870,count,error)||
       !npc_census::field(memory,npc,0x874,capacity,error))return false;
    if(owner_proxy!=proxy||count<0||capacity<count||capacity>100000||
       (count&&!entries)||entries>UINTPTR_MAX-size_t(count)*sizeof(uintptr_t))
        return fail(error,"Visitor Quest diagnostic owner array or Weaklink differs");
    size_t found=0;
    for(int32_t i=0;i<count;++i){
        uintptr_t quest=0;if(!npc_census::field(memory,entries,size_t(i)*8,quest,error))return false;
        if(quest==target)++found;
    }
    if(found!=1)return fail(error,"Visitor Quest diagnostic target absent or duplicated in owner array");
    key+=":quest:"+std::to_string(sequence);return true;
}

json link_json(const visitor::Link& value) {
    const char* state=value.state==visitor::LinkState::Live?"live":
        value.state==visitor::LinkState::Expired?"expired":"empty";
    return {{"state",state},{"diagnostic_key",value.diagnostic_key}};
}

json project(const visitor::State& value) {
    json progress=json::array();
    for(const auto& row:value.progress)progress.push_back({
        {"npc",link_json(row.npc)},{"quest",link_json(row.quest)},
        {"completed",row.completed},{"monsters_defeated",row.monsters_defeated},
        {"visited",row.visited},{"action_performed",row.action_performed},{"complete",row.complete}});
    return {{"gold",value.gold},{"xp",value.xp},{"level",value.level},
        {"quests_completed",value.quests_completed},{"progress",std::move(progress)}};
}

struct Header {
    uintptr_t manager=0,data=0;
    int32_t count=0,capacity=0;
    bool operator==(const Header&)const=default;
};

bool header(npc_census::Memory& memory,uintptr_t image,uintptr_t region,Header& out,std::string& error) {
    Header value;uintptr_t ap=0;uint8_t updating=1;
    if(!npc_census::field(memory,region,0x9c0,value.manager,error)||
       !npc_census::field(memory,value.manager,0,ap,error)||!npc_census::field(memory,value.manager,0xc8,updating,error)||
       !npc_census::field(memory,value.manager,0x28,value.data,error)||
       !npc_census::field(memory,value.manager,0x30,value.count,error)||
       !npc_census::field(memory,value.manager,0x34,value.capacity,error))return false;
    if(ap!=image+0x16fde30||updating||value.count<0||value.capacity<value.count||
       value.count>1000000||(value.count&&!value.data)||
       value.data>UINTPTR_MAX-size_t(value.count)*sizeof(uintptr_t))
        return fail(error,"Visitor Quest diagnostic ToonManager is updating or array bounds differ");
    out=value;return true;
}

bool read(npc_census::Memory& memory,uintptr_t image,uintptr_t region,json& output,std::string& error) {
    Header before,after;if(!header(memory,image,region,before,error))return false;
    std::vector<uintptr_t> toons(size_t(before.count));
    if(before.count&&!memory.copy(before.data,toons.data(),toons.size()*sizeof(uintptr_t),error))return false;
    std::set<uintptr_t> seen;std::set<int32_t> subscriber_ids;
    json samples=json::array();size_t unowned=0,progress_count=0,linked=0,expired=0;
    for(const auto toon:toons){
        uintptr_t ap=0,subscriber=0;
        if(!toon||!seen.insert(toon).second)
            return fail(error,"Visitor Quest diagnostic ToonManager contains null or duplicate receiver");
        if(!npc_census::field(memory,toon,0,ap,error)||!npc_census::field(memory,toon,0x940,subscriber,error))return false;
        if(ap!=image+0x1744e90)return fail(error,"Visitor Quest diagnostic Toon class differs");
        if(!subscriber){++unowned;continue;}
        uintptr_t backlink=0;int32_t id=-1;
        if(!npc_census::field(memory,subscriber,0x80,backlink,error)||!npc_census::field(memory,subscriber,0x20,id,error))return false;
        if(backlink!=toon||id<0||!subscriber_ids.insert(id).second)
            return fail(error,"Visitor Quest diagnostic Subscriber backlink or unique saved ID differs");
        visitor::State value;
        if(!visitor::read(memory,image,subscriber,
            [&](visitor::Kind kind,uintptr_t target,std::string& key,std::string& e){
                return identity(memory,image,kind,target,key,e);
            },value,error))return false;
        auto sample=project(value);sample["subscriber_id"]=id;
        progress_count+=sample.at("progress").size();
        for(const auto& row:sample.at("progress")){
            if(row.at("npc").at("state")=="live")++linked;
            if(row.at("npc").at("state")=="expired")++expired;
        }
        if(samples.size()<32)samples.push_back(std::move(sample));
    }
    if(!header(memory,image,region,after,error))return false;
    if(after!=before)return fail(error,"Visitor Quest diagnostic ToonManager header changed during read");
    std::vector<uintptr_t> actual(toons.size());
    if(after.count&&!memory.copy(after.data,actual.data(),actual.size()*sizeof(uintptr_t),error))return false;
    if(actual!=toons)return fail(error,"Visitor Quest diagnostic ToonManager membership changed during read");
    output={{"toons",toons.size()},{"subscribers",subscriber_ids.size()},{"unowned_toons",unowned},
        {"progress_count",progress_count},{"live_npc_links",linked},{"expired_npc_links",expired},
        {"samples",std::move(samples)},{"samples_truncated",subscriber_ids.size()>32},
        {"identity_scope","diagnostic_only"}};
    error.clear();return true;
}
}
