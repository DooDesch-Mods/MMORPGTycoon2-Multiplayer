using HeightDrag=void(*)(void*,void*,const void*,float);
HeightDrag height_drag=nullptr;
void height_entry(void*,void*,const void*,float);
bool height_code(region::Runtime& r,bool hooked,std::string& e){
    for(size_t i=0;i<std::size(height_codes);++i){const auto& code=height_codes[i];std::vector<uint8_t> bytes(code.size);if(!r.memory.copy(r.image+code.rva,bytes.data(),bytes.size(),e))return false;
        if(!hooked||i){if(std::memcmp(bytes.data(),code.bytes,code.size))return fail(e,"height exact native body differs RVA="+std::to_string(code.rva));continue;}
        if(!height_drag||bytes[0]!=0xe9||std::memcmp(bytes.data()+5,code.bytes+5,code.size-5))return fail(e,"height owned original hook target/tail differs");
        const auto original=uintptr_t(height_drag);std::vector<uint8_t> prefix(code.span);if(!r.memory.copy(original,prefix.data(),prefix.size(),e)||std::memcmp(prefix.data(),code.bytes,code.span))return fail(e,"height original trampoline prefix differs");
        constexpr std::array<uint8_t,6> jump{0xff,0x25,0,0,0,0};std::array<uint8_t,6> actual{};uintptr_t continuation=0,entry=0;int32_t displacement=0;
        if(!r.memory.copy(original+code.span,actual.data(),actual.size(),e)||actual!=jump||!r.memory.copy(original+code.span+6,&continuation,8,e)||continuation!=r.image+code.rva+code.span)return fail(e,"height original trampoline continuation differs");
        std::memcpy(&displacement,bytes.data()+1,4);const auto relay=uintptr_t(int64_t(r.image+code.rva+5)+displacement);
        if(!r.memory.copy(relay,actual.data(),actual.size(),e)||actual!=jump||!r.memory.copy(relay+6,&entry,8,e)||entry!=uintptr_t(&height_entry))return fail(e,"height native hook relay owner differs");
    }
    const std::array<float,2> wanted{20.0f,-20.0f};std::array<float,2> actual{};
    return region::at(r,r.image,0x14f4aa0,actual,e)&&(actual==wanted||fail(e,"height original step constants differ"));
}
bool height_enable(region::Runtime& r,std::string& e){
    if(!height_code(r,false,e))return false;
    const auto create=MH_CreateHook(reinterpret_cast<void*>(r.image+0x5e1860),reinterpret_cast<void*>(&height_entry),reinterpret_cast<void**>(&height_drag));
    if(create!=MH_OK||!height_drag)return fail(e,"height original Drag hook creation failed status="+std::to_string(create));
    const auto enable=MH_EnableHook(reinterpret_cast<void*>(r.image+0x5e1860));
    return enable==MH_OK?height_code(r,true,e):fail(e,"height original Drag hook enable failed status="+std::to_string(enable));
}
bool height_grid(region::Runtime& r,uintptr_t receiver,terrain_height::Grid& grid,std::string& digest,std::string& e){
    if(!terrain_height::read(r.image,receiver,grid,[&](uintptr_t base,size_t offset,auto& value,std::string& error){return region::at(r,base,offset,value,error);},e))return false;
    std::vector<uint32_t> bits;for(float value:grid.values)bits.push_back(std::bit_cast<uint32_t>(value));
    std::array<uint32_t,3> origin{};for(size_t i=0;i<3;++i)origin[i]=std::bit_cast<uint32_t>(grid.origin[i]);
    return region::sha256(json{{"width",grid.width},{"height",grid.height},{"cell_size_bits",std::bit_cast<uint32_t>(grid.cell_size)},{"origin_bits",origin},{"height_bits",bits}}.dump(),digest,e);
}
bool height_cursor(region::Runtime& r,uintptr_t expected,uintptr_t& cursor,uintptr_t& model,std::string& e){
    uintptr_t ap=0;if(!region::at(r,r.image,0x13df9d0,cursor,e)||!cursor||(expected&&cursor!=expected)||!region::at(r,cursor,0,ap,e)||ap!=r.image+0x1786dd0||!region::at(r,cursor,0x160,model,e)||!model||!region::at(r,model,0,ap,e)||ap!=r.image+0x1749490)return fail(e,e.empty()?"height actual Cursor/model owner differs":e);return true;
}
uint32_t height_cache(void* brush,const std::array<float,3>& point){__try{std::memcpy(static_cast<unsigned char*>(brush)+0x14c,point.data(),12);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t height_lookup(uintptr_t image,uintptr_t map,const std::array<float,3>& hit,uintptr_t& receiver){__try{receiver=uintptr_t(reinterpret_cast<void*(*)(void*,const void*,void*)>(image+0x32a990)(reinterpret_cast<void*>(map),hit.data(),nullptr));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool height_region_at(region::Runtime& r,const std::array<float,3>& hit,uintptr_t expected,uintptr_t& receiver,std::string& e){
    uintptr_t map=0,ap=0;if(!region::at(r,r.image,0x13df8c0,map,e)||!region::at(r,map,0,ap,e)||ap!=r.image+0x1742860)return fail(e,e.empty()?"height actual Map owner differs":e);
    const auto seh=height_lookup(r.image,map,hit,receiver);return native_error(seh,"height GetRegionAt",e)&&(!expected||receiver==expected||fail(e,"height hit resolves to another native Region"));
}
void height_entry(void* brush,void* supplied_cursor,const void* position,float delta){
    if(!terrain.ready||!net::session_wire::connected()){height_drag(brush,supplied_cursor,position,delta);return;}
    std::string e;auto* r=runtime(e);const auto controller=typed_action_runtime::status();
    if(!r||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||terrain.capture||terrain.pending){rejected(e.empty()?"height requires idle bound Controller":e);return;}
    if(!height_code(*r,true,e)){rejected(e);return;}
    uintptr_t cursor=0,model=0,active=0,ap=0;uint8_t up=0;action::TerrainHeightIntent h;
    if(!height_cursor(*r,uintptr_t(supplied_cursor),cursor,model,e)||!region::at(*r,cursor,0x190,active,e)||active!=uintptr_t(brush)||!region::at(*r,active,0,ap,e)||ap!=r->image+0x1740010||!region::at(*r,active,0x148,up,e)||up>1||!region::at(*r,active,0x14c,h.previous,e)||!region::at(*r,model,0xe0,h.picked,e)||!region::at(*r,cursor,0x170,h.radius,e)||!r->memory.copy(uintptr_t(position),h.hit.data(),12,e)){rejected(e.empty()?"height original active tool/input differs":e);return;}
    if(h.picked==h.previous)return;
    h.up=up!=0;uintptr_t receiver=0;
    // Region selection uses the original Drag hit; the selected cell uses the
    // original Cursor model translation. They are separate native inputs.
    region::LocalView before;int32_t id=-1;terrain_height::Grid grid;
    if(!height_region_at(*r,h.hit,0,receiver,e)||!region::at(*r,receiver,0x834,id,e)||!region::read(*r,id,receiver,before,e)||!height_grid(*r,receiver,grid,h.before_sha256,e)||!terrain_height::index(grid,h.picked,h.cell,e)){rejected(e);return;}
    Stroke stroke;stroke.brush=uintptr_t(brush);stroke.intent.id=id;stroke.intent.height=h;stroke.binding={before.world.origin.world_epoch,before.world.origin.generation};stroke.snapshot=net::current_snapshot_id();stroke.ui_token=terrain.ui_token;
    if(!action::valid_terrain(stroke.intent)){rejected("height captured native input outside typed domain");return;}
    // Preserve the native per-cell guard when capture delays actual effects
    // until the Host grants the request. Holding the same cell cannot replay.
    if(!native_error(height_cache(brush,h.picked),"height cell cache",e)){rejected(e);return;}
    terrain.pending=std::move(stroke);++terrain.captured;
}
uint32_t height_original(TerrainOperation& op,uintptr_t image,uintptr_t cursor,uintptr_t model,uint32_t& restore){
    std::array<float,3> prior{};float radius=0;bool borrowed=false;uint32_t original=0;restore=0;
    __try{
        reinterpret_cast<Unary>(image+0x5e16d0)(op.brush->bytes);op.constructed=true;
        const auto& h=*op.request.terrain.height;
        op.brush->bytes[0x148]=h.up?1:0;std::memcpy(op.brush->bytes+0x14c,h.previous.data(),12);
        std::memcpy(prior.data(),reinterpret_cast<const void*>(model+0xe0),12);radius=*reinterpret_cast<float*>(cursor+0x170);borrowed=true;
        std::memcpy(reinterpret_cast<void*>(model+0xe0),h.picked.data(),12);*reinterpret_cast<float*>(cursor+0x170)=h.radius;
        height_drag(op.brush->bytes,reinterpret_cast<void*>(cursor),h.hit.data(),0.0f);op.returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){original=GetExceptionCode();}
    if(borrowed){__try{std::memcpy(reinterpret_cast<void*>(model+0xe0),prior.data(),12);*reinterpret_cast<float*>(cursor+0x170)=radius;}__except(EXCEPTION_EXECUTE_HANDLER){restore=GetExceptionCode();}}
    return original;
}
bool height_state(region::Runtime& r,const region::LocalView& view,int32_t cell,action::TerrainState& state,terrain_height::Grid& grid,uintptr_t& global,std::string& e){
    if(!terrain_state(r,view,state,global,e)||!height_grid(r,view.region,grid,state.heights_sha256,e)||cell<0||size_t(cell)>=grid.values.size())return fail(e,e.empty()?"height native selected cell outside array":e);
    state.height_cell=cell;state.height_bits=std::bit_cast<uint32_t>(grid.values[size_t(cell)]);return true;
}
action::Outcome height_perform(const action::Binding& b,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=runtime(e);if(!r||!terrain.ready||request.action!=action::Action::TerrainStroke||!request.terrain.height||!action::valid_terrain(request.terrain)){e="height native runtime/intent unavailable original="+e;return action::Outcome::RejectedNoEffect;}
    if(terrain.operations.contains(request.action_uuid)){e="height action already attempted; no replay";return action::Outcome::Unknown;}
    if(!height_code(*r,true,e))return action::Outcome::RejectedNoEffect;
    const auto controller=typed_action_runtime::status();if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){e="height lacks exact Controller request";return action::Outcome::RejectedNoEffect;}
    const auto& h=*request.terrain.height;region::LocalView before;action::TerrainState state;action::Ledger money;terrain_height::Grid grid;uintptr_t global=0,cursor=0,model=0,receiver=0;undo::UndoWitness undo_before;int32_t cell=-1;
    if(!region::read(*r,request.terrain.id,0,before,e)||!region::bound(before,b,request.snapshot_uuid,e)||!region::ledger(money,e)||!height_state(*r,before,h.cell,state,grid,global,e)||!terrain_height::index(grid,h.picked,cell,e)||!height_cursor(*r,0,cursor,model,e)||!undo::read_undo(r->memory,r->image,undo_before,e))return action::Outcome::RejectedNoEffect;
    if(undo_before.active_graph||money.cash<100||!money.record_present||cell!=h.cell||state.heights_sha256!=h.before_sha256){e="height original idle graph/cash/cell/before digest differs";return action::Outcome::RejectedNoEffect;}
    if(!height_region_at(*r,h.hit,before.region,receiver,e))return action::Outcome::RejectedNoEffect;
    if(!action::terrain_receipt_fits(request,money,before.state,e))return action::Outcome::RejectedNoEffect;
    if(authority&&(money!=authority->before||!action::same_region_content(before.state,authority->region_before)||!action::same_terrain_content(state,authority->terrain_before))){e="height Client before content differs from Host";return action::Outcome::RejectedNoEffect;}
    auto& op=terrain.operations[request.action_uuid];op.binding=b;op.request=request;op.native_cost=100;op.evidence.before=money;op.evidence.region_before=authority?authority->region_before:before.state;op.evidence.terrain_before=authority?authority->terrain_before:state;op.attempted=true;
    auto unknown=[&]{op.cause=e;return action::Outcome::Unknown;};if(authority&&!native_error(seed(before.random,global,*authority),"height authority RNG seed",e))return unknown();
    uint32_t restore=0;const auto original=height_original(op,r->image,cursor,model,restore);
    if(original||restore){e="height original Drag SEH="+std::to_string(original)+" input restoration SEH="+std::to_string(restore);return unknown();}
    undo::UndoWitness undo_after;region::LocalView after;terrain_height::Grid after_grid;uintptr_t after_global=0,actual_cursor=0,actual_model=0;
    if(!height_cursor(*r,cursor,actual_cursor,actual_model,e))return unknown();
    if(actual_model!=model){e="height original Cursor model receiver changed after callback";return unknown();}
    if(!region::read(*r,request.terrain.id,before.region,after,e)||!region::bound(after,b,request.snapshot_uuid,e)||!region::ledger(op.evidence.after,e)||!height_state(*r,after,h.cell,op.evidence.terrain_after,after_grid,after_global,e)||!undo::read_undo(r->memory,r->image,undo_after,e)||!terrain_height::delta(grid,after_grid,h.cell,h.up,e))return unknown();
    if(undo_after!=undo_before){e="height original Undo witness changed; required unchanged native history";return unknown();}
    op.evidence.region_after=after.state;op.evidence.undo_confirmed=true;
    if(!region::sha256(json::array().dump(),op.evidence.region_retired_sha256,e))return unknown();
    if(!action::terrain_height_effect(request,op.evidence,e))return unknown();
    if(authority&&(op.evidence.region_after!=authority->region_after||op.evidence.terrain_after!=authority->terrain_after||op.evidence.after!=authority->after||op.evidence.region_retired_sha256!=authority->region_retired_sha256)){e="height original consequence differs from Host";return unknown();}
    op.confirmed=true;return action::Outcome::Confirmed;
}
bool height_observe(const action::Binding& b,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=runtime(e);if(!r)return false;const auto found=terrain.operations.find(request.action_uuid);
    if(found==terrain.operations.end()||found->second.binding!=b||found->second.request!=request||!found->second.confirmed)return fail(e,"height observation lacks confirmed exact native operation");
    const auto& op=found->second;region::LocalView actual;action::TerrainState state;terrain_height::Grid grid;action::Ledger money;uintptr_t global=0;
    if(!region::read(*r,request.terrain.id,0,actual,e)||!region::bound(actual,b,request.snapshot_uuid,e)||!height_state(*r,actual,request.terrain.height->cell,state,grid,global,e)||!region::ledger(money,e))return false;
    if(actual.state!=op.evidence.region_after||state!=op.evidence.terrain_after||money!=op.evidence.after)return fail(e,"height observation changed after original return");out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
