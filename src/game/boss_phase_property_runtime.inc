struct NativePropertyCalls {
    uint32_t create(uintptr_t image,uintptr_t owner,const std::string& name,bool& returned){
        __try{const auto phase=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0xc8);
            reinterpret_cast<void(*)(void*)>(image+0x4fe70)(reinterpret_cast<void*>(phase));
            reinterpret_cast<void(*)(void*,void*)>(image+0x938e0)(reinterpret_cast<void*>(owner),reinterpret_cast<void*>(phase));
            const prop_text::Text value{name.data(),name.size(),{}};
            reinterpret_cast<void(*)(void*,const void*)>(image+0x11bfce0)(reinterpret_cast<void*>(phase+0x38),&value);returned=true;return 0;
        }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    }
    uint32_t destroy(uintptr_t image,uintptr_t owner,uintptr_t phase,bool& returned){
        __try{reinterpret_cast<void(*)(void*,void*)>(image+0x938f0)(reinterpret_cast<void*>(owner),reinterpret_cast<void*>(phase));returned=true;return 0;}
        __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    }
    uint32_t add(uintptr_t image,uintptr_t phase,uintptr_t prototype){
        __try{const auto element=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0x20);
            reinterpret_cast<void(*)(void*)>(image+0x54170)(reinterpret_cast<void*>(element));
            *reinterpret_cast<uintptr_t*>(element+0x18)=prototype;
            reinterpret_cast<void(*)(void*,void*)>(image+0x4f8e0)(reinterpret_cast<void*>(phase),reinterpret_cast<void*>(element));return 0;
        }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    }
    uint32_t remove(uintptr_t image,uintptr_t phase,int32_t element){
        __try{reinterpret_cast<void(*)(void*,int32_t)>(image+0x4fab0)(reinterpret_cast<void*>(phase),element);return 0;}
        __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    }
    uint32_t event(uintptr_t image,uintptr_t phase,bool post){
        __try{
            // Original ItemView uses a 0x100-byte event slot, with Version at +0xd0.
            alignas(16) unsigned char event[0x100]{};
            reinterpret_cast<void(*)(void*,void*)>(image+(post?0xd7f00:0xd7e40))(event,reinterpret_cast<void*>(phase));
            reinterpret_cast<void(*)(const void*)>(image+0xd9200)(event);
            *reinterpret_cast<uintptr_t*>(event+0xd0)=image+0x174cca0;
            reinterpret_cast<void(*)(void*)>(image+0x89da30)(event+0xd0);
            reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(event+8);return 0;
        }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    }
    uint32_t text(uintptr_t image,uintptr_t destination,const std::string& value){
        __try{const prop_text::Text native{value.data(),value.size(),{}};
            reinterpret_cast<void(*)(void*,const void*)>(image+0x11bfce0)(reinterpret_cast<void*>(destination),&native);return 0;
        }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    }
    uint32_t bytes(uintptr_t destination,const void* data,size_t size){
        __try{std::memcpy(reinterpret_cast<void*>(destination),data,size);return 0;}
        __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    }
};
uint32_t property_label(uintptr_t image,uintptr_t record,uintptr_t& label){
    __try{label=reinterpret_cast<uintptr_t(*)(const void*)>(image+0x96d880)(reinterpret_cast<const void*>(record));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t property_value(uintptr_t image,uintptr_t record,action::BossPhaseKind kind,uintptr_t& text,float& health,std::array<float,4>& color,int32_t& element){
    __try{
        if(kind==action::BossPhaseKind::Color)reinterpret_cast<void(*)(void*,const void*)>(image+0x969960)(color.data(),reinterpret_cast<const void*>(record));
        else{const auto token=reinterpret_cast<uintptr_t(*)(const void*,int32_t)>(image+0x967740)(reinterpret_cast<const void*>(record),0);
            if(kind==action::BossPhaseKind::RemoveAbility)element=reinterpret_cast<int32_t(*)(const void*)>(image+0x96d2c0)(reinterpret_cast<const void*>(token));
            else if(kind==action::BossPhaseKind::Health)health=reinterpret_cast<float(*)(const void*)>(image+0x96d300)(reinterpret_cast<const void*>(token));
            else text=reinterpret_cast<uintptr_t(*)(const void*)>(image+0x96d880)(reinterpret_cast<const void*>(token));}
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool property_same(const action::BossPhaseRow& phase,const action::BossPhaseIntent& intent){
    switch(intent.kind){case action::BossPhaseKind::Name:return phase.name==intent.text;
    case action::BossPhaseKind::Health:return phase.health==intent.health;case action::BossPhaseKind::Vfx:return phase.vfx==intent.text;
    case action::BossPhaseKind::Color:return phase.color==intent.color;default:return false;}
}
bool property_membership_valid(const action::BossTypePhases& owner,const action::BossPhaseIntent& intent){
    if(intent.kind==action::BossPhaseKind::Create)return size_t(intent.phase)==owner.phases.size()&&owner.phases.size()<10000&&intent.text=="{design_boss_defaultphasename} "+std::to_string(owner.phases.size()+1);
    if(intent.kind==action::BossPhaseKind::Destroy)return size_t(intent.phase)<owner.phases.size();
    if(intent.kind==action::BossPhaseKind::RemoveAbility)return size_t(intent.element)<owner.phases[size_t(intent.phase)].abilities.size();
    if(intent.kind==action::BossPhaseKind::AddAbility){std::vector<int32_t> matches;
        for(size_t i=0;i<owner.ability_names.size();++i)if(owner.ability_names[i]==intent.text)matches.push_back(int32_t(i));
        return !matches.empty()&&matches==intent.ability_indices&&owner.phases[size_t(intent.phase)].abilities.size()+matches.size()<=10000;}
    return true;
}
bool property_capture(Runtime& r,uintptr_t item,action::BossPhaseIntent intent,std::string& e){
    const auto controller=typed_action_runtime::controller_status();action::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!installed||!r.ready||GetCurrentThreadId()!=r.thread||captured||r.pending||prop_text::pending()||
       !controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||
       !typed_action_runtime::network_binding(snapshot,binding,e)){
        if(e.empty())e="Boss phase property capture requires idle bound Mainthread Controller";return false;}
    auto at=[&](uintptr_t base,size_t offset,auto& value){return base&&base<=UINTPTR_MAX-offset&&r.memory.read(base+offset,&value,sizeof value,e);};
    uintptr_t ap=0,object=0,phase=0,parent=0,window=0,list=0,owned_window=0;
    if(!at(item,0,ap)||ap!=r.image+0x1721750||!at(item,0x340,object)||!at(item,0x348,phase)||!at(item,0x350,parent)||
       !at(item,0x280,owned_window)||!live_list(r,object,window,list,e)||!list||parent!=list||owned_window!=window){
        if(e.empty())e="Boss phase property current Item/List/Window ownership differs";return false;}
    Sample before;if(!sample(r,binding,snapshot,before,e))return false;
    for(size_t i=0;i<before.local.size();++i)if(before.local[i].object==object){intent.character_id=int32_t(i);
        for(size_t j=0;j<before.local[i].phases.size();++j)if(before.local[i].phases[j].pointer==phase)intent.phase=int32_t(j);}
    if(intent.character_id<0||intent.phase<0){e="Boss phase property current owner/phase absent";return false;}
    if(intent.kind==action::BossPhaseKind::AddAbility){intent.ability_indices.clear();const auto& names=before.value.bosses[size_t(intent.character_id)].ability_names;
        for(size_t i=0;i<names.size();++i)if(names[i]==intent.text)intent.ability_indices.push_back(int32_t(i));
        if(intent.ability_indices.empty())return true;}
    if(!action::valid_boss_phase(intent)||before.value.bosses[size_t(intent.character_id)].deleted||!property_membership_valid(before.value.bosses[size_t(intent.character_id)],intent)){e="Boss phase property exact current type/phase/value differs";return false;}
    if(property_same(before.value.bosses[size_t(intent.character_id)].phases[size_t(intent.phase)],intent))return true;
    captured=Pending{binding,snapshot,std::move(intent)};++captures;return true;
}
bool property_entry(void* object,void* record){
    auto* r=published.load(std::memory_order_acquire);
    if(!net::session_wire::connected()||!r)return original_command(object,record);
    try{
        uintptr_t label=0;std::string command,error;auto seh=property_label(r->image,uintptr_t(record),label);
        if(seh||!prop_text::text(*r,label,command,error)){reject("Boss phase UI command read SEH="+std::to_string(seh)+" "+error);return true;}
        action::BossPhaseIntent intent;
        if(command=="SetName")intent.kind=action::BossPhaseKind::Name;
        else if(command=="SetHealth")intent.kind=action::BossPhaseKind::Health;
        else if(command=="SetVFX")intent.kind=action::BossPhaseKind::Vfx;
        else if(command=="SetVFXColor")intent.kind=action::BossPhaseKind::Color;
        else if(command=="DestroyElement")intent.kind=action::BossPhaseKind::RemoveAbility;
        else if(command=="DestroyPhase")intent.kind=action::BossPhaseKind::Destroy;
        else return original_command(object,record);
        uintptr_t text=0;float health=0;seh=intent.kind==action::BossPhaseKind::Destroy?0:property_value(r->image,uintptr_t(record),intent.kind,text,health,intent.color,intent.element);
        if(seh){reject("Boss phase UI property read SEH="+std::to_string(seh));return true;}
        if(intent.kind==action::BossPhaseKind::Name||intent.kind==action::BossPhaseKind::Vfx){
            if(!prop_text::text(*r,text,intent.text,error)){reject(error);return true;}}
        if(intent.kind==action::BossPhaseKind::Health){
            if(!std::isfinite(health)||double(health)<double(INT32_MIN)||double(health)>double(INT32_MAX)){reject("Boss phase UI health is outside finite int32 conversion");return true;}
            intent.health=int32_t(health);}
        if(!property_capture(*r,uintptr_t(object),std::move(intent),error))reject(error);return true;
    }catch(const std::exception& x){reject(std::string("Boss phase UI property capture: ")+x.what());return true;}
}

void menu_entry(void* object,const void* name){
    auto* r=published.load(std::memory_order_acquire);if(!net::session_wire::connected()||!r){original_menu(object,name);return;}
    try{action::BossPhaseIntent intent;intent.kind=action::BossPhaseKind::AddAbility;std::string error;
        if(!prop_text::text(*r,uintptr_t(name),intent.text,error)||!property_capture(*r,uintptr_t(object),std::move(intent),error))reject(error);
    }catch(const std::exception& x){reject(std::string("Boss phase ability menu capture: ")+x.what());}
}

bool create_capture(Runtime& r,uintptr_t list,std::string& e){
    const auto controller=typed_action_runtime::controller_status();action::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!installed||!r.ready||GetCurrentThreadId()!=r.thread||captured||r.pending||prop_text::pending()||
       !controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||
       !typed_action_runtime::network_binding(snapshot,binding,e)){
        if(e.empty())e="Boss phase create requires idle bound Mainthread Controller";return false;}
    uintptr_t object=0,window=0,current_list=0;
    if(!list||!r.memory.read(list+0x318,&object,sizeof object,e)||!live_list(r,object,window,current_list,e)||list!=current_list){
        if(e.empty())e="Boss phase create requires current selected owner/list";return false;}
    Sample before;if(!sample(r,binding,snapshot,before,e))return false;
    action::BossPhaseIntent intent;intent.kind=action::BossPhaseKind::Create;
    for(size_t i=0;i<before.local.size();++i)if(before.local[i].object==object)intent.character_id=int32_t(i);
    if(intent.character_id<0){e="Boss phase create current type absent";return false;}
    const auto& owner=before.value.bosses[size_t(intent.character_id)];intent.phase=int32_t(owner.phases.size());
    intent.text="{design_boss_defaultphasename} "+std::to_string(owner.phases.size()+1);
    if(owner.deleted||!action::valid_boss_phase(intent)||!property_membership_valid(owner,intent)){e="Boss phase create owner/bounds differ";return false;}
    captured=Pending{binding,snapshot,std::move(intent)};++captures;return true;
}
void create_entry(void* list){
    auto* r=published.load(std::memory_order_acquire);
    if(!net::session_wire::connected()||!r||uintptr_t(_ReturnAddress())!=r->image+0x6963eb){original_create(list);return;}
    try{std::string error;if(!create_capture(*r,uintptr_t(list),error))reject(error);}
    catch(const std::exception& x){reject(std::string("Boss phase create capture: ")+x.what());}
}
