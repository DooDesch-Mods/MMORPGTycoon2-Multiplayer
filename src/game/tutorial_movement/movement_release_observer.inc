bool observe_release(void*,const Snapshot& owner,Snapshot& out,std::string& e){
    out=owner;
    if(GetCurrentThreadId()!=mainthread)return reject(e,"tutorial release Mainthread differs");
    const auto window=reinterpret_cast<SDL_Window*>(owner.window);const auto hwnd=reinterpret_cast<HWND>(owner.hwnd);
    DWORD pid=0;
    if(!window||!hwnd||SDL_GetWindowFromID(owner.window_id)!=window||
       SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr)!=hwnd||
       GetWindowThreadProcessId(hwnd,&pid)!=mainthread||pid!=GetCurrentProcessId())
        return reject(e,"tutorial release original input window changed");
    if(!verify_contract(memory,image,sdl,e))return false;
    int count=0;const auto keys=SDL_GetKeyboardState(&count);
    if(!keys||count<=SDL_SCANCODE_W)return reject(e,"tutorial release SDL keyboard state unavailable");
    uintptr_t input=0,ap=0;
    if(!at(image,0x13df990,input,e)||!at(input,0,ap,e))return false;
    if(ap!=image+0x1749430)return reject(e,"tutorial release native Input AP differs");
    Array axes;if(!array(input+0x48,0x17461c0,0x70,axes,e))return false;
    if(axes.count<6)return reject(e,"tutorial release movement axes absent");
    uint8_t subtract=0;
    if(!at(axes.data+2*0x70,0x5c,out.axis,e)||!at(axes.data+2*0x70,0x63,subtract,e))return false;
    Array after;if(!at(input,0x48,after,e))return false;
    if(after!=axes||subtract||!std::isfinite(out.axis))return reject(e,"tutorial release native W axis owner/value differs");
    out.key_down=keys[SDL_SCANCODE_W];e.clear();return true;
}
