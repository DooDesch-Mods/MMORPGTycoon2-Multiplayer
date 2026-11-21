namespace ordered_message_native {
constexpr std::array<unsigned char,16> clear_entry={0x57,0x56,0x53,0x48,0x83,0xec,0x20,0x8b,0x51,0x18,0x48,0x89,0xce,0x85,0xd2,0x7e};
bool prepare(std::string& e){
    struct Pin {uintptr_t rva;size_t size;const char* sha;};
    const Pin pins[]={
        {0x14cebcc,76,"342da6eeadef4898741e0a0d93edbac63b910e48ec043ccaa19fb58988e7a1a6"},
        {0x540750,240,"2b98b7cf86c7b015d2ad018280597722a0aa1e4812bd3c50cd00f33910060458"},
        {0x50b350,2784,"9e9e061fea2d333310b7e89423292606eda4787846f826905fdf4b87683d5417"},
        {0x50a6b0,2784,"d0a697cdc3a33eb5db4bf1f6716cdc5dcdd191bcf935a5fc3e250d8e0020ee1d"},
        {0x509650,48,"e13e059080cc6a068f93bca7119495cc8a358089356b20c333d04d5269b4db7b"},
        {0x502450,544,"21414b8349596e10db6b924f881e8601e1fe2aa4059ac00385a79d9dadb9f3f9"},
        {0x502e60,464,"1ea4bda6c3a9cb84210cc5061cf54acefc159e759bcac786d1f3cdf5fb34b370"},
        {0x4fbd60,624,"b7ddd6574e773053eb591fa60c2cd4fc7e7885947afc644db046ff55c88bbad9"}};
    for(const auto& pin:pins){std::vector<unsigned char> bytes(pin.size);
        if(!memory.copy(image+pin.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=pin.sha)return fail(e,"Ordered tutorial native body differs RVA="+std::to_string(pin.rva));
    }
    clear_owner={image+0x540750,reinterpret_cast<uintptr_t>(&clear),0,true,false};return true;
}
bool enabled(std::string& e){
    if(!clear_owner.original_verified||clear_owner.target!=image+0x540750||!clear_owner.trampoline||
       clear_owner.detour!=reinterpret_cast<uintptr_t>(&clear))return fail(e,"Ordered tutorial Clear hook owner incomplete");
    std::array<unsigned char,16> entry{};std::array<unsigned char,35> trampoline{};
    if(!memory.copy(clear_owner.target,entry.data(),entry.size(),e)||
       !memory.copy(clear_owner.trampoline,trampoline.data(),trampoline.size(),e))return false;
    if(entry[0]!=0xe9||std::memcmp(entry.data()+5,clear_entry.data()+5,11))return fail(e,"Ordered tutorial Clear owned entry differs");
    int32_t displacement=0;std::memcpy(&displacement,entry.data()+1,4);
    const auto relay=clear_owner.trampoline+21;
    if(int64_t(clear_owner.target+5)+displacement!=int64_t(relay))return fail(e,"Ordered tutorial Clear hook relay differs");
    constexpr unsigned char jump[]={0xff,0x25,0,0,0,0};uintptr_t continuation=0,detour=0;
    std::memcpy(&continuation,trampoline.data()+13,8);std::memcpy(&detour,trampoline.data()+27,8);
    if(std::memcmp(trampoline.data(),clear_entry.data(),7)||std::memcmp(trampoline.data()+7,jump,6)||
       continuation!=clear_owner.target+7||std::memcmp(trampoline.data()+21,jump,6)||detour!=clear_owner.detour)
        return fail(e,"Ordered tutorial Clear original trampoline differs");
    clear_owner.enabled=true;e.clear();return true;
}
}
