namespace {
bool style_catalog(Runtime& r,std::vector<std::string>& out,std::string& e) {
    // GetTerrainStyleCount returns 9; GetTerrainStyleName reads the canonical ComboBox values at stride 0x80.
    out.clear();std::set<std::string> unique;
    for(size_t i=0;i<9;++i){std::string name;if(!text(r,r.image+0x1a317e0+i*0x80,name,e))return false;
        if(!unique.insert(name).second)return fail(e,"region terrain style catalog has duplicate key index="+std::to_string(i));out.push_back(name);}
    return true;
}
bool validate_styles(Runtime& r,const std::array<std::string,4>& styles,std::string& e) {
    std::vector<std::string> catalog;if(!style_catalog(r,catalog,e))return false;
    for(size_t i=0;i<4;++i)if(std::find(catalog.begin(),catalog.end(),styles[i])==catalog.end())
        return fail(e,"region style target not in original catalog index="+std::to_string(i)+" actual="+styles[i]);
    return true;
}
uint32_t combo_get(uintptr_t image,uintptr_t combo,uintptr_t& result) {
    __try {result=uintptr_t(reinterpret_cast<const void*(*)(void*)>(image+0x6ab190)(reinterpret_cast<void*>(combo)));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool style_controls(Runtime& r,uintptr_t window,std::array<uintptr_t,4>& controls,std::array<std::string,4>& selected,json& receipt,std::string& e) {
    receipt=json::array();
    for(size_t i=0;i<4;++i){uintptr_t ap=0,value=0;
        if(!at(r,window,0x560+i*8,controls[i],e))return false;
        if(!controls[i])return fail(e,"region UI terrain_type"+std::to_string(i)+" actual=0 required=nonzero");
        if(!at(r,controls[i],0,ap,e))return false;
        // The complete-object constructor stores this address point, including the Itanium +16 header.
        if(ap!=r.image+0x16d9b30)return fail(e,"region UI combo address point index="+std::to_string(i)+" actual="+std::to_string(ap-r.image)+" required="+std::to_string(0x16d9b30));
        const auto seh=combo_get(r.image,controls[i],value);
        if(seh)return fail(e,"region original ComboBox GetSelection SEH="+std::to_string(seh));
        if(!text(r,value,selected[i],e))return false;
        receipt.push_back({{"index",i},{"object",controls[i]},{"selected",selected[i]}});
    }
    return true;
}
uint32_t label_is(uintptr_t image,void* record,const char* label,bool& match) {
    __try {match=reinterpret_cast<bool(*)(const void*,const char*)>(image+0x1113d80)(record,label);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool appearance_capture(Runtime&,void*,void*,bool&,std::string&);
bool styles_command_entry(void* window,void* record) {
    auto* r=published.load(std::memory_order_acquire);
    if(!r->ready||!net::session_wire::connected())return r->original_ui.load(std::memory_order_acquire)(window,record);
    try {
        bool appearance_handled=false;std::string appearance_error;
        if(!appearance_capture(*r,window,record,appearance_handled,appearance_error)){rejected(*r,appearance_error);return false;}
        if(appearance_handled)return true;
    }catch(const std::exception& e){rejected(*r,std::string("region appearance capture exception: ")+e.what());return false;}
    bool apply=false;const auto seh=label_is(r->image,record,"TerrainApply",apply);
    if(seh){rejected(*r,"region UI command label SEH="+std::to_string(seh));return false;}
    if(!apply)return r->original_ui.load(std::memory_order_acquire)(window,record);
    try {
        std::string e;const auto controller=typed_action_runtime::status();
        if(GetCurrentThreadId()!=r->thread||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||r->pending||property_pending()){rejected(*r,"region TerrainApply requires idle bound Controller");return false;}
        uintptr_t region=0,widget=0;action::RegionIntent intent;LocalView view;
        if(!at(*r,uintptr_t(window),0x4e8,region,e)||!at(*r,region,0x834,intent.id,e)||!read(*r,intent.id,region,view,e)||!at(*r,region,0x370,widget,e)){rejected(*r,e);return false;}
        json state,controls;std::array<uintptr_t,4> combo{};
        if(!ui_readback::read(r->memory,r->image,view.world.selection,state,e)||!ui_readback::validate(state,r->image,widget,region,e,false)||
           state.at("window").get<uintptr_t>()!=uintptr_t(window)){rejected(*r,e.empty()?"region TerrainApply receiver differs from active window":e);return false;}
        if(!style_controls(*r,uintptr_t(window),combo,intent.styles,controls,e)||!validate_styles(*r,intent.styles,e)){rejected(*r,e);return false;}
        r->last_ui={{"command","TerrainApply"},{"ui_state",state},{"controls",controls},{"captured",true}};
        // Defer the whole original mutation scope until the Host orders it.
        r->pending=intent;r->pending_snapshot=net::current_snapshot_id();r->pending_binding={view.world.origin.world_epoch,view.world.origin.generation};++r->captured;return true;
    }catch(const std::exception& e){rejected(*r,std::string("region TerrainApply capture exception: ")+e.what());return false;}
}
uint32_t invoke_styles(uintptr_t image,uintptr_t region,const NativeString* styles,uintptr_t random,const uint64_t* authority_random,Operation& receipt) {
    // The original TerrainApply branch uses static EditManager calls and four SetStyle calls.
    static constexpr char label[]="region terrain types change";
    NativeString description{label,sizeof(label)-1,{}};
    uint32_t exception=0;
    __try {
        if(authority_random)std::memcpy(reinterpret_cast<void*>(random),authority_random,32);
        if(reinterpret_cast<bool(*)()>(image+0x264c30)()) {
            reinterpret_cast<void(*)(const void*)>(image+0x265300)(&description);receipt.edit_opened=true;
        }
        for(int32_t i=0;i<4;++i){reinterpret_cast<void(*)(void*,int32_t,const void*,bool)>(image+0x355c90)(reinterpret_cast<void*>(region),i,styles+i,true);++receipt.style_calls;}
        if(receipt.edit_opened){receipt.edit_end_attempted=true;reinterpret_cast<void(*)()>(image+0x265650)();receipt.edit_ended=true;}
        receipt.returned=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();}
    // Match the native branch's exceptional EndEdit cleanup, with no retry after EndEdit itself fails.
    if(receipt.edit_opened&&!receipt.edit_end_attempted){
        receipt.edit_end_attempted=true;
        __try {reinterpret_cast<void(*)()>(image+0x265650)();receipt.edit_ended=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){receipt.edit_cleanup_exception=GetExceptionCode();}
    }
    return exception;
}
bool install_styles(Runtime& r,std::string& e) {
    constexpr unsigned char expected[]{0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x48};
    std::array<unsigned char,sizeof expected> actual{};
    if(!r.memory.copy(r.image+0x797850,actual.data(),actual.size(),e))return false;
    if(std::memcmp(expected,actual.data(),actual.size()))return fail(e,"region UICommand native entry bytes differ");
    UICommand original=nullptr;
    const auto created=MH_CreateHook(reinterpret_cast<void*>(r.image+0x797850),reinterpret_cast<void*>(&styles_command_entry),reinterpret_cast<void**>(&original));
    if(created!=MH_OK||!original)return fail(e,"region UICommand hook create status="+std::to_string(created));
    r.original_ui.store(original,std::memory_order_release);
    const auto enabled=MH_EnableHook(reinterpret_cast<void*>(r.image+0x797850));
    if(enabled!=MH_OK)return fail(e,"region UICommand hook enable status="+std::to_string(enabled));
    return true;
}
uint32_t invoke_styles_ui(uintptr_t image,uintptr_t window,const uintptr_t* controls,const NativeString* values,void* record,UiReceipt& receipt) {
    __try {
        for(size_t i=0;i<4;++i){
            reinterpret_cast<void(*)(void*,const void*,bool)>(image+0x6ab830)(reinterpret_cast<void*>(controls[i]),values+i,true);
            const auto* selected=reinterpret_cast<const NativeString*(*)(void*)>(image+0x6ab190)(reinterpret_cast<void*>(controls[i]));
            if(!selected||selected->length!=values[i].length||std::memcmp(selected->data,values[i].data,values[i].length))return 0xE0010003;
        }
        reinterpret_cast<void(*)(void*,const char*)>(image+0x967370)(record,"TerrainApply");receipt.constructed=true;
        receipt.called=true;receipt.accepted=reinterpret_cast<UICommand>(image+0x797850)(reinterpret_cast<void*>(window),record);receipt.returned=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool styles_ui(const std::string& token,int32_t region_id,const std::array<std::string,4>& styles,json& out,std::string& e) {
    out=json::object();auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||GetCurrentThreadId()!=r->thread||!net::session_wire::connected())return fail(e,"region styles UI requires enabled connected Mainthread");
    if(token.empty()||token.size()>64||!validate_styles(*r,styles,e))return fail(e,"region styles UI input invalid: "+e);
    const auto controller=typed_action_runtime::status();
    if(!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||r->pending)return fail(e,"region styles UI requires idle Controller");
    if(r->ui_attempts.contains(token))return fail(e,"region styles UI token already attempted; inspect receipt before retry");
    LocalView before;if(!read(*r,region_id,0,before,e))return false;
    uintptr_t widget=0,ap=0,owner=0;
    if(!at(*r,before.region,0x370,widget,e)||!at(*r,widget,0,ap,e)||ap!=r->image+0x17024b0||!at(*r,widget,0x178,owner,e)||owner!=before.region)return fail(e,"region styles UI actual RegionWidget differs: "+e);
    r->ui_attempts.insert(token);out["selection_attempted"]=true;
    const auto selection=select_ui(r->image,before.world.selection,widget);out["selection_exception"]=selection;
    if(selection)return fail(e,"region styles SelectProp/SetInfoWindow SEH="+std::to_string(selection));
    json state;if(!ui_readback::read(r->memory,r->image,before.world.selection,state,e))return false;
    out["ui_state"]=state;r->last_ui={{"token",token},{"ui_state",state}};
    if(!ui_readback::validate(state,r->image,widget,before.region,e,false))return false;
    const auto window=state.at("window").get<uintptr_t>();std::array<uintptr_t,4> controls{};std::array<std::string,4> selected{};
    if(!style_controls(*r,window,controls,selected,out["controls_before"],e))return false;
    std::array<NativeString,4> values{};for(size_t i=0;i<4;++i)values[i]={styles[i].data(),styles[i].size(),{}};
    alignas(8) unsigned char record[0x78]{};UiReceipt receipt;const auto captured=r->captured;
    const auto native=invoke_styles_ui(r->image,window,controls.data(),values.data(),record,receipt);
    const auto cleanup=receipt.constructed?cleanup_record(r->image,record,receipt):0;
    out["command_attempted"]=receipt.called;out["command_returned"]=receipt.returned;out["command_accepted"]=receipt.accepted;
    out["record_cleaned"]=receipt.cleaned;out["native_exception"]=native;out["cleanup_exception"]=cleanup;
    if(native||cleanup||!receipt.returned||!receipt.accepted||!receipt.cleaned)return fail(e,"region actual TerrainApply failed SEH="+std::to_string(native)+" cleanup="+std::to_string(cleanup)+" cause="+r->cause);
    const bool once=r->captured==captured+1&&r->pending&&r->pending->id==region_id&&r->pending->preset.empty()&&r->pending->styles==styles;
    out["captured_once"]=once;if(!once)return fail(e,"region TerrainApply did not reach exact UI capture: "+r->cause);
    LocalView after;if(!read(*r,region_id,before.region,after,e))return false;
    if(after.state!=before.state)return fail(e,"region client TerrainApply mutated before Host authority");
    out["local_state_unchanged"]=true;out["pending_intent"]=true;return true;
}
