namespace zone_edit_identity_fixture {
namespace identity58=mt2mp::game::zone_edit_identity;
namespace read58=mt2mp::game::zone_edit_read;
namespace census58=mt2mp::game::dungeon_zone_roster;
namespace poly58=mt2mp::game::ui_terrain_runtime::poly;
using Current58=mt2mp::game::world_zone_runtime::Current;
struct Memory58 {
    std::map<uintptr_t,unsigned char> bytes58;
    uintptr_t mutation58=0;unsigned occurrence58=0,seen58=0;
    template<class T>void put(uintptr_t address,const T& value){
        const auto* bytes=reinterpret_cast<const unsigned char*>(&value);
        for(size_t index=0;index<sizeof value;++index)bytes58[address+index]=bytes[index];
    }
    bool read(uintptr_t address,void* destination,size_t count,std::string& error){
        if(address==mutation58&&++seen58==occurrence58)bytes58[address]^=1;
        auto* bytes=static_cast<unsigned char*>(destination);
        for(size_t index=0;index<count;++index){const auto found=bytes58.find(address+index);
            if(found==bytes58.end()){error="Zone identity fixture original field unreadable";return false;}
            bytes[index]=found->second;
        }
        return true;
    }
    bool copy(uintptr_t address,void* destination,size_t count,std::string& error){return read(address,destination,count,error);}
};
struct Fixture58 {
    static constexpr uintptr_t image58=0x140000000,manager58=0x200000,root58=0x300000,region58=0x400000,slots58=0x500000;
    static constexpr const char* operation58="11111111-1111-4111-8111-111111111111";
    Memory58 memory58;std::map<uint64_t,uintptr_t> registry58;std::vector<Current58> before58;
    uint64_t failed_find58=0;
    Fixture58(){
        for(size_t index=0;index<5;++index){const int32_t type=index==4?1:int32_t(index);
            const auto key=type==1?std::string(64,'a')+":world:zone:"+std::to_string(index==4?18:17):
                std::string(64,'a')+":world:zone-type:"+std::to_string(type)+":root:67:child:0";
            Current58 row{key,"root",0x600000+index*0x10000,root58,region58,manager58,0x100000007+index*uint64_t(0x100000000),type};
            install58(row,index);before58.push_back(row);
        }
        roster58(before58);
    }
    void install58(const Current58& row,size_t ordinal){
        const auto zone=row.object,geometry=uintptr_t(0xa00000+ordinal*0x10000);
        memory58.put(zone,image58+0x1745ce0);memory58.put(zone+0x18,row.uid);
        memory58.put(zone+0x170,row.root);memory58.put(zone+0x178,row.region);memory58.put(zone+0x24c,row.type);
        memory58.put(zone+0x250,int32_t(3));memory58.put(zone+0x360,int32_t(9));
        memory58.put(zone+0x560,int32_t(17));memory58.put(zone+0x56c,int32_t(23));
        memory58.put(zone+0x2f0,image58+0x16e0140);memory58.put(zone+0x330,image58+0x16eb080);
        memory58.put(zone+0x338,geometry+0x100);memory58.put(zone+0x340,int32_t(1));memory58.put(zone+0x344,int32_t(1));
        memory58.put(geometry+0x100,geometry+0x200);memory58.put(geometry+0x200,image58+0x16fef20);
        memory58.put(geometry+0x240,image58+0x1746160);memory58.put(geometry+0x248,geometry+0x300);
        memory58.put(geometry+0x250,int32_t(3));memory58.put(geometry+0x254,int32_t(3));
        const std::array<std::array<uint32_t,3>,3> section{{{0x3f800000,0,0x40000000},{0x40400000,0,0x40800000},{0x40a00000,0,0x40c00000}}};
        memory58.put(geometry+0x300,section);
        memory58.put(zone+0x308,geometry+0x400);memory58.put(geometry+0x400,poly58::VectorView{geometry+0x500,geometry+0x518,geometry+0x518});
        memory58.put(geometry+0x500,poly58::VectorView{geometry+0x600,geometry+0x630,geometry+0x630});
        const std::array<mt2mp::action_session::TerrainPoint,3> path{{{-10,20},{30,40},{50,-60}}};memory58.put(geometry+0x600,path);
        registry58[row.uid]=row.object;
    }
    void roster58(const std::vector<Current58>& rows){
        memory58.put(manager58,image58+0x16fe170);memory58.put(manager58+0xf0,region58);
        memory58.put(region58+0x968,manager58);memory58.put(region58+0x70,root58);
        for(int32_t type=0;type<4;++type){
            const auto array=manager58+0x18+size_t(type)*0x30,slots=slots58+size_t(type)*0x1000;
            int32_t count=0;
            for(const auto& row:rows)if(row.type==type){memory58.put(slots+size_t(count)*8,row.object);++count;}
            memory58.put(array,image58+0x172e1e0);memory58.put(array+8,slots);
            memory58.put(array+0x10,count);memory58.put(array+0x14,count+1);
        }
    }
    bool find58(uint64_t uid,uintptr_t& object,std::string& error){
        if(uid==failed_find58&&uid){error="Zone identity fixture original FindUID failed";return false;}
        const auto found=registry58.find(uid);object=found==registry58.end()?0:found->second;return true;
    }
    bool census_read58(census58::Census& out,std::string& error){
        auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return memory58.read(object+offset,&value,sizeof value,cause);};
        auto find=[&](uint64_t uid,uintptr_t& object,std::string& cause){return find58(uid,object,cause);};
        return census58::read(at,find,image58,manager58,root58,region58,out,error);
    }
    bool reconcile58(const std::vector<Current58>& prior,const census58::Census& current,uintptr_t born,uint64_t uid,
                     const std::map<uint64_t,Current58>& restored,std::vector<Current58>& out,std::string& error){
        auto find=[&](uint64_t value,uintptr_t& object,std::string& cause){return find58(value,object,cause);};
        return identity58::reconcile(prior,current,operation58,born,uid,restored,find,out,error);
    }
    bool authored58(const std::vector<Current58>& owners,std::vector<read58::Row>& out,std::string& error){
        auto find=[&](uint64_t uid,uintptr_t& object,std::string& cause){return find58(uid,object,cause);};
        return read58::read(memory58,image58,owners,find,out,error);
    }
};
inline unsigned run58(){
    unsigned comparisons58=0;std::string cause58;
    const auto need58=[&](bool okay,const char* label){++comparisons58;if(!okay)throw std::runtime_error(std::string(label)+": "+cause58);};
    Fixture58 sample58;census58::Census raw58;std::vector<Current58> aliases58;std::vector<read58::Row> values58;
    need58(sample58.census_read58(raw58,cause58),"Initial complete four-array census rejected");
    need58(raw58.arrays[0].members.size()==1&&raw58.arrays[1].members.size()==2&&raw58.arrays[2].members.size()==1&&raw58.arrays[3].members.size()==1,
        "Identity fixture omitted a native Zone array");
    need58(sample58.reconcile58(sample58.before58,raw58,0,0,{},aliases58,cause58),"Current survivor aliases rejected");
    need58(sample58.authored58(aliases58,values58,cause58)&&values58.size()==5,"Actual complete authored reader omitted a current Zone");
    for(const auto& row:values58){
        need58(row.content.type==row.owner.type&&row.content.density==3&&row.content.level==9&&row.content.monster_type==17&&row.content.constructed_day==23,
            "Authored capture lost type/scalars/constructed day");
        need58(row.content.sections.size()==1&&row.content.sections[0].size()==3&&row.content.sections[0][2][2]==0x40c00000&&
            row.shape==mt2mp::action_session::TerrainShape{{{-10,20},{30,40},{50,-60}}},"Authored capture lost full section words or clipped coordinates");
    }
    {auto reordered=sample58.before58;std::swap(reordered[1],reordered[4]);sample58.roster58(reordered);
        need58(sample58.census_read58(raw58,cause58)&&sample58.reconcile58(sample58.before58,raw58,0,0,{},aliases58,cause58),"Native member reorder invalidated stable aliases");
        for(const auto& row:aliases58){const auto old=std::find_if(sample58.before58.begin(),sample58.before58.end(),[&](const auto& item){return item.uid==row.uid;});
            need58(old!=sample58.before58.end()&&*old==row,"Survivor canonical key or full UID was replaced after native reorder");}}
    Current58 born58{"action:"+std::string(Fixture58::operation58),"root",0xe00000,Fixture58::root58,Fixture58::region58,Fixture58::manager58,0x900000007,2};
    {Fixture58 fixture;fixture.install58(born58,5);auto rows=fixture.before58;rows.push_back(born58);fixture.roster58(rows);
        need58(fixture.census_read58(raw58,cause58)&&fixture.reconcile58(fixture.before58,raw58,born58.object,born58.uid,{},aliases58,cause58),"Actual single birth receipt rejected");
        const auto found=std::find(aliases58.begin(),aliases58.end(),born58);need58(found!=aliases58.end(),"Original returned birth did not retain the assigned action alias");
        need58(!fixture.reconcile58(fixture.before58,raw58,0,0,{},aliases58,cause58),"Unreceipted native birth accepted");
        need58(!fixture.reconcile58(fixture.before58,raw58,born58.object,uint32_t(born58.uid),{},aliases58,cause58),"Partial birth UID accepted");
        auto extra=born58;extra.object+=0x1000;extra.uid+=uint64_t(1)<<32;fixture.install58(extra,6);rows.push_back(extra);fixture.roster58(rows);
        need58(fixture.census_read58(raw58,cause58)&&!fixture.reconcile58(fixture.before58,raw58,born58.object,born58.uid,{},aliases58,cause58),"Extra native birth silently acquired an alias");}
    {Fixture58 fixture;need58(fixture.census_read58(raw58,cause58)&&!fixture.reconcile58(fixture.before58,raw58,born58.object,born58.uid,{},aliases58,cause58),"Missing original birth receipt succeeded");}
    {Fixture58 fixture;const auto retired=fixture.before58[2];auto survivors=fixture.before58;survivors.erase(survivors.begin()+2);fixture.roster58(survivors);
        need58(fixture.census_read58(raw58,cause58)&&!fixture.reconcile58(fixture.before58,raw58,0,0,{},aliases58,cause58),"Retirement accepted a still registered original full UID");
        fixture.registry58.erase(retired.uid);
        need58(fixture.reconcile58(fixture.before58,raw58,0,0,{},aliases58,cause58)&&aliases58.size()==4,"Original complete retirement rejected");
        const auto retired_aliases=aliases58;auto restored=retired;restored.object=0xf00000;fixture.install58(restored,7);auto live=survivors;live.push_back(restored);fixture.roster58(live);
        need58(fixture.census_read58(raw58,cause58)&&fixture.reconcile58(retired_aliases,raw58,0,0,{{retired.uid,retired}},aliases58,cause58),"Retained original full UID restoration rejected");
        need58(std::find(aliases58.begin(),aliases58.end(),restored)!=aliases58.end(),"Restoration replaced canonical alias or failed to adopt the actual new receiver");
        need58(!fixture.reconcile58(retired_aliases,raw58,0,0,{},aliases58,cause58),"Restoration succeeded without retained original authority");
        auto wrong=retired;wrong.uid=uint32_t(retired.uid);
        need58(!fixture.reconcile58(retired_aliases,raw58,0,0,{{retired.uid,wrong}},aliases58,cause58),"Original Make authority accepted only matching low UID bits");
        wrong=retired;wrong.root+=8;need58(!fixture.reconcile58(retired_aliases,raw58,0,0,{{retired.uid,wrong}},aliases58,cause58),"Original Make authority accepted a foreign root");}
    for(int mode58=0;mode58<8;++mode58){Fixture58 fixture;auto owners=fixture.before58;const auto target=owners[3];
        if(mode58==0)fixture.memory58.put(target.object+0x170,target.root+8);
        if(mode58==1)fixture.memory58.put(target.object+0x178,target.region+8);
        if(mode58==2)fixture.memory58.put(target.object+0x18,uint64_t(uint32_t(target.uid)));
        if(mode58==3)fixture.registry58[target.uid]=owners[0].object;
        if(mode58==4)owners[3].key=owners[0].key;
        if(mode58==5)owners[3].type=1;
        if(mode58==6)owners[3].object=owners[0].object;
        if(mode58==7)owners[3].uid=owners[0].uid;
        values58={{}};need58(!fixture.authored58(owners,values58,cause58)&&!cause58.empty()&&values58.empty(),"Foreign/partial/duplicate authored owner succeeded or retained stale output");}
    for(int mode58=0;mode58<3;++mode58){Fixture58 fixture;const auto target=fixture.before58[3];
        fixture.memory58.mutation58=mode58==0?target.object+0x250:mode58==1?uintptr_t(0xa30000+0x600):target.object+0x18;
        fixture.memory58.occurrence58=2;
        need58(!fixture.authored58(fixture.before58,values58,cause58)&&!cause58.empty()&&values58.empty(),"Actual authored repeat read accepted changing scalar, deep shape or full UID");}
    {Fixture58 fixture;fixture.memory58.bytes58.erase(fixture.before58[3].object+0x56c);
        need58(!fixture.authored58(fixture.before58,values58,cause58)&&cause58=="Zone identity fixture original field unreadable","Actual authored reader replaced original memory failure cause");}
    {Fixture58 fixture;fixture.failed_find58=fixture.before58[3].uid;
        need58(!fixture.authored58(fixture.before58,values58,cause58)&&cause58=="Zone identity fixture original FindUID failed","Actual authored reader replaced original FindUID cause");}
    {Fixture58 fixture;auto bad=fixture.before58;bad[0].manager+=8;need58(fixture.census_read58(raw58,cause58)&&!fixture.reconcile58(bad,raw58,0,0,{},aliases58,cause58),"Prior alias admitted a foreign actual manager");}
    return comparisons58;
}
}
inline unsigned zone_edit_identity_cases(){return zone_edit_identity_fixture::run58();}
