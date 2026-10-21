struct RootSceneryReadBatch::State {
    SavedSceneryBatch proof;
    WorldState expected;
    std::optional<opened::CurrentWorld> cached_origin;
    uint64_t origin_revision=0;
    bool fresh_origin=false;
    bool world_ready=false,fresh_world=false;
    bool verify_origin(std::string& error){
        if(!cached_origin)return true;
        struct Fresh {bool& flag;explicit Fresh(bool& value):flag(value){flag=true;}~Fresh(){flag=false;}} fresh(fresh_origin);
        opened::CurrentWorld actual;
        return RootSceneryReadBatch::read_current(actual,error);
    }
    uintptr_t image=0;
    uint32_t thread=0;
    LookupApi api;
    bool same(const WorldState& value)const{
        return same_world(expected.origin,value.origin)&&expected.root==value.root&&expected.selection==value.selection&&
            expected.active_graph==value.active_graph&&expected.saves==value.saves&&expected.edits==value.edits;
    }
    bool admit(const WorldState& value,uintptr_t native_image,uint32_t native_thread,LookupApi lookup_api,std::string& error){
        if(!proof.active()){expected=value;image=native_image;thread=native_thread;api=lookup_api;}
        if(!same(value)||image!=native_image||thread!=native_thread||api.find!=lookup_api.find||api.cast!=lookup_api.cast){
            error="saved Scenery readonly batch complete World or resolver changed";return proof.reject(error);
        }
        return proof.admit(image,uintptr_t(value.origin.map),value.root,
            mt2mp::game::live_host_state::apply_timing::original_call_revision,error);
    }
    bool finish_body(std::string& error){
        if(!proof.active())return true;
        struct FreshWorld {bool& flag;explicit FreshWorld(bool& value):flag(value){flag=true;}~FreshWorld(){flag=false;}} fresh(fresh_world);
        ProcessMemory memory;NativeOrigin origin;LookupApi resolved;
        if(!resolve_lookup(memory,image,mt2mp::native_uid::required_exe_sha,resolved,error))return false;
        Reader reader(memory,origin,image,thread,resolved);WorldState before,after;
        if(!reader.current(before,true,error)||!admit(before,image,thread,resolved,error))return false;
        auto read=[&](uintptr_t pointer,size_t offset,auto& value,std::string& cause){
            if(!pointer||pointer>UINTPTR_MAX-offset){cause="saved Scenery batch final field overflow";return false;}
            return memory.copy(pointer+offset,&value,sizeof value,cause);
        };
        auto capture=[](uintptr_t native_image,uintptr_t map,uintptr_t root,mt2mp::game::region_saved_identity::Index& out,auto& at,std::string& cause){
            mt2mp::game::live_host_state::apply_timing::PageReadScope pages;
            return mt2mp::game::region_saved_identity::capture(native_image,map,root,out,at,cause);
        };
        auto region=[&](const place::SceneryContext& context,place::SceneryWitness& out,std::string& cause){
            struct Host final:place::SceneryHost {
                Reader& reader;State& batch;place::SceneryContext context;
                Host(Reader& r,State& b,const place::SceneryContext& c):reader(r),batch(b),context(c){}
                bool current(place::SceneryContext& out,std::string& why)override{
                    WorldState value;if(!reader.current(value,true,why))return false;
                    if(!batch.same(value)){why="saved Scenery batch World changed during final census";return false;}
                    out=context;return true;
                }
                bool registry_object(uint64_t uid,uintptr_t& object,std::string& why)override{
                    uintptr_t casted=0;const auto code=lookup(batch.api,batch.image,uid,0x1673900,0x164e5c0,0,object,casted);
                    if(code){why="saved_scenery_lookup_SEH_"+std::to_string(code);return false;}
                    if(object!=casted){why="saved_scenery_typed_registry_differs";return false;}return true;
                }
            } owner(reader,*this,context);
            mt2mp::game::live_host_state::apply_timing::PageReadScope fields(!mt2mp::game::live_host_state::apply_timing::PageReadScope::reader_active());
            return place::read_scenery(memory,owner,image,out,cause);
        };
        auto boundary=[](){return mt2mp::game::live_host_state::apply_timing::original_call_revision;};
        if(!proof.finish(boundary,capture,read,region,error))return false;
        if(!reader.current(after,true,error)||!admit(after,image,thread,resolved,error))return false;
        return true;
    }
    bool finish(std::string& error){
        if(!proof.healthy(error))return false;
        if(!verify_origin(error)||!finish_body(error)||!verify_origin(error))return proof.reject(error);
        return true;
    }
};
bool RootSceneryReadBatch::reuse_world(WorldState& out,bool idle,uintptr_t image,uint32_t thread,LookupApi api,bool& reused,std::string& error){
    reused=false;if(!current_)return true;
    auto& state=*current_->state_;
    if(!state.world_ready||state.fresh_world)return true;
    if(!state.proof.healthy(error)||!state.admit(state.expected,image,thread,api,error))return false;
    if(!same_world(out.origin,state.expected.origin)){
        error="saved Scenery readonly cached World origin differs";return state.proof.reject(error);
    }
    if(idle&&(state.expected.saves||state.expected.edits||state.expected.active_graph)){
        error="info: remove_wait_save_edit_or_graph";return state.proof.reject(error);
    }
    out=state.expected;reused=true;return true;
}
bool RootSceneryReadBatch::reuse_current(opened::CurrentWorld& out,bool& reused,std::string& error){
    out={};reused=false;
    if(!current_)return true;
    auto& state=*current_->state_;
    if(!state.proof.healthy(error))return false;
    if(state.cached_origin&&state.origin_revision!=mt2mp::game::live_host_state::apply_timing::original_call_revision){
        error="saved Scenery readonly Origin batch crossed an original native call";return state.proof.reject(error);
    }
    if(state.cached_origin&&!state.fresh_origin){out=*state.cached_origin;reused=true;}
    return true;
}
bool RootSceneryReadBatch::remember_current(const opened::CurrentWorld& value,std::string& error){
    if(!current_)return true;
    auto& state=*current_->state_;
    if(!state.proof.healthy(error))return false;
    if(!state.cached_origin){state.cached_origin=value;state.origin_revision=mt2mp::game::live_host_state::apply_timing::original_call_revision;return true;}
    if(state.origin_revision!=mt2mp::game::live_host_state::apply_timing::original_call_revision){
        error="saved Scenery readonly Origin batch crossed an original native call";return state.proof.reject(error);
    }
    if(!same_world(*state.cached_origin,value)){
        error="saved Scenery readonly batch complete World changed before publication";return state.proof.reject(error);
    }
    return true;
}
bool RootSceneryReadBatch::reject_current(std::string& error){return current_?current_->state_->proof.reject(error):false;}
thread_local RootSceneryReadBatch* RootSceneryReadBatch::current_=nullptr;
RootSceneryReadBatch::RootSceneryReadBatch():previous_(current_){
    state_=previous_?previous_->state_:std::make_shared<State>();current_=this;
}
RootSceneryReadBatch::~RootSceneryReadBatch(){current_=previous_;}
bool RootSceneryReadBatch::finish(std::string& error){return state_->finish(error);}
