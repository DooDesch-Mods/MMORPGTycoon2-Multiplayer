bool staff_ui_code(uintptr_t image,std::string& e){
    if(!tutorial_replication::start_intent::verify_ui(image,[&](uintptr_t p,void* out,size_t n,std::string& why){return memory.copy(p,out,n,why);},e,developer_staff::action_runtime::verify_entry))return false;
    constexpr tutorial_replication::start_intent::Body bodies[]={
        {0x6c4d10,0xa0,"5a691aaf4a4c5a42d1312aab315a219f3bbd41a19d59328c40c1ff8ad8241f2d"},
        {0x25edb0,0x70,"570f31fa2dead3d5677ab4bdecccb3d8c09e20433e500a523588b2de4c14c223"},
        {0x7b2160,0x970,"c03c93aa698caa05f663a176c403f6c4b531085eebb8bfb1a9f0a8cd64134b7d"},
        {0x7b3120,0x530,"8200153ed7ec51e0658b3538349b2a4f7e561fba82a2289fa405f79d1c6fd92b"},
        {0x7b2b00,0x5d0,"71291bf19795c5377b43b7d24c1e2079942f38f90f31f9e1510fb03482cf9bca"},
        {0x6c45e0,0x730,"ea87b4e2c05610ccc2065753f0101bbe49ca685db22177f06269abd40b988642"}
    };
    for(const auto& entry:bodies){std::vector<unsigned char> bytes(entry.size);
        if(!memory.copy(image+entry.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=entry.sha)return fail(e,"startingpoint Staff UI body differs RVA="+std::to_string(entry.rva));}
    return true;
}
DWORD staff_page(uintptr_t image,uintptr_t tabs){
    constexpr char name[]="{dev_tab_staff}";const BorrowedString page{name,sizeof(name)-1,{}};
    __try{reinterpret_cast<void(*)(void*,const void*)>(image+0x7524f0)(reinterpret_cast<void*>(tabs),&page);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD staff_done(uintptr_t image,uintptr_t lounge,bool& done){
    __try{done=reinterpret_cast<bool(*)(void*)>(image+0x25edb0)(reinterpret_cast<void*>(lounge));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
