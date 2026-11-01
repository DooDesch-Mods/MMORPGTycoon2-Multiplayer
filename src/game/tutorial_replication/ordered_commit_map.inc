namespace ordered_commit_map {
struct Reader{bool read(uintptr_t p,void* out,size_t bytes,std::string& e){return memory.copy(p,out,bytes,e);}};
std::string digest(const std::string& text){return mt2mp::sha256(std::vector<unsigned char>(text.begin(),text.end()));}
bool code(std::string& e){
    struct Body{uintptr_t rva;size_t size;const char* sha;};
    for(const auto& body:std::array<Body,5>{{
        {0x330940,160,"616242adac1052d2f96e16649821ec7223d06908c9f51f21d0bd589544b638bc"},
        {0x330760,480,"b603cd8e86c82587f2f740a9e143d3df697132c3bb5c8e637da7a79aa3ef3d0f"},
        {0x354b20,1104,"3f37211f950a8cbdc28b6d11f9736825c2ac4edc601d1d12d7e336e967889473"},
        {0x58180,112,"717307a172a198ef8d1b6f28710cf5247907b63affdc2f3c5be7c33551a66dae"},
        {0x58b00,64,"6348d00a4fd0733e538ce3878538e83b4f72fbba6bffcfacc995ea25c4fdbd5c"}}}){
        std::vector<unsigned char> bytes(body.size);
        if(!memory.copy(image+body.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=body.sha)return fail(e,"Startingpoint original Map Bug body differs RVA="+std::to_string(body.rva));
    }
    return true;
}
bool read(const tutorial_development::StartingpointSnapshot& snapshot,nlohmann::json& out,std::string& e){
    const auto map=reinterpret_cast<uintptr_t>(snapshot.origin.map);
    region_saved_identity::Index regions;
    const auto field=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return at(p,offset,value,cause);};
    if(!region_saved_identity::capture(image,map,map+0x1c0,regions,field,e))return false;
    std::sort(regions.saved.begin(),regions.saved.end());
    auto rows=nlohmann::json::array();
    Reader reader;
    for(const auto& [id,region]:regions.saved){
        nlohmann::json bugs;uint64_t count=0;std::string digest_text;
        const auto hash=[&](const std::string& value,std::string& sha,std::string&){bugs=nlohmann::json::parse(value);sha=digest(value);return true;};
        if(!scenery_stroke_bugs::read(reader,image,region,hash,count,digest_text,e))return false;
        rows.push_back(nlohmann::json::array({id,std::move(bugs)}));
    }
    uintptr_t random=0;std::array<uint64_t,4> words{};int32_t extra=0;
    if(!at(image,0x164b250,random,e)||!at(random,0,words,e)||!at(random,0x20,extra,e))return false;
    out={{"regions",std::move(rows)},{"random",words},{"random_extra",extra}};return true;
}
}
