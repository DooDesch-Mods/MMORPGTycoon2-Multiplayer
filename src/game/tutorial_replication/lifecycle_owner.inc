namespace lifecycle_native {
struct Cache {World world;action_session::Context context;State proof;uint32_t serial=0;};
std::optional<Cache> cache;
struct Creation {World world;action_session::Context context;action_session::Binding binding;uintptr_t log=0,asset=0,sequence=0;std::string id;bool entered=false;};
thread_local Creation* creation=nullptr;
void clear(){cache.reset();}
bool bind_cache(const World& w,const action_session::Context& c,std::string& e){
    if(cache&&(!same_world(cache->world,w)||cache->context!=c))return fail(e,"tutorial lifecycle cache has different full Origin/Context");
    if(!cache)cache=Cache{w,c,{},0};return true;
}
bool array(uintptr_t owner,size_t offset,int32_t maximum,uintptr_t& data,int32_t& count,std::string& e){
    int32_t capacity=0;if(!at(owner,offset,data,e)||!at(owner,offset+8,count,e)||!at(owner,offset+12,capacity,e))return false;
    return (count>=0&&count<=maximum&&capacity>=count&&capacity<=maximum&&(!capacity||data))||fail(e,"tutorial lifecycle owned array count/capacity differs");
}
struct Owners {uintptr_t log=0,library=0,asset=0;std::vector<uintptr_t> instances;size_t completed=0;};
bool log_owners(uintptr_t state,const std::string& id,Owners& out,std::string& e){
    uintptr_t data=0;int32_t count=0;out.instances.clear();out.completed=0;
    if(!at(state,0x1e8,out.log,e)||!type(out.log,0x16fd7b0,e)||!array(out.log,0x20,64,data,count,e))return false;
    for(int32_t i=0;i<count;++i){uintptr_t instance=0;if(!at(data,size_t(i)*8,instance,e))return false;out.instances.push_back(instance);}
    if(!array(out.log,0x50,512,data,count,e))return false;
    for(int32_t i=0;i<count;++i){std::string complete;if(!str(data,size_t(i)*32,complete,e))return false;if(complete==id)++out.completed;}
    return out.completed<=1||fail(e,"tutorial completed ID occurs more than once");
}
bool owners(uintptr_t state,uintptr_t mode,const std::string& id,Owners& out,std::string& e){
    out={};uintptr_t singleton=0,data=0;int32_t count=0;
    if(!supported(id))return fail(e,"tutorial lifecycle scenario is not fixed Initial/LevelTwo");
    if(!at(state,0x1e8,out.log,e)||!type(out.log,0x16fd7b0,e)||
       !at(mode,0x48,out.library,e)||!type(out.library,0x171bd80,e)||
       !at(image,0x13df560,singleton,e))return false;
    if(singleton!=out.library)return fail(e,"tutorial ScenarioLibrary reciprocal Mode owner differs");
    if(!array(out.library,0x20,4096,data,count,e))return false;
    size_t found=0;
    for(int32_t i=0;i<count;++i){uintptr_t asset=0;std::string actual;if(!at(data,size_t(i)*8,asset,e)||!str(asset,0x18,actual,e))return false;
        if(actual!=id)continue;++found;out.asset=asset;}
    if(found!=1||!type(out.asset,0x1720510,e))return fail(e,e.empty()?"tutorial scenario is not uniquely owned by Mode library":e);
    std::string filename;uint8_t repeatable=0;
    if(!str(out.asset,0x60,filename,e)||!at(out.asset,0x58,repeatable,e))return false;
    if(filename!="scenarios/"+id||repeatable)return fail(e,"tutorial scenario filename/nonrepeatable contract differs");
    return log_owners(state,id,out,e);
}
bool log_owners(const World& w,const std::string& id,Owners& out,std::string& e){return log_owners(reinterpret_cast<uintptr_t>(w.state),id,out,e);}
bool owners(const World& w,const std::string& id,Owners& out,std::string& e){return owners(reinterpret_cast<uintptr_t>(w.state),reinterpret_cast<uintptr_t>(w.mode),id,out,e);}
bool bootstrap_owners(const std::string& id,Owners& out,std::string& e){
    if(!load.armed||load.ready||creation||GetCurrentThreadId()!=mainthread)return fail(e,"Client tutorial bootstrap requires unscoped armed load on bound Mainthread");
    uintptr_t base=0;uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(base,thread,sha,e))return false;
    if(base!=image||thread!=mainthread||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(e,"Client tutorial bootstrap resolver image/thread/executable differs");
    const auto request=building_mixed_identity::opened_file::status();
    if(!request.error.empty())return fail(e,request.error);
    if(!request.initialized||request.hook_mask!=building_mixed_identity::opened_file::required_hooks||!request.native_request_attempted||request.game_thread!=mainthread||request.uuid!=load.uuid||request.generation!=load.generation||request.state=="failed"||request.state=="invalidated")
        return fail(e,"Client tutorial bootstrap NativeOpen request identity/state differs");
    uintptr_t game=0,mode=0,active=0,state=0,log=0;int32_t mode_state=-1;
    if(!at(image,0x13df8f0,game,e)||!type(game,0x1743740,e)||!at(image,0x13df320,mode,e)||!type(mode,0x16e6730,e)||
       !at(game,0x28,active,e)||!at(mode,0x2c4,mode_state,e)||!at(mode,0x58,state,e)||!type(state,0x16de920,e)||!at(image,0x13df330,log,e))return false;
    if(active!=mode||(mode_state!=3&&mode_state!=4))return fail(e,"Client tutorial bootstrap active Mode/loading state differs");
    // Only native owner identity is needed to suppress an automatic start.
    // This path never publishes a completed world or admits a Host-frame effect.
    if(!owners(state,mode,id,out,e))return false;
    return out.log==log||fail(e,"Client tutorial bootstrap ScenarioLog reciprocal owner differs");
}
bool owned_cursor(const Owners& owner,const std::string& id,NativeCursor& out,bool& present,std::string& e){
    out={};present=false;
    for(const auto instance:owner.instances){std::string actual;if(!str(instance,0x18,actual,e))return false;if(actual!=id)continue;
        if(present)return fail(e,"tutorial supported instance is duplicated");present=true;
        out.instance=instance;out.value.scenario=id;uintptr_t history=0;int32_t history_count=0;uint8_t complete=0;
        if(!type(instance,0x173ce70,e)||!at(instance,0x58,out.sequence,e)||!type(out.sequence,0x16dbd80,e)||
           !array(out.sequence,0x20,2048,out.steps,out.value.count,e)||!at(out.sequence,0x50,out.value.next,e)||!at(out.sequence,0x54,complete,e)||
           !str(instance,0x60,out.value.last,e)||!array(instance,0x48,512,history,history_count,e))return false;
        if(complete>1||out.value.count!=(id=="initial_tutorial"?107:17)||out.value.next<0||out.value.next>out.value.count)return fail(e,"tutorial native fixed sequence count/index/complete differs");
        out.value.complete=complete!=0;
        for(int32_t i=0;i<history_count;++i){std::string entry;if(!str(history,size_t(i)*32,entry,e))return false;out.value.history.push_back(std::move(entry));}
    }return true;
}
bool version(const World& w,uintptr_t& owner,std::array<int32_t,3>& out,std::string& e){
    uintptr_t reciprocal=0;if(!at(reinterpret_cast<uintptr_t>(w.state),0x1c8,owner,e)||!type(owner,0x1715450,e)||!at(image,0x13df500,reciprocal,e))return false;
    if(reciprocal!=owner)return fail(e,"tutorial version SoftwareStatus reciprocal owner differs");
    for(size_t i=0;i<3;++i)if(!at(owner,0x88+i*4,out[i],e)||out[i]<0)return fail(e,e.empty()?"tutorial version has negative native component":e);
    return true;
}
bool decorate(const World& w,const action_session::Context& c,State& out,std::string& e){
    if(lifecycle::find(out.active,0)){uintptr_t software=0;std::array<int32_t,3> values{};if(!version(w,software,values,e))return false;out.version=values;}
    if(!cache)return true;
    if(!same_world(cache->world,w)||cache->context!=c)return fail(e,"tutorial lifecycle observation has stale full Origin/Context");
    out.lifecycle_order=cache->proof.lifecycle_order;out.leveltwo_initial_next=cache->proof.leveltwo_initial_next;
    for(size_t i=0;i<2;++i){const auto* terminal=lifecycle::find(cache->proof.terminal,i);
        if(terminal&&lifecycle::completed(out,i)){out.terminal.push_back(*terminal);if(i==0){out.terminal_startingpoint=cache->proof.terminal_startingpoint;out.terminal_version=cache->proof.terminal_version;}}
        else out.lifecycle_order[i][1]=0;
    }return true;
}
void remember_terminal(uintptr_t sequence);
void scenario_start(void*,void*,void*);
void advance(void*);
}
