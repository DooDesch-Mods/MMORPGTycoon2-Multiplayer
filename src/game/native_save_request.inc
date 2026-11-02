bool original_save_target(uintptr_t expected_mode,const std::string& filename,uint32_t kind,bool option,std::string& target,std::string& error){
    target.clear();if(!check_thread(error))return false;
    if((kind!=1&&kind!=2&&kind!=3)||filename.empty()||filename.size()>4096||filename.find('\0')!=std::string::npos)
        return fail(error,"original Save filename or kind differs");
    if(kind==3&&(filename!="periodic"||option))return fail(error,"Periodic Save requires its original fixed filename/options");
    auto* mode=singleton(mode_slot);
    if(!in_game()||reinterpret_cast<uintptr_t>(mode)!=expected_mode)
        return fail(error,"original Save active native Mode changed");
    int saves=-1;if(!read_at(mode,0x30c,saves)||saves!=0)
        return fail(error,"original Save requires idle native count actual="+std::to_string(saves)+" required=0");
    std::string directory;if(!read_game_string(mode,0x2e0,directory)||directory.empty())
        return fail(error,"original Save directory unavailable");
    target=(kind==1&&option?std::string{}:directory+"/")+filename+".vrz";
    error.clear();return true;
}
bool capture_original_save(uintptr_t expected_mode,const std::string& filename,uint32_t kind,bool option,const std::string& expected_target,std::string& error){
    std::string target;if(!original_save_target(expected_mode,filename,kind,option,target,error))return false;
    if(target!=expected_target)return fail(error,"original Save immutable target changed");
    ClockState clock;if(!read_clock(clock,error))return false;
    if(!clock.paused||clock.time_scale!=0)return fail(error,"original Save requires the prepared paused Clock");
    const auto exception=call_owned_save(reinterpret_cast<void*>(expected_mode),filename.c_str(),kind==3?0x4a0580:kind==1?0x4a0490:0x4a1b40,option,error);
    if(exception)return fail(error,"original Save failed: native exception="+std::to_string(exception)+(error.empty()?std::string{}:" cause="+error));
    int saves=-1;if(!read_at(reinterpret_cast<void*>(expected_mode),0x30c,saves)||saves<0||saves>1||(kind==2&&saves!=0))
        return fail(error,"original Save returned with invalid native count actual="+std::to_string(saves)+" kind="+std::to_string(kind)+"; no replay");
    error.clear();return true;
}
bool request_original_save(const std::string& filename,bool blocking,std::string& error){
    if(!check_thread(error))return false;
    if(filename.rfind("MPTest",0)!=0||filename.size()>64||filename.find_first_of("\"\\/\r\n\t")!=std::string::npos||filename.find('\0')!=std::string::npos)
        return fail(error,"owned native Save probe requires a bounded MPTest filename");
    auto* mode=singleton(mode_slot);int saves=-1;
    if(!in_game()||!read_at(mode,0x30c,saves)||saves!=0)return fail(error,"owned native Save probe requires active idle Mode");
    const auto command=std::string(blocking?"ImmediateAutosave \"":"Autosave \"")+filename+"\"";
    const auto exception=call_string(0x4a2f90,mode,command.c_str());
    if(exception)return fail(error,"original Save command failed: native exception="+std::to_string(exception)+"; no replay");
    error.clear();return true;
}

