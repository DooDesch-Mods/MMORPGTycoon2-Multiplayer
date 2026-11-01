bool appearance_ui_intent(const action::RegionPropertyIntent& intent,std::string& e){
    if(!action::appearance_kind(intent.kind)||!intent.name.empty()||intent.type!=-1||intent.hit!=std::array<float,3>{}||intent.levels!=std::array<int32_t,2>{-1,-1})
        return fail(e,"Region appearance UI requires one appearance kind without unrelated intent fields");
    const size_t used=intent.kind==action::RegionPropertyKind::Sky?16:intent.kind==action::RegionPropertyKind::Particles?1:4;
    for(size_t i=0;i<16;++i)if(!std::isfinite(intent.appearance_value[i])||(i>=used&&intent.appearance_value[i]!=0))
        return fail(e,"Region appearance UI coefficients are nonfinite or contain unrelated values");
    if(intent.kind==action::RegionPropertyKind::Particles){const auto p=intent.appearance_value[0];
        if(p<0||p>6||p!=std::floor(p))return fail(e,"Region appearance UI particle kind outside native domain");}
    if(intent.kind==action::RegionPropertyKind::Sky)for(size_t i=0;i<4;++i){
        const float middle=intent.appearance_value[i]+intent.appearance_value[4+i];
        if(!std::isfinite(middle)||middle-intent.appearance_value[i]!=intent.appearance_value[4+i]||
           intent.appearance_value[8+i]-middle!=intent.appearance_value[12+i])
            return fail(e,"Region sky coefficients cannot be represented exactly by the original three color pickers");
    }
    return true;
}
uint32_t appearance_ui_color(uintptr_t image,uintptr_t picker,const std::array<float,4>& color){
    __try{reinterpret_cast<void(*)(void*,const void*)>(image+0x836e00)(reinterpret_cast<void*>(picker),color.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t appearance_ui_combo(uintptr_t image,uintptr_t combo,const NativeString& value){
    __try{reinterpret_cast<void(*)(void*,const void*,bool)>(image+0x6ab830)(reinterpret_cast<void*>(combo),&value,false);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t appearance_ui_command(uintptr_t image,uintptr_t window,const char* label,void* record,UiReceipt& receipt){
    __try{
        reinterpret_cast<void(*)(void*,const char*)>(image+0x967370)(record,label);receipt.constructed=true;
        receipt.called=true;receipt.accepted=reinterpret_cast<UICommand>(image+0x797850)(reinterpret_cast<void*>(window),record);
        receipt.returned=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool appearance_ui_controls(Runtime& r,uintptr_t window,const action::RegionPropertyIntent& intent,json& out,std::string& e){
    const auto which=size_t(uint8_t(intent.kind)-uint8_t(action::RegionPropertyKind::Tint));
    auto set_color=[&](size_t offset,const std::array<float,4>& color){
        for(float value:color)if(!std::isfinite(value))return fail(e,"Region original picker input is nonfinite after native light conversion");
        uintptr_t picker=0;if(!appearance_control(r,window,offset,0x16fae80,picker,e))return false;
        const auto seh=appearance_ui_color(r.image,picker,color);
        if(seh)return fail(e,"Region original ColorPicker SetColor failed SEH="+std::to_string(seh));
        std::array<float,4> actual{};if(!appearance_color(r,window,offset,actual,e))return false;
        return actual==color||fail(e,"Region original ColorPicker did not retain the requested color");
    };
    if(which==3){
        std::array<float,4> bottom{},middle{},top{};
        for(size_t i=0;i<4;++i){bottom[i]=intent.appearance_value[i];middle[i]=bottom[i]+intent.appearance_value[4+i];top[i]=intent.appearance_value[8+i];}
        if(!set_color(0x5d8,bottom)||!set_color(0x5d0,middle)||!set_color(0x5c8,top))return false;
    }else if(which==4){
        uintptr_t combo=0;std::string selected;
        if(!appearance_control(r,window,0x5e8,0x16d9b30,combo,e)||
           !text(r,r.image+0x1a307e0+size_t(intent.appearance_value[0])*0x20,selected,e))return false;
        NativeString value{selected.data(),selected.size(),{}};int32_t converted=-1;bool valid=false;
        const auto conversion=appearance_fx(r.image,uintptr_t(&value),converted,valid);
        if(conversion||!valid||converted!=int32_t(intent.appearance_value[0]))return fail(e,"Region native particle catalog conversion differs SEH="+std::to_string(conversion));
        const auto seh=appearance_ui_combo(r.image,combo,value);
        if(seh)return fail(e,"Region original particle ComboBox SetSelection failed SEH="+std::to_string(seh));
        out["particle_catalog_key"]=selected;
    }else{
        std::array<float,4> color{};std::copy_n(intent.appearance_value.begin(),4,color.begin());
        if(which==1||which==2){uintptr_t slider=0;int32_t position=0;float scale=0;
            if(!appearance_control(r,window,which==1?0x5b0:0x5b8,0x1787a30,slider,e)||!at(r,slider,0x4d0,position,e))return false;
            const auto seh=appearance_slider(r.image,slider,position,scale);
            if(seh||!std::isfinite(scale)||scale==0||color[3]!=1)return fail(e,"Region original light slider cannot represent requested color SEH="+std::to_string(seh));
            for(size_t i=0;i<3;++i)color[i]/=scale;
            out["native_light_scale"]=scale;
        }
        const size_t offset=which==0?0x5a0:which==1?0x5a8:which==2?0x5c0:0x5f0;
        if(!set_color(offset,color))return false;
    }
    action::RegionPropertyIntent observed;observed.kind=intent.kind;
    if(!appearance_read_controls(r,window,which,observed,e))return false;
    out["control_appearance_value"]=observed.appearance_value;
    return observed.appearance_value==intent.appearance_value||fail(e,"Original appearance controls do not represent the exact requested coefficients");
}
bool appearance_ui(Runtime& r,const std::string& token,const action::RegionPropertyIntent& intent,json& out,std::string& e){
    if(!appearance_ui_intent(intent,e))return false;
    if(properties.ui_attempts.contains(token))return fail(e,"Region appearance UI token already attempted; inspect receipt before retry");
    LocalView view;action::RegionPropertyState before;
    if(!property_read(r,intent.id,0,before,view,e))return false;
    action::RegionAppearance prior;if(!appearance_read_native(r,view.region,prior,e))return false;before.appearance=prior;
    if(action::appearance_matches(intent,prior))return fail(e,"Region appearance UI request is unchanged");
    uintptr_t widget=0,ap=0,owner=0;
    if(!at(r,view.region,0x370,widget,e)||!at(r,widget,0,ap,e)||ap!=r.image+0x17024b0||
       !at(r,widget,0x178,owner,e)||owner!=view.region)return fail(e,e.empty()?"Region appearance UI RegionWidget owner differs":e);
    constexpr std::array<unsigned char,7> color_code{0x57,0x56,0x53,0x48,0x83,0xec,0x50};std::array<unsigned char,7> code{};
    if(!r.memory.copy(r.image+0x836e00,code.data(),code.size(),e)||code!=color_code)return fail(e,e.empty()?"Region original ColorPicker SetColor bytes differ":e);
    properties.ui_attempts.insert(token);out["attempted"]=true;
    const auto captured=properties.captured;
    const auto selection=select_ui(r.image,view.world.selection,widget);out["selection_exception"]=selection;
    if(selection)return fail(e,"Region appearance original selection failed SEH="+std::to_string(selection));
    json ui;const bool read=ui_readback::read(r.memory,r.image,view.world.selection,ui,e);
    out["ui_state"]=ui;
    if(!read||!ui_readback::validate(ui,r.image,widget,view.region,e,false))return false;
    const auto window=ui.at("window").get<uintptr_t>();
    if(!appearance_ui_controls(r,window,intent,out,e))return false;
    if(properties.captured!=captured||properties.pending)return fail(e,"Region appearance control setup unexpectedly captured an action");
    static constexpr const char* commands[]{"ColorTint","ColorLight","ColorAmbient","SkyColor","FXChange","ColorRegionFX"};
    alignas(8) unsigned char record[0x78]{};UiReceipt receipt;
    const auto call=appearance_ui_command(r.image,window,commands[size_t(uint8_t(intent.kind)-uint8_t(action::RegionPropertyKind::Tint))],record,receipt);
    const auto cleanup=receipt.constructed?cleanup_record(r.image,record,receipt):0;
    out["command_attempted"]=receipt.called;out["command_returned"]=receipt.returned;out["command_accepted"]=receipt.accepted;out["record_cleaned"]=receipt.cleaned;
    if(call||cleanup||!receipt.returned||!receipt.accepted||!receipt.cleaned)return fail(e,"Region original appearance UICommand failed SEH="+std::to_string(call)+" cleanup="+std::to_string(cleanup)+" cause="+r.cause);
    const bool once=properties.captured==captured+1&&properties.pending&&*properties.pending==intent;out["captured_once"]=once;
    if(!once)return fail(e,"Region original appearance command did not capture exactly the requested intent: "+properties.cause);
    action::RegionPropertyState after;LocalView actual;action::RegionAppearance appearance;
    if(!property_read(r,intent.id,view.region,after,actual,e)||!appearance_read_native(r,view.region,appearance,e))return false;after.appearance=appearance;
    out["local_state_unchanged"]=after==before;out["pending_intent"]=true;
    return after==before||fail(e,"Region appearance UI changed native state before the consumed command");
}
