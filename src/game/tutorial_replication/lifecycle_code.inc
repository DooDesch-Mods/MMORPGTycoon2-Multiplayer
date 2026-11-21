namespace lifecycle_native {
bool verify_code(std::string& e){
    constexpr start_intent::Body bodies[]={
        {0x4f9a60,0x1b0,"b52a65ccadfd9c1144f2e8a114ad294697e12af756802c4a3caf855330967310"},
        {0x502e60,0x1d0,"1ea4bda6c3a9cb84210cc5061cf54acefc159e759bcac786d1f3cdf5fb34b370"},
        {0x4fa0b0,0x230,"5058a0f4fe23b42155de92e9a0d1d2aac622aca24fa2bad2741e3799f8495a63"},
        {0x4f9fe0,0xd0,"6c53ae5151781acb6ebcb3f8f56bff6c7ee87f0a0c7a0e4e9ecb41bb3816e47b"},
        {0x4fc0d0,0x110,"099f93b02269638473088fa9f845d60142dcf74d3b24ea23c1d5432046576569"},
        {0x4fb650,0x50,"c7eac7d90f3f18b51a4f58652cf2b0e992f6ef288952365c124c3e30dd1279f2"},
        {0x52bb70,0x10,"e64b03213dbaae494025927687f8eac6b91764e549d4c4724f8e1ed9ae30ea38"},
    };
    for(const auto& entry:bodies){std::vector<unsigned char> bytes(entry.size);
        if(!memory.copy(image+entry.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=entry.sha)return fail(e,"tutorial lifecycle original body differs RVA="+std::to_string(entry.rva));}
    return true;
}
bool verify_transition_code(std::string& e){
    auto copy=[](auto address,auto out,auto count,auto& error){return memory.copy(address,out,count,error);};
    if(!tutorial_sync::verify_owned_scenario_start_entry(owned_scenario_start,owned_scenario_start.target,copy,e)||
       !tutorial_sync::verify_owned_advance_entry(owned_advance,owned_advance.target,copy,e))return false;
    constexpr start_intent::Body bodies[]={
        {0x4fa0b0,0x230,"5058a0f4fe23b42155de92e9a0d1d2aac622aca24fa2bad2741e3799f8495a63"},
        {0x4f9fe0,0xd0,"6c53ae5151781acb6ebcb3f8f56bff6c7ee87f0a0c7a0e4e9ecb41bb3816e47b"},
        {0x4fc0d0,0x110,"099f93b02269638473088fa9f845d60142dcf74d3b24ea23c1d5432046576569"},
        {0x4fb650,0x50,"c7eac7d90f3f18b51a4f58652cf2b0e992f6ef288952365c124c3e30dd1279f2"},
        {0x502330,0xa0,"ff516b3fa91249110222075c7507aeada1770c83845cfec79c416bd377630a17"},
        {0x52bb70,0x10,"e64b03213dbaae494025927687f8eac6b91764e549d4c4724f8e1ed9ae30ea38"}
    };
    for(const auto& entry:bodies){std::vector<unsigned char> bytes(entry.size);
        if(!memory.copy(image+entry.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=entry.sha)return fail(e,"tutorial lifecycle transition body differs RVA="+std::to_string(entry.rva));}
    return true;
}
}
