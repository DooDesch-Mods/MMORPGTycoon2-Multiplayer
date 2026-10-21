namespace subscriber_cheat_fixture {
struct Fixture {
    static constexpr uintptr_t image_base=0x10000000,manager_address=0x20000000,slots_address=0x30000000;
    static constexpr uintptr_t subscriber_address=0x40000000,toon_address=0x50000000;
    static constexpr uint64_t toon_uid=0x700000007;
    Memory memory;std::map<uint64_t,uintptr_t> registry;
    uintptr_t mutation_address=0;unsigned mutation_hits=0,mutation_occurrence=2;uint32_t mutation_kind=0;
    unsigned guarded=0,compared=0;bool stale_on_guard=false,fail_compare=false;
    Fixture(){
        memory.put(image_base+0x13df690,manager_address);memory.put(manager_address,image_base+0x1725f40);
        memory.put(manager_address+0x40,slots_address);memory.put(manager_address+0x48,int32_t(1));memory.put(manager_address+0x4c,int32_t(1));
        memory.put(slots_address,subscriber_address);memory.put(subscriber_address,image_base+0x16e75f0);
        memory.put(subscriber_address+0x20,int32_t(7));memory.put(subscriber_address+0x50,uint32_t(2));
        memory.put(subscriber_address+0x70,std::bit_cast<uint32_t>(4.5f));memory.put(subscriber_address+0xc8,uint8_t(0));memory.put(subscriber_address+0x125,uint8_t(0));
        memory.put(subscriber_address+0x80,toon_address);memory.put(toon_address,image_base+0x1744e90);
        memory.put(toon_address+0x18,toon_uid);memory.put(toon_address+0x940,subscriber_address);memory.put(toon_address+0x84c,uint32_t(1));
        registry[toon_uid]=toon_address;
    }
    template<class T>bool read(uintptr_t object,size_t offset,T& value,std::string& error){
        if(object+offset==mutation_address&&++mutation_hits==mutation_occurrence){
            if(mutation_kind==1)memory.put(toon_address+0x18,toon_uid^(uint64_t(1)<<32));
            if(mutation_kind==2)memory.put(subscriber_address+0x80,toon_address+0x1000);
            if(mutation_kind==3)memory.put(toon_address+0x84c,uint32_t(2));
        }
        return memory.read(object,offset,value,error);
    }
    bool sample(s::State& values,s::Owners& owners,std::string& error){
        auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return read(object,offset,value,cause);};
        auto copy=[&](uintptr_t address,void* out,size_t size,std::string& cause){return memory.copy(address,out,size,cause);};
        return s::read_all(at,copy,image_base,values,owners,error);
    }
    bool guard(const s::Owners& owners,size_t index,std::string& error){
        ++guarded;if(stale_on_guard)registry[toon_uid]=toon_address+0x1000;
        const auto& [object,row]=owners.rows[index];s::Value actual;s::ToonOwner local;
        auto at=[&](uintptr_t pointer,size_t offset,auto& value,std::string& cause){return read(pointer,offset,value,cause);};
        if(!s::read(at,image_base,object,row.id,actual,error,&local))return false;
        if(actual!=row.value||local!=owners.toons[index]){error="current Subscriber/Toon owner or value changed";return false;}
        if(local.object){const auto found=registry.find(local.uid);
            if(found==registry.end()||found->second!=local.object){error="current Toon full UID registry differs";return false;}}
        return true;
    }
    bool compare(uintptr_t address,unsigned size,uint32_t before,uint32_t after,std::string& error){
        ++compared;if(fail_compare){error="injected Toon cheat CAS failure";return false;}
        uint32_t actual=0;if(size!=4||!memory.copy(address,&actual,4,error)||actual!=before){if(error.empty())error="exact Toon cheat CAS prior differs";return false;}
        memory.put(address,after);return true;
    }
};
struct Receipt {bool attempted=false;uint32_t attempted_stores=0,stores=0;};
inline void run(){
    std::string error;s::State values;s::Owners owners;
    for(uint32_t classification=0;classification<4;++classification){Fixture fixture;fixture.memory.put(Fixture::toon_address+0x84c,classification);
        need(fixture.sample(values,owners,error),error);
        need(values.size()==1&&values[0].id==7&&values[0].value.toon_cheat_state==classification&&
            owners.toons.size()==1&&owners.toons[0].object==Fixture::toon_address&&owners.toons[0].uid==Fixture::toon_uid,
            "actual Toon cheat classification lost its saved Subscriber ID/full local UID owner");
        s::State decoded;need(s::decode(s::encode(values),decoded,error)&&decoded==values,"populated Toon cheat frame field failed exact roundtrip");
    }
    for(int mode=0;mode<6;++mode){Fixture fixture;
        if(mode==0)fixture.memory.put(Fixture::toon_address,Fixture::image_base+0x1744e91);
        if(mode==1)fixture.memory.put(Fixture::toon_address+0x18,uint64_t(0));
        if(mode==2)fixture.memory.put(Fixture::toon_address+0x940,Fixture::subscriber_address+8);
        if(mode==3)fixture.memory.put(Fixture::toon_address+0x84c,uint32_t(4));
        if(mode==4)fixture.memory.bytes.erase(Fixture::toon_address+0x84c);
        if(mode==5)fixture.memory.put(Fixture::toon_address+0x84c,uint32_t(UINT32_MAX));
        need(!fixture.sample(values,owners,error)&&!error.empty()&&values.empty()&&owners.rows.empty(),"invalid Toon cheat owner/state published a partial capture");
        if(mode==4)need(error=="missing fixture byte","Toon cheat read hid the original memory cause");
    }
    for(uint32_t mode=1;mode<4;++mode){Fixture fixture;fixture.mutation_address=mode==2?Fixture::subscriber_address+0x80:Fixture::toon_address+(mode==1?0x18:0x84c);fixture.mutation_kind=mode;
        need(!fixture.sample(values,owners,error)&&!error.empty(),"changing Toon full UID/receiver/classification was accepted during read");}
    Fixture fixture;need(fixture.sample(values,owners,error),error);const auto wire=s::encode(values);s::State decoded;
    for(const auto& invalid:{nlohmann::json(true),nlohmann::json(-1),nlohmann::json(1.0),nlohmann::json(4),nlohmann::json(uint64_t(UINT32_MAX)+1)}){
        auto bad=wire;bad[0]["toon_cheat_state"]=invalid;need(!s::decode(bad,decoded,error),"coerced or unsupported Toon cheat classification accepted");}
    {auto bad=wire;bad[0].erase("toon_cheat_state");need(!s::decode(bad,decoded,error),"complete Subscriber live row omitted Toon classification field");}
    auto target=values;target[0].value.toon_cheat_state=3;
    auto sample=[&](s::State& value,s::Owners& owner,std::string& cause){return fixture.sample(value,owner,cause);};
    auto guard=[&](const s::Owners& owner,size_t index,std::string& cause){return fixture.guard(owner,index,cause);};
    auto compare=[&](uintptr_t address,unsigned size,uint32_t before,uint32_t after,std::string& cause){return fixture.compare(address,size,before,after,cause);};
    Receipt receipt;need(s::adopt(target,sample,guard,compare,receipt,error),error);
    need(receipt.attempted&&receipt.attempted_stores==1&&receipt.stores==1&&fixture.guarded==1&&fixture.compared==1,
        "Host cheat classification was not adopted through one guarded exact native CAS");
    receipt={};fixture.guarded=fixture.compared=0;need(s::adopt(target,sample,guard,compare,receipt,error)&&!receipt.attempted&&!fixture.guarded&&!fixture.compared,"unchanged Toon cheat state replayed stores");
    auto missing=target;missing[0].value.toon_cheat_state.reset();
    need(!s::adopt(missing,sample,guard,compare,receipt,error)&&!receipt.attempted&&!fixture.compared&&error.find("Toon presence differs")!=std::string::npos,"missing original Toon lifetime was silently synthesized");
    target[0].value.toon_cheat_state=2;fixture.stale_on_guard=true;
    need(!s::adopt(target,sample,guard,compare,receipt,error)&&!receipt.attempted&&!fixture.compared&&error=="current Toon full UID registry differs","changed current Toon registry receiver was used for a store");
    fixture.stale_on_guard=false;fixture.registry[Fixture::toon_uid]=Fixture::toon_address;fixture.fail_compare=true;
    need(!s::adopt(target,sample,guard,compare,receipt,error)&&receipt.attempted&&receipt.attempted_stores==1&&!receipt.stores&&error=="injected Toon cheat CAS failure","failed Toon CAS was confirmed or its cause was lost");
    Fixture absent;absent.memory.put(Fixture::subscriber_address+0x80,uintptr_t(0));
    need(absent.sample(values,owners,error)&&!values[0].value.toon_cheat_state&&owners.toons[0]==s::ToonOwner{},"absent original Toon was invented during capture");
}
}
