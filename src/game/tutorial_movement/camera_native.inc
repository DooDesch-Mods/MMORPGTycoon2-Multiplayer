struct CameraPending {CameraInput input;uint64_t timestamp;};
std::vector<CameraPending> camera_pending;
bool camera_pin(std::string& error){
    struct Pin{uintptr_t rva;size_t size;const char* sha;};
    constexpr Pin pins[]={{0x5b6b0,432,"6ec762d7f147d9471a01893a356d5a268e27d68a09662c8c05906a9dc557ff29"},
        {0x5c1d0,48,"e0badd4f1fdab1c8fb86b117501f6e83ab1eca23f9f533dfe598276b8fd0e838"},
        {0x5c200,160,"f94929f1b72de1515aca6cfcf97be679db9052fa7c9fdf164a38859d8139ff2a"}};
    for(const auto& pin:pins){std::vector<unsigned char> bytes(pin.size);if(!memory.copy(sdl+pin.rva,bytes.data(),bytes.size(),error))return false;
        if(mt2mp::sha256(bytes)!=pin.sha)return reject(error,"tutorial camera original SDL ingress differs RVA="+std::to_string(pin.rva));}
    return true;
}
bool camera_button_state(Snapshot& out,std::string& error,bool releasing=false){
    uintptr_t input=0,ap=0;if(!at(image,0x13df990,input,error)||!at(input,0,ap,error))return false;
    if(ap!=image+0x1749430)return reject(error,"tutorial camera native Input owner differs");
    Array axes,controls;if(!array(input+0x48,0x17461c0,0x70,axes,error)||axes.count<=29)return reject(error,"tutorial camera rotation axis29 absent");
    const auto axis=axes.data+29*0x70;
    if(!array(axis+0x40,0x1746310,0x14,controls,error)||!at(axis,0x5c,out.axis,error))return false;
    uint8_t derived=0;if(!at(axis,0x63,derived,error))return false;
    if(derived||!std::isfinite(out.axis))return reject(error,"tutorial camera original rotation axis invalid");
    bool found=false;
    for(int32_t i=0;i<controls.count;++i){int32_t type=0,code=0;uint8_t modifier=0;
        const auto control=controls.data+size_t(i)*0x14;
        if(!at(control,0,type,error)||!at(control,4,code,error)||!at(control,8,modifier,error))return false;
        if(type==4&&code==3&&!modifier)found=true;
    }
    if(!found)return reject(error,"tutorial camera native axis29 lacks original RMB3 binding");
    const auto window=reinterpret_cast<SDL_Window*>(out.window);
    out.mouse_relative=SDL_GetWindowRelativeMouseMode(window);out.mouse_focus=SDL_GetMouseFocus()==window;
    out.mouse_capture=(SDL_GetWindowFlags(window)&SDL_WINDOW_MOUSE_CAPTURE)!=0;
    out.mouse_auto_capture=SDL_GetHintBoolean(SDL_HINT_MOUSE_AUTO_CAPTURE,true);
    const auto buttons=SDL_GetMouseState(nullptr,nullptr);out.key_down=(buttons&SDL_BUTTON_RMASK)!=0;
    if(!releasing&&(buttons&~uint32_t(SDL_BUTTON_RMASK)))return reject(error,"tutorial camera another mouse button is held");
    return true;
}
bool camera_observe(void*,int32_t index,Snapshot& out,std::string& error){
    if(!observe_native(out,error,true,index)||!camera_button_state(out,error))return false;
    if(index==10&&out.mouse_auto_capture)return reject(error,"tutorial rotation requires owned Host SDL_MOUSE_AUTO_CAPTURE=0; Windows capture reconciles synthetic RMB with physical state");
    return true;
}
bool camera_release_observe(void*,const Snapshot& owner,Snapshot& out,std::string& error){
    if(GetCurrentThreadId()!=mainthread)return reject(error,"tutorial camera release Mainthread differs");
    const auto window=reinterpret_cast<SDL_Window*>(owner.window);const auto hwnd=reinterpret_cast<HWND>(owner.hwnd);DWORD pid=0;
    if(!window||SDL_GetWindowFromID(owner.window_id)!=window||SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr)!=hwnd||
       GetWindowThreadProcessId(hwnd,&pid)!=mainthread||pid!=GetCurrentProcessId())return reject(error,"tutorial camera original release window owner changed");
    out=owner;return camera_button_state(out,error,true);
}
DWORD camera_original(SDL_Window* window,CameraInput input,uint64_t timestamp){
    __try{
        if(input==CameraInput::RotateDown||input==CameraInput::RotateUp)
            reinterpret_cast<void(*)(Uint64,SDL_Window*,SDL_MouseID,Uint8,bool)>(sdl+0x5c1d0)(timestamp,window,0,SDL_BUTTON_RIGHT,input==CameraInput::RotateDown);
        else if(input==CameraInput::RotateMotion)
            reinterpret_cast<void(*)(Uint64,SDL_Window*,SDL_MouseID,bool,float,float)>(sdl+0x5c200)(timestamp,window,0,true,16.0f,0.0f);
        else reinterpret_cast<void(*)(Uint64,SDL_Window*,SDL_MouseID,float,float,SDL_MouseWheelDirection)>(sdl+0x5b6b0)(timestamp,window,0,0.0f,input==CameraInput::WheelIn?1.0f:-1.0f,SDL_MOUSEWHEEL_NORMAL);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool camera_send(void*,const Snapshot& owner,CameraInput input,std::string& error){
    Snapshot current;if(!camera_release_observe(nullptr,owner,current,error)||!camera_pin(error))return false;
    if(camera_pending.size()>=3)return reject(error,"tutorial camera pending original events exceed3");
    const auto timestamp=SDL_GetTicksNS();if(!timestamp)return reject(error,"tutorial camera event timestamp unavailable");
    camera_pending.push_back({input,timestamp});
    const auto exception=camera_original(reinterpret_cast<SDL_Window*>(owner.window),input,timestamp);
    if(exception)return reject(error,"tutorial camera original SDL ingress SEH="+std::to_string(exception));
    error.clear();return true;
}
CameraFixture camera_fixture({nullptr,camera_observe,camera_release_observe,camera_send,[](void*){return uint64_t(GetTickCount64());}});
bool camera_active(){return camera_fixture.receipt().active;}
std::string camera_event_detail(const SDL_Event& event,CameraInput expected,uint32_t window){
    std::string detail=" expected_input="+std::to_string(static_cast<int>(expected))+" expected_window="+std::to_string(window)+
        " observed_type="+std::to_string(event.type)+" timestamp="+std::to_string(event.common.timestamp);
    if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN||event.type==SDL_EVENT_MOUSE_BUTTON_UP)
        detail+=" observed_window="+std::to_string(event.button.windowID)+" observed_device="+std::to_string(event.button.which)+" observed_button="+std::to_string(event.button.button);
    else if(event.type==SDL_EVENT_MOUSE_MOTION)
        detail+=" observed_window="+std::to_string(event.motion.windowID)+" observed_device="+std::to_string(event.motion.which)+" observed_xrel="+std::to_string(event.motion.xrel)+" observed_yrel="+std::to_string(event.motion.yrel);
    else if(event.type==SDL_EVENT_MOUSE_WHEEL)
        detail+=" observed_window="+std::to_string(event.wheel.windowID)+" observed_device="+std::to_string(event.wheel.which)+" observed_x="+std::to_string(event.wheel.x)+" observed_y="+std::to_string(event.wheel.y);
    return detail;
}
void camera_poll(const SDL_Event& event){
    if(!camera_active())return;
    const auto found=std::find_if(camera_pending.begin(),camera_pending.end(),[&](const auto& item){return item.timestamp==event.common.timestamp;});
    if(found==camera_pending.end())return;
    const auto kind=found->input;const auto window=camera_fixture.receipt().before.window_id;
    const bool button=(kind==CameraInput::RotateDown||kind==CameraInput::RotateUp);
    const bool match=button?(event.type==(kind==CameraInput::RotateDown?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP)&&event.button.windowID==window&&event.button.which==0&&event.button.button==SDL_BUTTON_RIGHT):
        kind==CameraInput::RotateMotion?(event.type==SDL_EVENT_MOUSE_MOTION&&event.motion.windowID==window&&event.motion.which==0&&event.motion.xrel==16.0f&&event.motion.yrel==0.0f):
        (event.type==SDL_EVENT_MOUSE_WHEEL&&event.wheel.windowID==window&&event.wheel.which==0&&event.wheel.x==0&&event.wheel.y==(kind==CameraInput::WheelIn?1.0f:-1.0f));
    std::string error;
    if(!match){camera_fixture.suspend("tutorial camera exact original event readback differs"+camera_event_detail(event,kind,window),error);report(error);return;}
    camera_pending.erase(found);if(!camera_fixture.polled(kind,error))report(error);
}
