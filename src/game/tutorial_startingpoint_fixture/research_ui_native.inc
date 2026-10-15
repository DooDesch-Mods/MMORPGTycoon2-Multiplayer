bool research_code(uintptr_t image,std::string& e){
    if(!tutorial_replication::start_intent::verify_ui(image,[&](uintptr_t p,void* out,size_t n,std::string& why){return memory.copy(p,out,n,why);},e,developer_staff::action_runtime::verify_entry)||
       !tutorial_replication::verify_pay_entry(image+0x262ec0,e))return false;
    constexpr tutorial_replication::start_intent::Body bodies[]={
        {0x6c4d10,0xa0,"5a691aaf4a4c5a42d1312aab315a219f3bbd41a19d59328c40c1ff8ad8241f2d"},
        {0x6c5380,0x12e0,"91cb5142872063581f6518e2a49b60a11c317e275755dc20d4ad8ccde07186a1"},
        {0x70a9d0,0x510,"6aa253964ce9974688fa75c2022de5e71374ca5c7aaecd8b759809dd2cb6fc61"},
        {0x70a920,0xb0,"19b6b364be74709aa474b61085dbf7f953af87eab39523ac97d7cbb2729e32b4"},
        {0xc3faa0,0x160,"2409e63c7d9780917d81876f68ace10ae49aa01398ba2ddfc6a5a301721b290a"}
    };
    for(const auto& entry:bodies){std::vector<unsigned char> bytes(entry.size);
        if(!memory.copy(image+entry.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=entry.sha)return fail(e,"startingpoint Research UI body differs RVA="+std::to_string(entry.rva));}
    return true;
}
DWORD research_page(uintptr_t image,uintptr_t tabs){
    constexpr char name[]="{dev_tab_tech}";const BorrowedString page{name,sizeof(name)-1,{}};
    __try{reinterpret_cast<void(*)(void*,const void*)>(image+0x7524f0)(reinterpret_cast<void*>(tabs),&page);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD research_select(uintptr_t image,uintptr_t grid,int32_t row){
    __try{reinterpret_cast<bool(*)(void*,int32_t)>(image+0x70a9d0)(reinterpret_cast<void*>(grid),row);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
