namespace lifecycle_native {
bool verify_code(std::string& e){
    constexpr start_intent::Body bodies[]={
        {0x4f3b00,0x1b0,"eba7184a0b036f7aa9cb0544806dda9c6893ad2110f41d96a080fc199c7ad28d"},
        {0x4fceb0,0x1d0,"35875591663d53f5f4c99f398db97d2c159ea33ba70084d40d1df0cf9aae8fff"},
        {0x4f4130,0x230,"c279d2ec5c5169c87ddaf491c0a95113e4d932bc5946f431b3f19cc9cd3e64e5"},
        {0x4f4060,0xd0,"241c3d8cfeff9e6c4403da5e11d54b513a52b480504f30b7fbea81df59771c05"},
        {0x4f6130,0x110,"64c3a193bb4be2290bb4b79e09024939d8e14178ad10b3918feb896386268d6d"},
        {0x4f56b0,0x50,"2034a4bb1c334012efe7eee40da27b02d02d67e6e51e9f1fea4d79b4df754241"},
        {0x525780,0x10,"e64b03213dbaae494025927687f8eac6b91764e549d4c4724f8e1ed9ae30ea38"},
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
        {0x4f4130,0x230,"c279d2ec5c5169c87ddaf491c0a95113e4d932bc5946f431b3f19cc9cd3e64e5"},
        {0x4f4060,0xd0,"241c3d8cfeff9e6c4403da5e11d54b513a52b480504f30b7fbea81df59771c05"},
        {0x4f6130,0x110,"64c3a193bb4be2290bb4b79e09024939d8e14178ad10b3918feb896386268d6d"},
        {0x4f56b0,0x50,"2034a4bb1c334012efe7eee40da27b02d02d67e6e51e9f1fea4d79b4df754241"},
        {0x4fc380,0xa0,"684dfda6bfeed76c738eaa043d10e69631293954cba9570ed831595a9a39abb8"},
        {0x525780,0x10,"e64b03213dbaae494025927687f8eac6b91764e549d4c4724f8e1ed9ae30ea38"}
    };
    for(const auto& entry:bodies){std::vector<unsigned char> bytes(entry.size);
        if(!memory.copy(image+entry.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=entry.sha)return fail(e,"tutorial lifecycle transition body differs RVA="+std::to_string(entry.rva));}
    return true;
}
}
