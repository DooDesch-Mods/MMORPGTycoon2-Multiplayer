namespace ordered_commit_map {
struct Reader{bool read(uintptr_t p,void* out,size_t bytes,std::string& e){return memory.copy(p,out,bytes,e);}};
std::string digest(const std::string& text){return mt2mp::sha256(std::vector<unsigned char>(text.begin(),text.end()));}
bool code(std::string& e){
    struct Body{uintptr_t rva;size_t size;const char* sha;};
    for(const auto& body:std::array<Body,5>{{
        {0x3349d0,160,"8eabd1d3dd263d49850e4d4880a19cf1f10fcc4067a4240c463d93d48371e467"},
        {0x3347f0,480,"bb0a3882db621fac953f9f4aabd1b9e4f875a61809d6a87665195d0a2205def5"},
        {0x355390,1392,"426bd7b13da4b2c5078af473f97ed39410fb4c441cd5ab6c2c32e3a9bddff2b4"},
        {0x58680,112,"fb106d69fbecb1c31f131bc58446b28d8274f388424711cba5dad72a42d4d395"},
        {0x59000,64,"de0a01121ead31a53c99a8588a4d80294f97763300e848fc4a3078fd5cd91883"}}}){
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
    if(!at(image,0x1650830,random,e)||!at(random,0,words,e)||!at(random,0x20,extra,e))return false;
    out={{"regions",std::move(rows)},{"random",words},{"random_extra",extra}};return true;
}
}
