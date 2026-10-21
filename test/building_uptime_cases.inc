void uptime_cases(){
    namespace up=mt2mp::game::building_uptime;
    Memory fixture_memory;std::string fixture_error;up::Receipt fixture_receipt;up::Native fixture_read;
    unsigned fixture_stores=0,fixture_reserves=0;bool fixture_guard_ok=true,fixture_store_ok=true,fixture_reserve_ok=true;
    auto fixture_reset=[&](const up::Values& values,int32_t capacity=10){
        fixture_memory.bytes.fill(0);fixture_memory.put(Memory::object+0xc60,Memory::image+0x17138d0);
        fixture_memory.put(Memory::object+0xc78,Memory::image+0x1749370);fixture_memory.put(Memory::object+0xc80,Memory::history);
        fixture_memory.put(Memory::object+0xc88,int32_t(values.size()));fixture_memory.put(Memory::object+0xc8c,capacity);
        fixture_memory.put(Memory::object+0xc90,int32_t{10});for(size_t i=0;i<values.size();++i)fixture_memory.put(Memory::history+i*4,values[i]);
        fixture_stores=fixture_reserves=0;fixture_guard_ok=fixture_store_ok=fixture_reserve_ok=true;
    };
    auto fixture_guard=[&](std::string& e){if(!fixture_guard_ok){e="fixture uptime current owner changed";return false;}return true;};
    auto fixture_compare=[&](uintptr_t p,const void* expected,const void* desired,size_t bytes,std::string& e){
        ++fixture_stores;std::vector<unsigned char> actual(bytes);
        if(!fixture_memory.copy(p,actual.data(),bytes,e))return false;
        if(!fixture_store_ok||std::memcmp(actual.data(),expected,bytes)){e="fixture uptime concurrent store";return false;}
        std::memcpy(fixture_memory.bytes.data()+p-Memory::base,desired,bytes);return true;
    };
    auto fixture_reserve=[&](uintptr_t array,int32_t capacity,std::string& e){
        ++fixture_reserves;need(array==Memory::object+0xc78&&capacity==10,"uptime used wrong original Reserve receiver");
        if(!fixture_reserve_ok){e="fixture uptime original Reserve failed";return false;}
        fixture_memory.put(Memory::object+0xc8c,capacity);return true;
    };
    auto fixture_apply=[&](const up::Values& before,const up::Values& target){return up::apply(fixture_memory,Memory::image,Memory::object,before,target,
        fixture_compare,fixture_reserve,fixture_guard,fixture_receipt,fixture_error);};
    const up::Values fixture_ten(10,1),fixture_nine(9,1);
    fixture_reset(fixture_ten);
    need(fixture_apply(fixture_ten,fixture_nine)&&fixture_receipt.confirmed&&fixture_receipt.attempted&&fixture_stores==1&&!fixture_reserves,
         "native308 Client extra uptime entry was not removed by exact Host count adoption");
    need(up::read(fixture_memory,Memory::image,Memory::object,fixture_read,fixture_error)&&fixture_read.values==fixture_nine,"uptime shrink complete readback");
    const auto fixture_bytes=fixture_memory.bytes;fixture_stores=0;
    need(fixture_apply(fixture_nine,fixture_nine)&&!fixture_receipt.attempted&&fixture_stores==0&&fixture_memory.bytes==fixture_bytes,"unchanged uptime mutated");
    fixture_reset({},0);need(fixture_apply({},fixture_ten)&&fixture_reserves==1&&fixture_receipt.confirmed,"uptime growth did not use original bounded Reserve");
    fixture_reset({1,0,1});need(fixture_apply({1,0,1},{0,1,0})&&fixture_stores==3,"same-length uptime values not replicated");
    for(unsigned fixture_bad=0;fixture_bad<6;++fixture_bad){
        fixture_reset({1});
        if(fixture_bad==0)fixture_memory.put(Memory::object+0xc60,uintptr_t{0});
        if(fixture_bad==1)fixture_memory.put(Memory::object+0xc78,uintptr_t{0});
        if(fixture_bad==2)fixture_memory.put(Memory::object+0xc90,int32_t{11});
        if(fixture_bad==3)fixture_memory.put(Memory::object+0xc88,int32_t{11});
        if(fixture_bad==4)fixture_memory.put(Memory::object+0xc8c,int32_t{0});
        if(fixture_bad==5)fixture_memory.put(Memory::history,int32_t{2});
        need(!fixture_apply({1},{0})&&!fixture_receipt.attempted,"invalid uptime native layout/value admitted");
    }
    fixture_reset({1});fixture_guard_ok=false;
    need(!fixture_apply({1},{0})&&!fixture_receipt.attempted&&fixture_error=="fixture uptime current owner changed","uptime owner failure mutated or lost cause");
    fixture_reset({1});fixture_store_ok=false;
    need(!fixture_apply({1},{0})&&fixture_receipt.attempted&&fixture_receipt.unknown&&fixture_error=="fixture uptime concurrent store","uptime CAS failure lost Unknown");
    fixture_reset({},0);fixture_reserve_ok=false;
    need(!fixture_apply({},fixture_ten)&&fixture_receipt.attempted&&fixture_receipt.unknown&&fixture_error=="fixture uptime original Reserve failed","uptime Reserve failure lost Unknown");
    fixture_reset({1});need(!fixture_apply({0},{1})&&!fixture_receipt.attempted,"wrong complete uptime before admitted");
    fixture_reset({1});need(!fixture_apply({1},{2})&&!fixture_receipt.attempted,"non-Boolean Host uptime admitted");
    fixture_reset(fixture_ten);need(!fixture_apply(fixture_ten,up::Values(11,1))&&!fixture_receipt.attempted,"uptime retention bound admitted an eleventh sample");
}
