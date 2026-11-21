bool staff_ui_code(uintptr_t image,std::string& e){
    if(!tutorial_replication::start_intent::verify_ui(image,[&](uintptr_t p,void* out,size_t n,std::string& why){return memory.copy(p,out,n,why);},e,developer_staff::action_runtime::verify_entry))return false;
    constexpr tutorial_replication::start_intent::Body bodies[]={
        {0x6cc130,0xa0,"8bfb796d0d0ba965fc3739152c9911d634c75c3342d6a19a9dba29d4e8436c28"},
        {0x261eb0,0x70,"0edc90eef4950db7c88082ea25c26609b3829b1b7d67850d352e3d51ffe24797"},
        {0x7ba400,0x970,"142ffdc08910b648016ca14aeb3c3a6a3fb834905c6715ad680789f70f1e6565"},
        {0x7bb3c0,0x530,"956098ed6fed256e05518f25106e68ca7e0c7155d6636f66852331f5895e9a67"},
        {0x7bada0,0x5d0,"a9dc679df285daa78f58eaab4313ff6e005d98ecb4ccf29efa7b8c8df8eabb79"},
        {0x6cba00,0x730,"0f95602104e401f2afe8bdc398bf548fe78d51f36e913bdd512105c706ae7478"}
    };
    for(const auto& entry:bodies){std::vector<unsigned char> bytes(entry.size);
        if(!memory.copy(image+entry.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=entry.sha)return fail(e,"startingpoint Staff UI body differs RVA="+std::to_string(entry.rva));}
    return true;
}
DWORD staff_page(uintptr_t image,uintptr_t tabs){
    constexpr char name[]="{dev_tab_staff}";const BorrowedString page{name,sizeof(name)-1,{}};
    __try{reinterpret_cast<void(*)(void*,const void*)>(image+0x759f70)(reinterpret_cast<void*>(tabs),&page);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD staff_done(uintptr_t image,uintptr_t lounge,bool& done){
    __try{done=reinterpret_cast<bool(*)(void*)>(image+0x261eb0)(reinterpret_cast<void*>(lounge));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
