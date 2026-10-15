namespace display_native {
bool verify_code(std::string& e){
    constexpr start_intent::Body bodies[]={
        {0x6c6660,0x1510,"46b9d00b1c84175c482c8fb5abe82502c39ee36e3a4299df195b0198106a9932"},
        {0x25ed50,0x10,"4b43567bf3359cff608bb04a5de1af65e583922cb44b96bbcaeeb2d74535576f"},
        {0x2642a0,0x510,"0b10dbcecf6fce21cea41662a4787f490894f6061b1073b2010beda59591558a"},
        {0xc6c620,0x150,"1405f3da1ddaafa39b05530c3c562bf439e7813cc763935ad9731dd1095eff90"},
        {0xa94360,0x120,"6376258870bccef03faf08a6651149ce5bc065e1665f819fbae62a9bbf16d521"},
        {0xc6c9b0,0x60,"79286ae0d1b13d0b256e0340f6c15f3a886a2783b69d0eba22604e395caee9a2"},
        {0x11b2b00,0xb0,"e6155e49c38d380ed513ce15ead8505762a5e50d6f1bde2e9985ec668fb95524"},
        {0x11bd1e0,0x20,"4a8cc79ce002b866698caded8eaea996814366638d34ce5cdf3ccb0394dc95e5"},
        {0xa16f10,0xc0,"0558e10498c849f04b3af18716eca5ac8deef6e1b82fb12ad27263fd9ff8b1cb"},
        {0x882400,0x20,"a1e7466620d4751c1710c99b60eab7e69b5a73b25513153912c31c652e76aa4a"},
        {0xa1ab30,0x200,"08e2bb77467685d89095838882d33a865532f193254ccaa63952aaf386095b72"},
        {0x701bb0,0x60,"8c8b1efdcdab5b6b3911c18630259bc3128339474bb6718eac1b34f826a823c4"},
        {0x7f80a0,0x80,"71c4b1105268ad20e522a8a6e4e93aa0afede1a62423e25dc2b8986656c1b01c"},
        {0x7070f0,0x40,"9d4d2a9eccef39702ffa7f761b7531153e63f1ee68f25b7d120c4730cdd3bda1"},
        {0x706f40,0x170,"93b51318b70337c58fec761d086ac5e2155166b2a2e4bf8c484197b90180463a"},
        {0x716cf0,0x110,"b087765f9b5b3dce92c281d17ed6d66469db7b2b81af1b867c7bbc1cd7ce1af6"}
    };
    for(const auto& entry:bodies){std::vector<unsigned char> bytes(entry.size);
        if(!memory.copy(image+entry.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=entry.sha)return fail(e,"Staff display original body differs RVA="+std::to_string(entry.rva));}
    return true;
}
// Hidden windows and inactive pages need no catalogue, Clock or cell scan.
bool visible_staff(uintptr_t window,std::string& e){
    uint8_t visible=0;if(!at(window,0x45f,visible,e))return false;
    if(visible!=1)return fail(e,"Staff display Window hidden");
    uintptr_t tabs=0,view=0,active=0;
    return at(window,0x540,tabs,e)&&at(tabs,0x3b8,view,e)&&page(view,window,"{dev_tab_staff}",active,e);
}
void post(uintptr_t window){
    if(!cache||cache->context.role!=action_session::Role::Client)return;
    bool attempted=false;
    try{
        if(!native_fault.empty()){unknown(native_fault);return;}
        std::string e;
        if(GetCurrentThreadId()!=mainthread){defer("Staff display UpdateUI caller is not bound Mainthread");return;}
        if(!visible_staff(window,e)){defer(e);return;}
        ++receipt.post_calls;
        Calls calls{*cache,window};
        const bool success=staff_display::render(cache->target,calls,attempted,e);
        if(!native_fault.empty()){unknown(native_fault);return;}
        if(!success){if(attempted)unknown(e);else defer(e);return;}
        if(receipt.deferred)LOG_INFO("[TutorialStaffDisplay] native cells recovered and confirmed");
        receipt.rendered=true;receipt.deferred=false;receipt.cause.clear();++receipt.confirmed_renders;
    }catch(const std::exception& cause){
        const auto error=std::string("Staff display posthook: ")+cause.what();
        if(attempted)unknown(error);else defer(error);
    }
}
}
void display_update(void* window,float delta){
    // The original owns row construction, ordinary local state and layout.
    // Presentation failure never skips it or enters the simulation fault gate.
    original_display(window,delta);
    ++display_native::receipt.original_calls;
    display_native::post(reinterpret_cast<uintptr_t>(window));
}