namespace {
initial_release::CallReceipt periodic_save_input(uintptr_t module,void* window){
    initial_release::Record record{};initial_release::CallReceipt out{};
    __try {
        reinterpret_cast<initial_release::RecordCtor>(module+0x967370)(&record,"Save");out.constructed=true;
        out.label_confirmed=reinterpret_cast<initial_release::RecordEquals>(module+0x1113d80)(&record,"Save");
        if(out.label_confirmed){
            out.attempted=true;
            out.accepted=reinterpret_cast<initial_release::UiCommand>(module+0x65f6c0)(window,&record);
            out.returned=true;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){out.seh_code=GetExceptionCode();}
    if(out.constructed){
        out.cleanup_attempted=true;
        __try {reinterpret_cast<initial_release::RecordDtor>(module+0x9676a0)(&record);out.cleanup_confirmed=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){out.cleanup_exception=GetExceptionCode();}
    }
    return out;
}
bool periodic_save_code(uintptr_t module,std::string& error){
    struct Pin {uintptr_t rva;size_t bytes;const char* hash;};
    static constexpr Pin pins[]={
        {0x967370,416,"2a6e4111f91e2c3cecb2aa35b99ed8d71f22a75f8300694f7d9ba6b086c01daa"},
        {0x9676a0,160,"33000088d5ae9e353738b283b76f13a641243aa996c7952714c57d6a07cc170f"},
        {0x1113d80,272,"dee54a7232d3aef27b73436213be00e802ba3cb997d4fb09a5f41f38468fafcc"},
        {0x65f6c0,256,"05107f590dde263e89f590318c98cbf9a969c64f8fadd1e105a7922041aa79e5"},
        {0x65f590,304,"c3f350e4a5b7521616bb6d973d01dae240538519d7401ff7449de4c4cfcbf44c"},
    };
    for(const auto& pin:pins){
        const auto entry=module+pin.rva;
        if(!readable(reinterpret_cast<void*>(entry),pin.bytes))return fail(error,"periodic Save original body unreadable RVA="+std::to_string(pin.rva));
        std::string cause;
        if(!hooks::framework::verify_original_hash(entry,reinterpret_cast<void*>(entry),pin.bytes,pin.hash,cause))
            return fail(error,"periodic Save original body differs RVA="+std::to_string(pin.rva)+": "+cause);
    }
    return true;
}
}
bool request_original_periodic_save(std::string& error){
    if(!check_thread(error))return false;
    auto* mode=singleton(mode_slot);int saves=-1;
    if(!in_game()||!read_at(mode,0x30c,saves)||saves!=0)return fail(error,"owned periodic Save probe requires active idle Mode");
    void* window=nullptr;
    if(!read_at(mode,0x90,window)||!has_vtable(window,0x1712560))
        return fail(error,"periodic Save Mode-owned AutosaveWindow unavailable or class differs");
    for(const size_t offset:{size_t{0x4f8},size_t{0x508},size_t{0x510}}){
        void* pane=nullptr;
        if(!read_at(window,offset,pane)||!readable(pane,sizeof(uintptr_t)))
            return fail(error,"periodic Save AutosaveWindow pane unavailable offset="+std::to_string(offset));
    }
    if(!periodic_save_code(image.load(),error))return false;
    // The original UI command calls SetMode(Saving), which calls PeriodicAutosave
    // from the executable. The Framework retains and orders that native request.
    const auto receipt=periodic_save_input(image.load(),window);
    if(!receipt.constructed||!receipt.label_confirmed||!receipt.attempted||!receipt.returned||!receipt.accepted||
       !receipt.cleanup_confirmed||receipt.seh_code||receipt.cleanup_exception)
        return fail(error,"periodic Save original UI command failed: constructed="+std::to_string(receipt.constructed)+
            " label="+std::to_string(receipt.label_confirmed)+" attempted="+std::to_string(receipt.attempted)+
            " returned="+std::to_string(receipt.returned)+" accepted="+std::to_string(receipt.accepted)+
            " exception="+std::to_string(receipt.seh_code)+" cleanup="+std::to_string(receipt.cleanup_confirmed)+
            " cleanup_exception="+std::to_string(receipt.cleanup_exception)+"; no replay");
    void* current=nullptr;int state=-1;
    if(!in_game()||singleton(mode_slot)!=mode||!read_at(mode,0x90,current)||current!=window||
       !has_vtable(window,0x1712560)||!read_at(window,0x4e8,state)||state!=1)
        return fail(error,"periodic Save original UI command returned with changed owner or non-saving window; no replay");
    error.clear();return true;
}

namespace {
uint32_t periodic_countdown_show(uintptr_t entry,void* window){
    __try{reinterpret_cast<void(*)(void*,bool,bool)>(entry)(window,true,false);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool request_original_periodic_countdown(std::string& error){
    if(!check_thread(error))return false;
    auto* mode=singleton(mode_slot);int saves=-1;void* window=nullptr;
    if(!in_game()||!read_at(mode,0x30c,saves)||saves!=0||!read_at(mode,0x90,window)||!has_vtable(window,0x1712560))
        return fail(error,"periodic countdown requires current idle Mode-owned AutosaveWindow");
    uint8_t visible=0,locked=0;
    if(!read_at(window,0x45f,visible)||!read_at(window,0x4ec,locked)||visible||locked)
        return fail(error,"periodic countdown requires hidden and unlocked original Window");
    const auto module=image.load();
    struct Pin {uintptr_t rva;size_t bytes;const char* hash;};
    static constexpr Pin pins[]={
        {0x81ec20,64,"6017edee8911e17d4541a597fa2ff4240b5652a187d885a5d9ef24b56ce25f57"},
        {0x65fa60,544,"572d1de21a8e165d7adfab777252f7f47ee483d529a87e15ca47a3483297ee4e"},
        {0x81e4e0,1856,"de12dc1850699c368d22c5df83fe6b2d37836d1ab26f78e019f22deb9162b371"},
    };
    for(const auto& pin:pins){std::string cause;const auto entry=module+pin.rva;
        if(!readable(reinterpret_cast<void*>(entry),pin.bytes)||!hooks::framework::verify_original_hash(entry,reinterpret_cast<void*>(entry),pin.bytes,pin.hash,cause))
            return fail(error,"periodic countdown original body differs RVA="+std::to_string(pin.rva)+": "+cause);
    }
    uintptr_t table=0,show=0,on_show=0,update=0;
    if(!read_at(window,0,table)||!read_at(reinterpret_cast<void*>(table),0x1d0,show)||!read_at(reinterpret_cast<void*>(table),0xb0,on_show)||
       !read_at(reinterpret_cast<void*>(table),0xd8,update)||show!=module+0x81ec20||on_show!=module+0x65fa60||update!=module+0x65fc80)
        return fail(error,"periodic countdown original Window virtual entries differ");
    const auto exception=periodic_countdown_show(show,window);
    if(exception)return fail(error,"periodic countdown original Show failed exception="+std::to_string(exception)+"; no replay");
    void* after_window=nullptr;int state=-1;float countdown=0;
    if(!in_game()||singleton(mode_slot)!=mode||!read_at(mode,0x90,after_window)||after_window!=window||
       !read_at(window,0x45f,visible)||!read_at(window,0x4ec,locked)||!read_at(window,0x4e8,state)||!read_at(window,0x4e4,countdown)||
       visible!=1||locked!=0||state!=0||countdown!=20.0f)
        return fail(error,"periodic countdown original Show returned with changed owner or countdown; no replay");
    error.clear();return true;
}

bool original_export_request(const std::string& nonce,uint32_t kind,bool option,uintptr_t& mode,std::string& filename,std::string& error){
    mode=0;filename.clear();if(!check_thread(error))return false;
    if((kind!=1&&kind!=2)||(nonce.size()!=36||nonce.find_first_not_of("0123456789abcdef-")!=std::string::npos))return fail(error,"Client export original Save kind or nonce invalid");
    std::string folder;if(!read_active_world_folder(folder,error))return false;
    mode=reinterpret_cast<uintptr_t>(singleton(mode_slot));filename="snapshot-"+nonce;
    if(kind==1&&option)filename="user/save/"+folder+"/"+filename;
    std::string target;return original_save_target(mode,filename,kind,option,target,error);
}
