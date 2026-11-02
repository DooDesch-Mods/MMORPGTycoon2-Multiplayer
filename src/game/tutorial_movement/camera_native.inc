struct CameraPending {CameraInput input;uint64_t timestamp;float x=0,y=0;};
std::vector<CameraPending> camera_pending;
struct CameraPosition {float before_x=0,before_y=0,target_x=0,target_y=0;int width=0,height=0;uint64_t position_timestamp=0,button_timestamp=0;std::string position_event_bytes,button_event_bytes;};
CameraPosition camera_position;
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
    if(input==CameraInput::Position)return reject(error,"tutorial camera position requires its validated preparation");
    if(camera_pending.size()>=3)return reject(error,"tutorial camera pending original events exceed3");
    const auto timestamp=SDL_GetTicksNS();if(!timestamp)return reject(error,"tutorial camera event timestamp unavailable");
    if(input==CameraInput::RotateDown)camera_position.button_timestamp=timestamp;
    camera_pending.push_back({input,timestamp});
    const auto exception=camera_original(reinterpret_cast<SDL_Window*>(owner.window),input,timestamp);
    if(exception)return reject(error,"tutorial camera original SDL ingress SEH="+std::to_string(exception));
    error.clear();return true;
}
DWORD camera_position_original(SDL_Window* window,uint64_t timestamp,float x,float y){
    __try{reinterpret_cast<void(*)(Uint64,SDL_Window*,SDL_MouseID,bool,float,float)>(sdl+0x5c200)(timestamp,window,0,false,x,y);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool camera_position_bounds(SDL_Window* window,int& left,int& top,int& right,int& bottom,std::string& error){
    int width=0,height=0;if(!SDL_GetWindowSize(window,&width,&height)||width<=2||height<=2)return reject(error,"tutorial camera logical window size unavailable");
    int64_t l=0,t=0,r=width,b=height;
    if(const auto* rect=SDL_GetWindowMouseRect(window)){
        if(rect->w<=0||rect->h<=0)return reject(error,"tutorial camera confinement rectangle is empty");
        l=std::max<int64_t>(l,rect->x);t=std::max<int64_t>(t,rect->y);
        r=std::min<int64_t>(r,int64_t(rect->x)+rect->w);b=std::min<int64_t>(b,int64_t(rect->y)+rect->h);
    }
    if(r-l<=2||b-t<=2)return reject(error,"tutorial camera confinement has no interior point");
    left=int(l);top=int(t);right=int(r);bottom=int(b);camera_position.width=width;camera_position.height=height;return true;
}
bool camera_prepare_position(void*,const Snapshot& owner,bool& pending,std::string& error){
    camera_position={};pending=false;Snapshot current;if(!camera_release_observe(nullptr,owner,current,error)||!camera_pin(error))return false;
    if(current.mouse_relative||current.mouse_capture||!current.mouse_focus||SDL_GetMouseState(nullptr,nullptr)!=0)
        return reject(error,"tutorial camera position requires focused uncaptured absolute mouse with no buttons");
    auto* window=reinterpret_cast<SDL_Window*>(owner.window);int left=0,top=0,right=0,bottom=0;
    if(!camera_position_bounds(window,left,top,right,bottom,error))return false;
    float x=0,y=0;if(SDL_GetMouseState(&x,&y)!=0||!std::isfinite(x)||!std::isfinite(y))return reject(error,"tutorial camera current mouse position invalid");
    camera_position.before_x=x;camera_position.before_y=y;camera_position.target_x=float(left+(right-left)/2);camera_position.target_y=float(top+(bottom-top)/2);
    if(x==camera_position.target_x&&y==camera_position.target_y){error.clear();return true;}
    if(camera_pending.size()>=3)return reject(error,"tutorial camera pending original events exceed3");
    const auto timestamp=SDL_GetTicksNS();if(!timestamp)return reject(error,"tutorial camera position timestamp unavailable");
    camera_position.position_timestamp=timestamp;camera_pending.push_back({CameraInput::Position,timestamp,camera_position.target_x,camera_position.target_y});pending=true;
    const auto exception=camera_position_original(window,timestamp,camera_position.target_x,camera_position.target_y);
    if(exception)return reject(error,"tutorial camera original position ingress SEH="+std::to_string(exception));
    error.clear();return true;
}
bool camera_position_ready(void*,const Snapshot& owner,std::string& error){
    Snapshot current;if(!camera_release_observe(nullptr,owner,current,error))return false;
    if(current.mouse_relative||current.mouse_capture||!current.mouse_focus)return reject(error,"tutorial camera lost uncaptured absolute mouse focus after positioning");
    if((GetAsyncKeyState(VK_RBUTTON)&0x8000)||(GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_MBUTTON)&0x8000))
        return reject(error,"tutorial camera physical mouse button changed after positioning");
    auto* window=reinterpret_cast<SDL_Window*>(owner.window);int left=0,top=0,right=0,bottom=0;
    if(!camera_position_bounds(window,left,top,right,bottom,error))return false;
    float x=0,y=0;if(SDL_GetMouseState(&x,&y)!=0||x!=camera_position.target_x||y!=camera_position.target_y||
       x<=float(left)||x>=float(right)||y<=float(top)||y>=float(bottom))return reject(error,"tutorial camera positioned mouse readback or confinement differs");
    error.clear();return true;
}
CameraFixture camera_fixture({nullptr,camera_observe,camera_release_observe,camera_send,camera_prepare_position,camera_position_ready,[](void*){return uint64_t(GetTickCount64());}});
bool camera_active(){return camera_fixture.receipt().active;}
std::string camera_event_bytes(const void* value,size_t size){
    constexpr char digits[]="0123456789abcdef";const auto* bytes=static_cast<const unsigned char*>(value);std::string out;out.reserve(size*2);
    for(size_t i=0;i<size;++i){out.push_back(digits[bytes[i]>>4]);out.push_back(digits[bytes[i]&15]);}return out;
}
std::string camera_event_detail(const SDL_Event& event,CameraInput expected,uint32_t window){
    std::string detail=" expected_input="+std::to_string(static_cast<int>(expected))+" expected_window="+std::to_string(window)+
        " observed_type="+std::to_string(event.type)+" timestamp="+std::to_string(event.common.timestamp);
    if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN||event.type==SDL_EVENT_MOUSE_BUTTON_UP)
        detail+=" observed_window="+std::to_string(event.button.windowID)+" observed_device="+std::to_string(event.button.which)+" observed_button="+std::to_string(event.button.button);
    else if(event.type==SDL_EVENT_MOUSE_MOTION)
        detail+=" observed_window="+std::to_string(event.motion.windowID)+" observed_device="+std::to_string(event.motion.which)+" observed_state="+std::to_string(event.motion.state)+" observed_x="+std::to_string(event.motion.x)+" observed_y="+std::to_string(event.motion.y)+" observed_xrel="+std::to_string(event.motion.xrel)+" observed_yrel="+std::to_string(event.motion.yrel);
    else if(event.type==SDL_EVENT_MOUSE_WHEEL)
        detail+=" observed_window="+std::to_string(event.wheel.windowID)+" observed_device="+std::to_string(event.wheel.which)+" observed_x="+std::to_string(event.wheel.x)+" observed_y="+std::to_string(event.wheel.y);
    return detail;
}
void camera_poll(const SDL_Event& event,uint64_t frame){
    if(!camera_active())return;
    const auto found=std::find_if(camera_pending.begin(),camera_pending.end(),[&](const auto& item){return item.timestamp==event.common.timestamp;});
    if(found==camera_pending.end())return;
    const auto kind=found->input;const auto window=camera_fixture.receipt().before.window_id;
    if(kind==CameraInput::Position&&event.type==SDL_EVENT_MOUSE_MOTION)camera_position.position_event_bytes=camera_event_bytes(&event.motion,sizeof(event.motion));
    if(kind==CameraInput::RotateDown&&(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN||event.type==SDL_EVENT_MOUSE_BUTTON_UP))camera_position.button_event_bytes=camera_event_bytes(&event.button,sizeof(event.button));
    const bool button=(kind==CameraInput::RotateDown||kind==CameraInput::RotateUp);
    const bool match=kind==CameraInput::Position?(event.type==SDL_EVENT_MOUSE_MOTION&&event.motion.windowID==window&&event.motion.which==0&&event.motion.state==0&&event.motion.x==found->x&&event.motion.y==found->y):
        button?(event.type==(kind==CameraInput::RotateDown?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP)&&event.button.windowID==window&&event.button.which==0&&event.button.button==SDL_BUTTON_RIGHT):
        kind==CameraInput::RotateMotion?(event.type==SDL_EVENT_MOUSE_MOTION&&event.motion.windowID==window&&event.motion.which==0&&event.motion.xrel==16.0f&&event.motion.yrel==0.0f):
        (event.type==SDL_EVENT_MOUSE_WHEEL&&event.wheel.windowID==window&&event.wheel.which==0&&event.wheel.x==0&&event.wheel.y==(kind==CameraInput::WheelIn?1.0f:-1.0f));
    std::string error;
    if(!match){camera_fixture.suspend("tutorial camera exact original event readback differs"+camera_event_detail(event,kind,window),error);report(error);return;}
    camera_pending.erase(found);if(!camera_fixture.polled(kind,frame,error))report(error);
}
