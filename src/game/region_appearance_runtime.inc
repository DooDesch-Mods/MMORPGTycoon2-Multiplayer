bool appearance_read_native(Runtime& r,uintptr_t region,action::RegionAppearance& out,std::string& e){
    action::RegionAppearance v;uint8_t enabled=0;
    if(!at(r,region,0x840,v.tint,e)||!at(r,region,0x870,v.sky,e)||!at(r,region,0x8dc,v.light,e)||
       !at(r,region,0x8ec,v.ambient,e)||!at(r,region,0x8fc,enabled,e)||!at(r,region,0x900,v.particles,e)||
       !at(r,region,0x904,v.particle_color,e))return false;
    if(enabled>1||v.particles<0||v.particles>6||bool(enabled)!=(v.particles!=0))return fail(e,"region native particle type/enabled differs");
    v.particles_enabled=enabled!=0;
    for(const auto* values:{&v.tint,&v.light,&v.ambient,&v.particle_color})
        for(float x:*values)if(!std::isfinite(x))return fail(e,"region native color is nonfinite");
    for(float x:v.sky)if(!std::isfinite(x))return fail(e,"region native sky coefficient is nonfinite");
    out=v;return true;
}
uint32_t appearance_picker(uintptr_t image,uintptr_t picker,uintptr_t& value){
    __try{value=uintptr_t(reinterpret_cast<const void*(__fastcall*)(void*)>(image+0x836380)(reinterpret_cast<void*>(picker)));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t appearance_slider(uintptr_t image,uintptr_t slider,int32_t position,float& value){
    __try{value=reinterpret_cast<float(__fastcall*)(void*,int32_t)>(image+0x7b95a0)(reinterpret_cast<void*>(slider),position);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t appearance_fx(uintptr_t image,uintptr_t string,int32_t& value,bool& converted){
    __try{converted=reinterpret_cast<bool(__fastcall*)(int32_t*,const void*)>(image+0x26f170)(&value,reinterpret_cast<const void*>(string));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool appearance_control(Runtime& r,uintptr_t window,size_t offset,uintptr_t required,uintptr_t& control,std::string& e){
    uintptr_t ap=0;
    if(!at(r,window,offset,control,e)||!at(r,control,0,ap,e))return false;
    return ap==r.image+required||fail(e,"region appearance control class differs offset="+std::to_string(offset));
}
bool appearance_color(Runtime& r,uintptr_t window,size_t offset,std::array<float,4>& out,std::string& e){
    uintptr_t picker=0,value=0;
    if(!appearance_control(r,window,offset,0x16fae80,picker,e))return false;
    const auto seh=appearance_picker(r.image,picker,value);
    if(seh||value!=picker+0x12a8)return fail(e,"region original ColorPicker result differs SEH="+std::to_string(seh));
    if(!at(r,value,0,out,e))return false;
    for(float x:out)if(!std::isfinite(x))return fail(e,"region selected color is nonfinite");
    return true;
}
bool appearance_capture(Runtime& r,void* window,void* record,bool& handled,std::string& e){
    handled=false;
    static constexpr const char* commands[]{"ColorTint","ColorLight","ColorAmbient","SkyColor","FXChange","ColorRegionFX"};
    size_t which=6;
    for(size_t i=0;i<6;++i){bool match=false;const auto seh=label_is(r.image,record,commands[i],match);
        if(seh)return fail(e,"region appearance command read failed SEH="+std::to_string(seh));if(match){which=i;break;}}
    if(which==6)return true;
    handled=true;
    if(!property_admit(r,e))return false;
    uintptr_t region=0,widget=0;action::RegionPropertyIntent intent;LocalView view;json ui;
    intent.kind=action::RegionPropertyKind(uint8_t(action::RegionPropertyKind::Tint)+uint8_t(which));
    if(!at(r,uintptr_t(window),0x4e8,region,e)||!at(r,region,0x834,intent.id,e)||!identity(r,intent.id,region,view,e)||
       !at(r,region,0x370,widget,e)||!ui_readback::read(r.memory,r.image,view.world.selection,ui,e)||
       !ui_readback::validate(ui,r.image,widget,region,e,false)||ui.at("window").get<uintptr_t>()!=uintptr_t(window))
        return fail(e,e.empty()?"region appearance receiver differs from active selected InfoWindow":e);
    std::array<float,4> color{};
    if(which==3){
        std::array<float,4> top{},middle{},bottom{};
        if(!appearance_color(r,uintptr_t(window),0x5c8,top,e)||!appearance_color(r,uintptr_t(window),0x5d0,middle,e)||
           !appearance_color(r,uintptr_t(window),0x5d8,bottom,e))return false;
        // Same coefficient order as SetSkyPreview -> vsSplineColor::Set(bottom,middle,top).
        for(size_t i=0;i<4;++i){intent.appearance_value[i]=bottom[i];intent.appearance_value[4+i]=middle[i]-bottom[i];
            intent.appearance_value[8+i]=top[i];intent.appearance_value[12+i]=middle[i]-top[i];}
    }else if(which==4){
        uintptr_t combo=0,value=0;int32_t selected=-1;bool converted=false;
        if(!appearance_control(r,uintptr_t(window),0x5e8,0x16d9b30,combo,e))return false;
        auto seh=combo_get(r.image,combo,value);if(!seh)seh=appearance_fx(r.image,value,selected,converted);
        if(seh||!converted||selected<0||selected>6)return fail(e,"region original environment particle conversion failed SEH="+std::to_string(seh));
        intent.appearance_value[0]=float(selected);
    }else{
        const size_t offset=which==0?0x5a0:which==1?0x5a8:which==2?0x5c0:0x5f0;
        if(!appearance_color(r,uintptr_t(window),offset,color,e))return false;
        if(which==1||which==2){uintptr_t slider=0;int32_t position=0;float scale=0;
            if(!appearance_control(r,uintptr_t(window),which==1?0x5b0:0x5b8,0x1787a30,slider,e)||!at(r,slider,0x4d0,position,e))return false;
            const auto seh=appearance_slider(r.image,slider,position,scale);
            if(seh||!std::isfinite(scale))return fail(e,"region original light slider conversion failed SEH="+std::to_string(seh));
            for(size_t i=0;i<3;++i)color[i]*=scale;color[3]=1;
        }
        std::copy(color.begin(),color.end(),intent.appearance_value.begin());
    }
    for(float x:intent.appearance_value)if(!std::isfinite(x))return fail(e,"region captured appearance coefficient is nonfinite");
    action::RegionAppearance before;
    if(!appearance_read_native(r,region,before,e))return false;
    if(action::appearance_matches(intent,before))return true;
    return property_capture(r,intent,region,e);
}
uint32_t appearance_original(uintptr_t image,uintptr_t region,const action::RegionPropertyIntent& intent,bool& returned){
    __try{
        uintptr_t rva=0;
        switch(intent.kind){
        case action::RegionPropertyKind::Tint:rva=0x352980;break;
        case action::RegionPropertyKind::Light:rva=0x3531d0;break;
        case action::RegionPropertyKind::Ambient:rva=0x353260;break;
        case action::RegionPropertyKind::Sky:rva=0x350280;break;
        case action::RegionPropertyKind::Particles:
            // Original RegionInfoWindow branch 797d13..797d26 has no setter or event.
            *reinterpret_cast<int32_t*>(region+0x900)=int32_t(intent.appearance_value[0]);
            *reinterpret_cast<uint8_t*>(region+0x8fc)=intent.appearance_value[0]!=0;break;
        case action::RegionPropertyKind::ParticleColor:
            // Original ColorRegionFX branch 797d91..797d95 copies exactly one color.
            std::memcpy(reinterpret_cast<void*>(region+0x904),intent.appearance_value.data(),16);break;
        default:return 0xE0010008;
        }
        if(rva)reinterpret_cast<void(__fastcall*)(void*,const void*)>(image+rva)(reinterpret_cast<void*>(region),intent.appearance_value.data());
        returned=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
