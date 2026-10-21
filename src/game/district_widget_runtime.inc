namespace {
using FloatSpinner=void(__fastcall*)(void*,float);
std::array<void*,5> additional_spinners{};
uintptr_t presenting_widget=0;
uint32_t exchange_binding(uintptr_t slot,uintptr_t expected,uintptr_t desired,bool& exchanged){
    __try{exchanged=uintptr_t(_InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(slot),reinterpret_cast<void*>(desired),reinterpret_cast<void*>(expected)))==expected;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct PresentationPort {
    bool exchange(uintptr_t slot,uintptr_t expected,uintptr_t desired,std::string& error){
        bool changed=false;const auto exception=exchange_binding(slot,expected,desired,changed);
        if(exception||!changed){error="District local widget binding CAS failed SEH="+std::to_string(exception)+" exchanged="+std::to_string(changed);return false;}return true;
    }
};
uint32_t widget_call(void* original,void* receiver,int32_t integer,float number,const void* color,bool notify,int kind){
    __try{
        if(kind==0)reinterpret_cast<Spinner>(original)(receiver,integer);
        else if(kind==1)reinterpret_cast<FloatSpinner>(original)(receiver,number);
        else if(kind==2)reinterpret_cast<Color>(original)(receiver,color,notify);
        else reinterpret_cast<void(__fastcall*)(void*,bool)>(original)(receiver,integer!=0);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool present(Runtime& runtime,const controls::Binding& binding,controls::Widget type,void* original,void* receiver,
             int32_t integer,float number,const void* color,bool notify,int kind,std::string& error){
    if(presenting_widget){error="District local widget presentation reentered a bound input";return false;}
    if(!controls::unchanged(runtime.memory,runtime.image,binding,type,error))return false;
    PresentationPort port;presenting_widget=binding.widget;
    const bool returned=district_widget_scope::run(port,binding.value+0x38,binding.scalar,[&](std::string& why){
        const auto exception=widget_call(original,receiver,integer,number,color,notify,kind);
        if(exception){why="District original local widget presentation failed SEH="+std::to_string(exception);return false;}return true;
    },error);
    presenting_widget=0;
    return returned&&controls::unchanged(runtime.memory,runtime.image,binding,type,error);
}
bool ui_value(Runtime& runtime,const controls::Binding& binding,property::Value& value,std::string& error){
    value={};value.kind=binding.kind;const auto* field=property::field(value.kind);
    if(field->type==property::Type::Flag){uint8_t flag=0;if(!district_saved_identity::at(runtime.memory,binding.value,0x40,flag,error))return false;value.bits[0]=flag;}
    else if(!runtime.memory.read(binding.value+0x40,value.bits.data(),field->type==property::Type::Color?16:4,error))return false;
    if(!property::valid(value)){error="District original local widget produced an invalid property value";return false;}return true;
}
bool widget_input(Runtime& runtime,const controls::Binding& binding,controls::Widget type,void* original,void* receiver,
                  int32_t integer,float number,const void* color,bool notify,int kind,bool refresh,std::string& error){
    const Source* held=source?&*source:pending_source?&*pending_source:nullptr;
    if(refresh&&held&&held->target.row.pointer==binding.object&&held->target.row.uid==binding.uid&&held->value.kind==binding.kind)
        return unchanged(runtime,*held,error);
    Source before;
    if(!refresh){
        if(source){before=*source;if(before.target.row.pointer!=binding.object||before.target.row.uid!=binding.uid||before.value.kind!=binding.kind){error="District input does not own the active edit lifetime";return false;}}
        else if(!capture_owner(runtime,binding.object,binding.kind,before,error))return false;
        if(!unchanged(runtime,before,error))return false;
    }
    if(!present(runtime,binding,type,original,receiver,integer,number,color,notify,kind,error))return false;
    if(refresh)return true;
    property::Value value;if(!ui_value(runtime,binding,value,error)||!unchanged(runtime,before,error))return false;
    return capture_widget(runtime,binding,value,type,error);
}
void spinner_dispatch(size_t index,void* receiver,int32_t integer,float number,uintptr_t caller){
    void* original=index==0?reinterpret_cast<void*>(spinner_original):additional_spinners[index-1];
    const auto kind=index==1?1:0;
    auto* runtime=published.load(std::memory_order_acquire);
    if(!active()||!runtime){const auto exception=widget_call(original,receiver,integer,number,nullptr,false,kind);if(exception)throw std::runtime_error("District unbound original spinner SEH="+std::to_string(exception));return;}
    try{
        const auto widget=uintptr_t(receiver)-((index==2||index==3)?0x318:0);
        controls::Binding binding;bool ours=false;std::string error;
        if(!controls::bind(runtime->memory,runtime->image,widget,controls::Widget::Spinner,binding,ours,error)){reject(error);return;}
        if(!ours){const auto exception=widget_call(original,receiver,integer,number,nullptr,false,kind);if(exception)reject("District unrelated original spinner SEH="+std::to_string(exception));return;}
        const bool refresh=index==0&&caller==runtime->image+0x7c3485;
        if(!widget_input(*runtime,binding,controls::Widget::Spinner,original,receiver,integer,number,nullptr,false,kind,refresh,error))reject(error);
    }catch(const std::exception& exception){reject(std::string("District native spinner: ")+exception.what());}
}
void spinner_entry(void* receiver,int32_t value){spinner_dispatch(0,receiver,value,0,uintptr_t(_ReturnAddress()));}
void spinner_float_entry(void* receiver,float value){spinner_dispatch(1,receiver,0,value,uintptr_t(_ReturnAddress()));}
void spinner_drag_thunk(void* receiver,int32_t value){spinner_dispatch(2,receiver,value,0,uintptr_t(_ReturnAddress()));}
void spinner_click_thunk(void* receiver,int32_t value){spinner_dispatch(3,receiver,value,0,uintptr_t(_ReturnAddress()));}
void spinner_click_entry(void* receiver,int32_t value){spinner_dispatch(4,receiver,value,0,uintptr_t(_ReturnAddress()));}
void spinner_drag_entry(void* receiver,int32_t value){spinner_dispatch(5,receiver,value,0,uintptr_t(_ReturnAddress()));}
void color_entry(void* receiver,const void* desired,bool notify){
    auto* runtime=published.load(std::memory_order_acquire);
    if(!active()||!runtime){color_original(receiver,desired,notify);return;}
    try{controls::Binding binding;bool ours=false;std::string error;
        if(!controls::bind(runtime->memory,runtime->image,uintptr_t(receiver),controls::Widget::Color,binding,ours,error)){reject(error);return;}
        if(!ours){color_original(receiver,desired,notify);return;}
        const auto caller=uintptr_t(_ReturnAddress())-runtime->image;
        const bool refresh=caller!=0x833fa0&&caller!=0x83a268&&caller!=0x83a345;
        if(!widget_input(*runtime,binding,controls::Widget::Color,reinterpret_cast<void*>(color_original),receiver,0,0,desired,notify,2,refresh,error))reject(error);
    }catch(const std::exception& exception){reject(std::string("District native color: ")+exception.what());}
}
}
bool checkbox(uintptr_t receiver,bool desired,void* original,bool& handled,std::string& error){
    handled=false;auto* runtime=published.load(std::memory_order_acquire);if(!active()||!runtime)return true;
    controls::Binding binding;
    if(!controls::bind(runtime->memory,runtime->image,receiver,controls::Widget::Checkbox,binding,handled,error))return false;
    if(!handled)return true;
    if(!original){error="District shared checkbox original is unavailable";return false;}
    uint8_t current=0;
    if(!runtime->memory.read(binding.scalar,&current,sizeof current,error))return false;
    if(current>1){error="District shared checkbox current field is not Boolean";return false;}
    return widget_input(*runtime,binding,controls::Widget::Checkbox,original,reinterpret_cast<void*>(receiver),int32_t(desired),0,nullptr,false,3,current==uint8_t(desired),error);
}
