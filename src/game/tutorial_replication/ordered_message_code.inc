namespace ordered_message_native {
constexpr std::array<unsigned char,16> clear_entry={0x57,0x56,0x53,0x48,0x83,0xec,0x20,0x8b,0x51,0x18,0x48,0x89,0xce,0x85,0xd2,0x7e};
bool prepare(std::string& e){
    struct Pin {uintptr_t rva;size_t size;const char* sha;};
    const Pin pins[]={
        {0x14c66fc,76,"d9dfa4baf20e623d785c7ec429a691e6b6f08e0c70736e5d254826d27893fcbf"},
        {0x53a240,240,"f34476ce33d418b94deafc5162d49e4489e12a2ddd35586fb7c4687ee2040632"},
        {0x505200,2784,"f593145147ea092beb32dc28e56c45deb6373b26598a211eb58f1a22dc464d5e"},
        {0x504560,2784,"c2352dc641ec1a58c98e117a4f15ebed9cb10c651cf4fb3330c30482f56826f3"},
        {0x503500,48,"e13e059080cc6a068f93bca7119495cc8a358089356b20c333d04d5269b4db7b"},
        {0x4fc4a0,544,"e4c5f3d4f008710e18738ac97b340daf36aa59d35f66192d0b7c8eb018472ffd"},
        {0x4fceb0,464,"35875591663d53f5f4c99f398db97d2c159ea33ba70084d40d1df0cf9aae8fff"},
        {0x4f5dc0,624,"d3e2b6f68a1961242e410f52eaa21739ff076d2eb9e4d457ef028bdb54cda937"}};
    for(const auto& pin:pins){std::vector<unsigned char> bytes(pin.size);
        if(!memory.copy(image+pin.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=pin.sha)return fail(e,"Ordered tutorial native body differs RVA="+std::to_string(pin.rva));
    }
    clear_owner={image+0x53a240,reinterpret_cast<uintptr_t>(&clear),0,true,false};return true;
}
bool enabled(std::string& e){
    if(!clear_owner.original_verified||clear_owner.target!=image+0x53a240||!clear_owner.trampoline||
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
