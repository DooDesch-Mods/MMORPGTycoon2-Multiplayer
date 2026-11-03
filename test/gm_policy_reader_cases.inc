namespace reader_fixture {
std::map<uintptr_t,uint64_t> fixture_memory;
auto fixture_read=[](uintptr_t object,size_t offset,auto& value,std::string& error){const auto found=fixture_memory.find(object+offset);if(found==fixture_memory.end()){error="fixture guarded native read failed";return false;}value=static_cast<std::remove_reference_t<decltype(value)>>(found->second);return true;};
void array(uintptr_t owner,size_t offset,uintptr_t data,const std::vector<uintptr_t>& members,uint32_t capacity=4){fixture_memory[owner+offset]=data;fixture_memory[owner+offset+8]=members.size();fixture_memory[owner+offset+12]=capacity;for(size_t i=0;i<members.size();++i)fixture_memory[data+i*8]=members[i];}
void reset_spatial(){
    fixture_memory.clear();fixture_memory[6000+0x10]=7000;fixture_memory[6000+0x18]=2;fixture_memory[6000+0x28]=8000;fixture_memory[6000+0x30]=2;fixture_memory[6000+0x34]=31;
    array(6000,0x40,16000,{10});array(6000,0x58,17000,{10});
    fixture_memory[7000+0x20]=1;fixture_memory[7000+0x58]=1;fixture_memory[7000+0x5c]=1;array(7000,0x30,18000,{10});array(7000,0x48,19000,{1000+0x30});
    fixture_memory[7120+0x20]=uint32_t(-1);fixture_memory[7120+0x58]=1;fixture_memory[7120+0x5c]=1;array(7120,0x30,0,{},0);array(7120,0x48,0,{},0);
    const auto bucket=(uint32_t(10)*uint32_t(0x9e3779ff))>>31;fixture_memory[8000+size_t(bucket)*24+0x10]=15000;fixture_memory[15000]=10;fixture_memory[15008]=10;fixture_memory[15016]=0;
    fixture_memory[1000+0x488]=10;fixture_memory[10+8]=1000+0x30;fixture_memory[10+0x118]=1000;fixture_memory[10+0x124]=0;fixture_memory[10+0x128]=0;fixture_memory[10+0x12b]=1;
}
void run(){std::string error;std::array<uintptr_t,2> owners{};std::array<std::vector<int32_t>,2> members;
    fixture_memory={{5000+0x68,6000},{5000+0x70,7000}};array(6000,0x38,8000,{1000},64);array(7000,0x38,9000,{2000,3000},64);
    need(actual::managers(fixture_read,5000,{1000,2000,3000},owners,members,error),error);need(members[0]==std::vector<int32_t>{0}&&members[1]==std::vector<int32_t>({1,2}),"fixed-slot manager membership/order differs");
    fixture_memory[9000]=1000;need(!actual::managers(fixture_read,5000,{1000,2000,3000},owners,members,error),"cross-manager duplicate accepted");fixture_memory[9000]=2000;
    fixture_memory[9008]=9999;need(!actual::managers(fixture_read,5000,{1000,2000,3000},owners,members,error),"foreign manager object accepted");fixture_memory[9008]=3000;
    fixture_memory[7000+0x40]=51;need(!actual::managers(fixture_read,5000,{1000,2000,3000},owners,members,error),"manager member extent accepted");fixture_memory[7000+0x40]=2;
    fixture_memory[7000+0x44]=5000;need(!actual::managers(fixture_read,5000,{1000,2000,3000},owners,members,error),"manager capacity extent accepted");
    reset_spatial();std::vector<actual::SpatialModel> models;need(actual::spatial_models(fixture_read,{1000},models,error),error);need(models.size()==1&&models[0].object==10&&models[0].dynamic==1,"actual model-info metadata differs");
    fixture_memory[10+0x118]=2000;need(!actual::spatial_models(fixture_read,{1000},models,error),"foreign model entity accepted");fixture_memory[10+0x118]=1000;
    fixture_memory[10+0x128]=1;need(!actual::spatial_models(fixture_read,{1000},models,error),"owned external transform accepted for inline GM transform");fixture_memory[10+0x128]=0;
    need(actual::spatial_models(fixture_read,{1000},models,error),error);actual::Spatial before,expected,after;
    need(actual::spatial_read(fixture_read,6000,models,before,error),error);need(before.nodes.size()==2&&before.models.members==std::vector<uintptr_t>{10},"spatial owner/member census differs");
    need(actual::spatial_expected(before,models,{0},expected,error),error);need(expected.models.members.empty()&&expected.pending.members.empty()&&expected.nodes.at(0).models.members.empty()&&expected.nodes.at(0).render.members.empty()&&expected.nodes.at(0).total==0&&expected.nodes.at(1).total==0&&expected.nodes.at(1).transforms==0,"complete dynamic model removal or ancestor decrement omitted");
    fixture_memory[6000+0x48]=0;fixture_memory[6000+0x60]=0;fixture_memory[7000+0x38]=0;fixture_memory[7000+0x50]=0;fixture_memory[7000+0x58]=0;fixture_memory[7000+0x5c]=0;fixture_memory[7120+0x58]=0;fixture_memory[7120+0x5c]=0;
    const auto bucket=(uint32_t(10)*uint32_t(0x9e3779ff))>>31;fixture_memory[8000+size_t(bucket)*24+0x10]=0;
    for(auto field:{uintptr_t(10+8),uintptr_t(10+0x118),uintptr_t(10+0x124),uintptr_t(10+0x128),uintptr_t(10+0x12b),uintptr_t(15000),uintptr_t(15008),uintptr_t(15016)})fixture_memory.erase(field);
    need(actual::spatial_read(fixture_read,6000,models,after,error)&&after==expected,"spatial removal witness dereferenced freed model or missed membership");
    fixture_memory[7120+0x5c]=1;need(actual::spatial_read(fixture_read,6000,models,after,error)&&after!=expected,"stale ancestor count invisible to direct witness");
    reset_spatial();need(actual::spatial_models(fixture_read,{1000},models,error),error);fixture_memory[15016]=15000;need(!actual::spatial_read(fixture_read,6000,models,after,error),"hash cycle accepted");fixture_memory[15016]=0;
    fixture_memory[7120+0x20]=0;need(!actual::spatial_read(fixture_read,6000,models,after,error),"ancestor cycle accepted");fixture_memory[7120+0x20]=uint32_t(-1);
    fixture_memory[15008]=11;need(!actual::spatial_read(fixture_read,6000,models,after,error),"hash key/object mismatch accepted");fixture_memory[15008]=10;
    fixture_memory[6000+0x34]=32;need(!actual::spatial_read(fixture_read,6000,models,after,error),"invalid native hash shift accepted");fixture_memory[6000+0x34]=31;
    fixture_memory.erase(6000+0x10);need(!actual::spatial_read(fixture_read,6000,models,after,error)&&error=="fixture guarded native read failed","native read cause suppressed");
    fixture::reset();actual::Cascade changed;need(actual::expected({0,actual::Field::CheckLog,0,1},fixture::fixture_policies,fixture::fixture_cascade.value,changed,error)&&changed==fixture::fixture_after.value,"complete queue survivor/assignment/Lounge transform differs from original");
    auto malformed=fixture::fixture_cascade.value;malformed.managers[0]={0};need(!actual::expected({0,actual::Field::CheckLog,0,1},fixture::fixture_policies,malformed,changed,error),"duplicate Lounge membership survived before-effect validation");
    auto proof=real::GmPolicyProof{};proof.gm_sha256=std::string(64,'a');proof.cascade_sha256=std::string(64,'b');proof.protected_sha256=std::string(64,'c');proof.tutorial_sha256=std::string(64,'d');need(actual::codec::proof(actual::encode(proof))==proof,"complete policy proof codec lost canonical fields");
    auto incomplete=actual::encode(proof);incomplete.erase("tutorial_sha256");bool rejected=false;try{actual::codec::proof(incomplete);}catch(const std::exception&){rejected=true;}need(rejected,"partial ordered policy proof decoded");
}
}
