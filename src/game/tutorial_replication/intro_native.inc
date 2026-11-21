namespace intro_native {
using Value=scenario_intro::State;
constexpr const char* id="intro_cutscene";
Update original_update=nullptr;void(*original_event)(void*,const void*)=nullptr;
release_ui_owner::Owner update_owner,event_owner;
bool scoped_start=false,start_entered=false,start_wait_logged=false;
struct Owners {lifecycle_native::Owners log;uintptr_t instance=0;std::vector<std::string> completed;};
bool owners(const World& world,Owners& out,Value& value,std::string& e){
    out={};value={};if(!lifecycle_native::owners(world,id,out.log,e))return false;
    for(auto instance:out.log.instances){std::string actual;if(!str(instance,0x18,actual,e))return false;if(actual!=id)continue;
        if(out.instance||!type(instance,0x173e880,e))return fail(e,e.empty()?"Intro Scenario current instance class or unique membership differs":e);
        out.instance=instance;uint8_t complete=0;if(!at(instance,0x38,complete,e)||complete>1||!at(instance,0x60,value.phase,e)||!at(instance,0x78,value.starting_points,e))return fail(e,e.empty()?"Intro Scenario native completion byte differs":e);
        value.active=true;value.retiring=complete!=0;
    }
    value.completed=out.log.completed!=0;uintptr_t data=0;int32_t count=0;if(!lifecycle_native::array(out.log.log,0x50,512,data,count,e))return false;
    for(int32_t i=0;i<count;++i){std::string entry;if(!str(data,size_t(i)*32,entry,e))return false;out.completed.push_back(std::move(entry));}
    return scenario_intro::valid(value,e);
}
bool fenced(uintptr_t instance,std::string& e){
    if(GetCurrentThreadId()!=mainthread||!type(instance,0x173e880,e))return fail(e,e.empty()?"Client Intro callback thread/class differs":e);
    lifecycle_native::Owners owner;
    if(!load.ready){if(!lifecycle_native::bootstrap_owners(id,owner,e))return false;}
    else{World world;action_session::Context c;action_session::Binding b;
        if(!action_session::selected_session().current(c,e)||c.role!=action_session::Role::Client||!native_open_runtime::read_current_world(world,e))return fail(e,e.empty()?"Client Intro callback lacks a current Client World":e);
        b={world.world_epoch,world.generation};if(!origin(c,b,world,e)||!lifecycle_native::owners(world,id,owner,e))return false;
    }
    return std::count(owner.instances.begin(),owner.instances.end(),instance)==1||fail(e,"Client Intro callback is not a unique current ScenarioLog member");
}
void update(void* instance,float delta){if(!load.armed){original_update(instance,delta);return;}std::string e;if(!fenced(reinterpret_cast<uintptr_t>(instance),e))block(e);}
void event(void* instance,const void* message){if(!load.armed){original_event(instance,message);return;}std::string e;if(!fenced(reinterpret_cast<uintptr_t>(instance),e))block(e);}
void start(void* log,void* asset,void* parameters){
    std::string e;lifecycle_native::Owners owner;
    if(GetCurrentThreadId()!=mainthread||parameters){block("Client Intro Start thread/parameters differs");return;}
    if(!load.ready){if(!lifecycle_native::bootstrap_owners(id,owner,e)){block(e);return;}}
    else{World world;action_session::Context c;action_session::Binding b;
        if(!action_session::selected_session().current(c,e)||c.role!=action_session::Role::Client||!native_open_runtime::read_current_world(world,e)){block(e.empty()?"Client Intro Start lacks a current Client World":e);return;}
        b={world.world_epoch,world.generation};if(!origin(c,b,world,e)||!lifecycle_native::owners(world,id,owner,e)){block(e);return;}}
    if(owner.log!=reinterpret_cast<uintptr_t>(log)||owner.asset!=reinterpret_cast<uintptr_t>(asset)){block("Client Intro Start current log/asset differs");return;}
    if(!scoped_start){if(!start_wait_logged){LOG_INFO("[Tutorial] Client Intro native Start waits for authoritative Host state");start_wait_logged=true;}return;}
    if(start_entered){block("Client Intro scoped Start reentered");return;}start_entered=true;original_scenario_start(log,asset,parameters);
}
bool verify_owned(std::string& e){auto copy=[](auto p,auto out,auto bytes,auto& cause){return memory.copy(p,out,bytes,cause);};auto hash=[](const auto& bytes){return mt2mp::sha256(bytes);};
    return release_ui_owner::verify(update_owner,copy,hash,"d76efd8c414f9651ff051692129d66fdcac8121369d244d2154866d8f1468e30",e)&&release_ui_owner::verify(event_owner,copy,hash,"da1d91b87fa010743c37c9236398b3d26a366ca2d73da3bdae7246346844bad8",e);
}
bool prepare_hooks(std::string& e){
    struct Body{uintptr_t rva;size_t bytes;const char* sha;};constexpr Body bodies[]={
        {0x4f7490,80,"9187fc75200e8fdf4d8571e927d484c7d6356524b757d9dd0971c44888db4bb4"},
        {0x4f7b00,192,"47d32acab21e80a39c06729046e0ae805f3ce07caca2e64de7bd033c38d2aab2"},
        {0x4f7540,96,"49ba48e93347ebb447e20d873eab63f3fc25bb52b3c04d74452bb4cef997bb5b"}};
    for(const auto& body:bodies){std::vector<unsigned char> bytes(body.bytes);if(!memory.copy(image+body.rva,bytes.data(),bytes.size(),e)||mt2mp::sha256(bytes)!=body.sha)return fail(e,e.empty()?"Intro original construction/destruction body differs":e);}
    update_owner={image+0x4f8230,reinterpret_cast<uintptr_t>(&update),0,1,5,std::vector<unsigned char>(1216)};
    event_owner={image+0x4f7840,reinterpret_cast<uintptr_t>(&event),0,1,5,std::vector<unsigned char>(80)};
    if(!memory.copy(update_owner.entry,update_owner.pristine.data(),update_owner.pristine.size(),e)||mt2mp::sha256(update_owner.pristine)!="d76efd8c414f9651ff051692129d66fdcac8121369d244d2154866d8f1468e30"||
       !memory.copy(event_owner.entry,event_owner.pristine.data(),event_owner.pristine.size(),e)||mt2mp::sha256(event_owner.pristine)!="da1d91b87fa010743c37c9236398b3d26a366ca2d73da3bdae7246346844bad8")return fail(e,e.empty()?"Intro original Update/Event full body differs":e);
    return true;
}
DWORD invoke(bool complete,uintptr_t log,uintptr_t asset){__try{if(complete)reinterpret_cast<void(*)(void*,void*)>(image+0x4fa0b0)(reinterpret_cast<void*>(log),reinterpret_cast<void*>(asset));else reinterpret_cast<void(*)(void*,void*,void*)>(image+0x4f9a60)(reinterpret_cast<void*>(log),reinterpret_cast<void*>(asset),nullptr);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD cas(uintptr_t address,bool byte,int32_t before,int32_t target,bool& stored){__try{stored=byte?uint8_t(_InterlockedCompareExchange8(reinterpret_cast<volatile char*>(address),char(target),char(before)))==uint8_t(before):InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address),LONG(target),LONG(before))==LONG(before);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool sample(const action_session::Context& c,const action_session::Binding& b,World& world,Owners& owner,Value& value,std::string& e,bool ordered_read=false){
    World after;if(!origin(c,b,world,e,nullptr,ordered_read)||!owners(world,owner,value,e)||!origin(c,b,after,e,nullptr,ordered_read)||!same_world(world,after))return fail(e,e.empty()?"Intro complete current Origin changed":e);return true;
}
bool adopt(const action_session::Context& c,const action_session::Binding& b,const Value& target,bool& attempted,std::string& e){
    if(c.role!=action_session::Role::Client||!scenario_intro::valid(target,e)||!startingpoint_clock(e)||!verify_owned(e)||!lifecycle_native::verify_transition_code(e))return fail(e,e.empty()?"Intro adoption requires current Client Clock0 and owned original routes":e);
    World world;Owners before;Value value;if(!sample(c,b,world,before,value,e))return false;
    if((value.completed&&!target.completed)||(value.active&&!target.active&&!target.completed))return fail(e,"Intro authoritative lifecycle cannot move backward or remove without completion");
    if(value==target)return true;
    if(!value.active&&target.active){
        struct Reset{~Reset(){scoped_start=false;}} reset;scoped_start=true;start_entered=false;attempted=true;
        const auto code=invoke(false,before.log.log,before.log.asset);if(code||!fault.empty())return fail(e,code?"Intro original StartScenario exception="+std::to_string(code):fault);
        Owners after;Value observed;World current;if(!sample(c,b,current,after,observed,e))return false;auto expected=before.log.instances;expected.push_back(after.instance);
        if(!start_entered||!same_world(world,current)||!after.instance||!observed.active||observed.completed||after.completed!=before.completed||after.log.log!=before.log.log||after.log.asset!=before.log.asset||after.log.library!=before.log.library||after.log.instances!=expected)return fail(e,"Intro original Start appended instance/owner readback differs");before=std::move(after);value=observed;
    }
    if(!value.completed&&target.completed){
        attempted=true;const auto code=invoke(true,before.log.log,before.log.asset);if(code)return fail(e,"Intro original CompleteScenario exception="+std::to_string(code));
        Owners after;Value observed;World current;if(!sample(c,b,current,after,observed,e))return false;auto expected=before.log.instances;if(before.instance)expected.erase(std::find(expected.begin(),expected.end(),before.instance));auto completed=before.completed;completed.push_back(id);
        if(!same_world(world,current)||after.log.log!=before.log.log||after.log.asset!=before.log.asset||after.log.library!=before.log.library||after.log.instances!=expected||after.completed!=completed||observed!=target)return fail(e,"Intro original Complete removal/ordered completed IDs readback differs");return true;
    }
    if(target.active){
        const std::array<size_t,3> offsets{0x60,0x78,0x38};const std::array<int32_t,3> desired{target.phase,target.starting_points,int32_t(target.retiring)};
        for(size_t i=0;i<offsets.size();++i){const std::array<int32_t,3> prior{value.phase,value.starting_points,int32_t(value.retiring)};if(prior[i]==desired[i])continue;
            Owners fresh;Value current_value;World current;if(!sample(c,b,current,fresh,current_value,e)||!same_world(world,current)||fresh.instance!=before.instance||fresh.log.log!=before.log.log||fresh.log.asset!=before.log.asset||fresh.log.library!=before.log.library||fresh.log.instances!=before.log.instances||fresh.completed!=before.completed||current_value!=value||!startingpoint_clock(e))return fail(e,e.empty()?"Intro fresh scalar prior/owner differs":e);
            bool stored=false;attempted=true;const auto code=cas(before.instance+offsets[i],i==2,prior[i],desired[i],stored);if(code||!stored)return fail(e,"Intro scalar CAS failed exception="+std::to_string(code)+"; no replay");
            if(i==0)value.phase=desired[i];else if(i==1)value.starting_points=desired[i];else value.retiring=desired[i]!=0;
        }
    }
    Owners after;Value observed;World current;return sample(c,b,current,after,observed,e)&&same_world(world,current)&&after.instance==before.instance&&after.log.log==before.log.log&&after.log.asset==before.log.asset&&after.log.library==before.log.library&&after.log.instances==before.log.instances&&after.completed==before.completed&&observed==target||fail(e,e.empty()?"Intro complete native state readback differs":e);
}
}
