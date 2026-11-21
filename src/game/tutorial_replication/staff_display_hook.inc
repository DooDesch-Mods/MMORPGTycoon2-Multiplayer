namespace display_native {
bool verify_code(std::string& e){
    constexpr start_intent::Body bodies[]={
        {0x6cda80,0x1510,"28285e1b032745e22ab8bfab2ac2e899b86fe454b3164c1b10b03551346a5978"},
        {0x261e50,0x10,"4b43567bf3359cff608bb04a5de1af65e583922cb44b96bbcaeeb2d74535576f"},
        {0x2673a0,0x510,"c7c0e8413c2aa641420bf7ed7e1ccd6b292610fda9e1ea87b56d6f2860dc043d"},
        {0xc77d50,0x150,"32cdc1b7a31f4a462783d34600e9b052eb7c7a1113d1d28a453c8b676084c417"},
        {0xa9f4a0,0x120,"8c7eda85ae5d77d7e13d725db91b75d301320423945ed0f5b8865617ac3b72cb"},
        {0xc780e0,0x60,"a35d5090fbc80f0b42406c0580c5c64cad3719b950d12cdfdf826e3204455466"},
        {0x11be4a0,0xb0,"a4efe58947d80e63e5d6ef578f2fcc1bcbbee2f949eb234f49e5c4143cfa9a08"},
        {0x11c8b80,0x20,"959c85086c72a0971d78253f37d70e041f67f69706a7872d0c0e387a720bf50d"},
        {0xa21c10,0xc0,"2d56fe7dc155d03ab64d29a4eff04317e77c854b18d405856c22437c17e6274b"},
        {0x88b1c0,0x20,"0acb1f4bf6d6a20c379d77227b4bcc91edd74d041eb0dd8d0f04e2f512763fb2"},
        {0xa25be0,0x200,"7d8cff17df2bfaae3cfd25c563cb7ebe93393f1e8ad9d8f703e5f2586d39c01f"},
        {0x7090a0,0x60,"a03e7fff28c4b40f8f1c7d80a04a2d331936fc0d64daa073cf55ce90ee11e695"},
        {0x8005f0,0x80,"4c59617bfa4468efadfca2b6788bfe763b412fda3ede2faac8de8ea2d83bde82"},
        {0x70e5e0,0x40,"be89b78e8a44a7ba018a0ade4949c10c27793060f7840a96d77bcac6e7fc41f8"},
        {0x70e430,0x170,"3ce0f5dba8af29d30830b2ce45ad49374a1d1b7bfcd070182102a01eda768759"},
        {0x71e280,0x110,"2e9c4ea63750a94d9f1741ce44cb0a37df37f6f6cc0f7b61f2172bd17f2cdd57"}
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
