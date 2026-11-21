bool finish_dungeon_maker(Runtime& runtime,json& receipt,std::string& error){
    host::ProcessMemory memory;uintptr_t manager=0,ap=0;
    if(!memory.copy(runtime.image+0x13eb340,&manager,8,error)||!manager||
       !memory.copy(manager,&ap,8,error)||ap!=runtime.image+0x1703e90){
        if(error.empty())error="Dungeon Building queued maker has no current original TaskManager";return false;
    }
    std::vector<unsigned char> bytes(480);
    if(!memory.copy(runtime.image+0x52d170,bytes.data(),bytes.size(),error)||
       !hooks::framework::verify_original_hash(runtime.image+0x52d170,bytes.data(),bytes.size(),
          "f1d69fa40fbf68371e1c5cfccd34e93320ba7316f75ac1e97313b061695e9897",error))return false;
    bool entered=false,returned=false;uint32_t exception=0;
    {
        live_host_state::apply_timing::PageReadScope original(live_host_state::apply_timing::PageBoundary::OriginalCall);
        exception=dungeon_build_calls::finish_tasks(runtime.image,manager,entered,returned);
    }
    receipt={{"attempted",entered},{"returned",returned},{"exception",exception}};
    if(exception||!returned){error="Dungeon Building original FinishAllTasks failed SEH="+std::to_string(exception);return false;}
    uintptr_t current=0;
    if(!memory.copy(runtime.image+0x13eb340,&current,8,error)||current!=manager||
       !memory.copy(manager,&ap,8,error)||ap!=runtime.image+0x1703e90){
        if(error.empty())error="Dungeon Building original TaskManager owner changed during drain";return false;
    }
    for(const size_t offset:{0x30,0x48,0x60,0x78,0x90,0xa8}){
        int32_t count=-1;
        if(!memory.copy(manager+offset,&count,sizeof count,error))return false;
        if(count){error="Dungeon Building original task drain incomplete offset="+std::to_string(offset)+" count="+std::to_string(count);return false;}
    }
    receipt["confirmed"]=true;return true;
}
