bool research_code(uintptr_t image,std::string& e){
    if(!tutorial_replication::start_intent::verify_ui(image,[&](uintptr_t p,void* out,size_t n,std::string& why){return memory.copy(p,out,n,why);},e,developer_staff::action_runtime::verify_entry)||
       !tutorial_replication::verify_pay_entry(image+0x265fc0,e))return false;
    constexpr tutorial_replication::start_intent::Body bodies[]={
        {0x6cc130,0xa0,"8bfb796d0d0ba965fc3739152c9911d634c75c3342d6a19a9dba29d4e8436c28"},
        {0x6cc7a0,0x12e0,"6e4f49bef5297c97fc9647e85954b81d024db376f9ee30e664510d7e1339f0fe"},
        {0x711ec0,0x510,"c69fffd82dcb1ee425b131d1512ab118cb48e336daf7fdb92c5e04f2a335ce98"},
        {0x711e10,0xb0,"2ea9613649d0caa5affa4be50978d44c6ef03bc75d91ab90974b3a2d4dd350a1"},
        {0xc4c350,0x160,"a5cb915cecd1ae32d26d2536b038593b69a8e5596772e6248d99a42fd8998e66"}
    };
    for(const auto& entry:bodies){std::vector<unsigned char> bytes(entry.size);
        if(!memory.copy(image+entry.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=entry.sha)return fail(e,"startingpoint Research UI body differs RVA="+std::to_string(entry.rva));}
    return true;
}
DWORD research_page(uintptr_t image,uintptr_t tabs){
    constexpr char name[]="{dev_tab_tech}";const BorrowedString page{name,sizeof(name)-1,{}};
    __try{reinterpret_cast<void(*)(void*,const void*)>(image+0x759f70)(reinterpret_cast<void*>(tabs),&page);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD research_select(uintptr_t image,uintptr_t grid,int32_t row){
    __try{reinterpret_cast<bool(*)(void*,int32_t)>(image+0x711ec0)(reinterpret_cast<void*>(grid),row);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
