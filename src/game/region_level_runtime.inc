namespace levels=region_levels;
namespace undo=build_actions::private_native;
using LevelDrag=void(__fastcall*)(void*,void*,const float*,float);
std::atomic<LevelDrag> level_drag{nullptr};
json level_json(const action::RegionLevelState& state){return {{"counts",state.counts},{"roster_sha256",state.roster_sha256},{"levels_sha256",state.levels_sha256},{"expected_sha256",state.expected_sha256},{"global_random",state.global_random}};}
bool level_state(Runtime& r,const LocalView& view,const std::array<int32_t,2>& range,action::RegionLevelState& out,uintptr_t& global,std::string& e){
    levels::Census census;
    const auto read_field=[&](uintptr_t p,size_t o,auto& v,std::string& error){return at(r,p,o,v,error);};
    if(!levels::census(r.image,view.region,range,census,read_field,e))return false;
    action::RegionLevelState value;value.counts=census.counts;
    if(!sha256(census.roster.dump(),value.roster_sha256,e)||!sha256(census.levels.dump(),value.levels_sha256,e)||
       !sha256(census.expected.dump(),value.expected_sha256,e)||!at(r,r.image,0x164b250,global,e))return false;
    if(!global)return fail(e,"region level native global RNG singleton absent");if(!at(r,global,0,value.global_random,e))return false;
    out=std::move(value);return true;
}
uint32_t level_window_find(uintptr_t image,uintptr_t& window){
    const NativeString name{"level_select",12,{}};
    __try{window=uintptr_t(reinterpret_cast<void*(__fastcall*)(const NativeString*)>(image+0x81aa90)(&name));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool level_inputs(Runtime& r,uintptr_t expected,uintptr_t& cursor,uintptr_t& collision,uintptr_t& window,uintptr_t& widget,std::array<int32_t,3>& prior,std::array<int32_t,3>& domain,std::string& e){
    uintptr_t ap=0;
    if(!at(r,r.image,0x13df9d0,cursor,e)||!at(r,cursor,0,ap,e)||ap!=r.image+0x1786dd0||
       (expected&&cursor!=expected)||!at(r,cursor,0x230,collision,e)||!collision)return fail(e,e.empty()?"region level actual Cursor/collision differs":e);
    const auto seh=level_window_find(r.image,window);
    if(seh||!window)return fail(e,"region level original Window.Find failed SEH="+std::to_string(seh));
    if(!at(r,window,0,ap,e)||ap!=r.image+0x1724450||!at(r,window,0x4f0,widget,e)||!at(r,widget,0,ap,e)||ap!=r.image+0x17241f0||
       !at(r,widget,0x348,prior,e)||!at(r,widget,0x354,domain,e))return fail(e,e.empty()?"region level actual range window/widget differs":e);
    return ((domain[2]&255)==1&&domain[0]>=1&&domain[1]>=domain[0]&&domain[1]<=INT32_MAX/10-10)||fail(e,"region level native widget allowed range invalid");
}
bool level_domain(const std::array<int32_t,2>& requested,const std::array<int32_t,3>& domain,std::string& e){
    return (requested[0]>=domain[0]&&requested[1]<=domain[1]&&requested[0]<=requested[1])||fail(e,"region levels outside original widget allowed range actual="+std::to_string(requested[0])+".."+std::to_string(requested[1])+" required="+std::to_string(domain[0])+".."+std::to_string(domain[1]));
}
uint32_t level_cache(void* brush,uintptr_t region){__try{*reinterpret_cast<uintptr_t*>(uintptr_t(brush)+0x150)=region;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
void level_entry(void* brush,void* supplied_cursor,const float* hit,float dt){
    auto* r=published.load(std::memory_order_acquire);const auto original=level_drag.load(std::memory_order_acquire);
    if(!r||!properties.ready||properties.replaying||!net::session_wire::connected()){original(brush,supplied_cursor,hit,dt);return;}
    std::string e;uintptr_t cursor=0,collision=0,window=0,widget=0,ap=0,active=0,region=0,previous=0,borrowed_window=0;
    std::array<int32_t,3> range{},domain{};action::RegionPropertyIntent intent;intent.kind=action::RegionPropertyKind::LevelRange;uint8_t owned=0;
    if(!level_inputs(*r,uintptr_t(supplied_cursor),cursor,collision,window,widget,range,domain,e)||
       !at(*r,uintptr_t(brush),0,ap,e)||ap!=r->image+0x1741210||!at(*r,cursor,0x190,active,e)||active!=uintptr_t(brush)||
       !at(*r,uintptr_t(brush),0x148,borrowed_window,e)||borrowed_window!=window||!at(*r,collision,0x1d8,region,e)||
       !at(*r,uintptr_t(brush),0x150,previous,e)){property_rejected(e.empty()?"region level actual tool/owner differs":e);return;}
    if(!region||region==previous)return;
    if(!at(*r,region,0x850,owned,e)){property_rejected(e);return;}
    if(!owned)return;
    intent.levels={range[0],range[1]};std::array<int32_t,2> current{};
    if((range[2]&255)!=1||!level_domain(intent.levels,domain,e)||!at(*r,region,0x838,current,e)||
       !at(*r,region,0x834,intent.id,e)||!hit||!r->memory.copy(uintptr_t(hit),intent.hit.data(),12,e)){property_rejected(e.empty()?"region level captured range/hit invalid":e);return;}
    if(current==intent.levels)return;
    if(!property_capture(*r,intent,region,e)){property_rejected(e);return;}
    const auto seh=level_cache(brush,region);if(seh)property_rejected("region level capture cache SEH="+std::to_string(seh));
}
bool install_levels(Runtime& r,std::string& e){
    constexpr unsigned char expected[]{0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81};std::array<unsigned char,8> actual{};
    if(!r.memory.copy(r.image+0x5d8480,actual.data(),actual.size(),e)||std::memcmp(actual.data(),expected,sizeof expected))return fail(e,e.empty()?"region level original Drag bytes differ":e);
    LevelDrag original=nullptr;const auto created=MH_CreateHook(reinterpret_cast<void*>(r.image+0x5d8480),reinterpret_cast<void*>(&level_entry),reinterpret_cast<void**>(&original));
    if(created!=MH_OK||!original)return fail(e,"region level Drag hook create status="+std::to_string(created));
    level_drag.store(original,std::memory_order_release);const auto enabled=MH_EnableHook(reinterpret_cast<void*>(r.image+0x5d8480));
    return enabled==MH_OK||fail(e,"region level Drag hook enable status="+std::to_string(enabled));
}
uint32_t level_restore(uintptr_t image,uintptr_t collision,uintptr_t prior_region,uintptr_t widget,const std::array<int32_t,3>& prior){
    __try{*reinterpret_cast<uintptr_t*>(collision+0x1d8)=prior_region;std::memcpy(reinterpret_cast<void*>(widget+0x348),prior.data(),12);
        reinterpret_cast<void(__fastcall*)(void*)>(image+0x7299c0)(reinterpret_cast<void*>(widget));
        return *reinterpret_cast<uintptr_t*>(collision+0x1d8)==prior_region&&std::memcmp(reinterpret_cast<const void*>(widget+0x348),prior.data(),12)==0?0:0xE0010005;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t level_original(uintptr_t image,uintptr_t cursor,uintptr_t collision,uintptr_t window,uintptr_t widget,uintptr_t region,PropertyOperation& op,uint32_t& restore){
    uintptr_t prior_region=0;std::array<int32_t,3> prior{};bool borrowed=false;uint32_t original=0;restore=0;
    __try{
        op.level_brush=uintptr_t(reinterpret_cast<void*(__fastcall*)(size_t)>(image+0xa02670)(0x178));
        if(!op.level_brush)return 0xE0010004;
        reinterpret_cast<void(__fastcall*)(void*)>(image+0x5d82f0)(reinterpret_cast<void*>(op.level_brush));op.level_constructed=true;
        *reinterpret_cast<uintptr_t*>(op.level_brush+0x148)=window;
        prior_region=*reinterpret_cast<uintptr_t*>(collision+0x1d8);std::memcpy(prior.data(),reinterpret_cast<const void*>(widget+0x348),12);borrowed=true;
        *reinterpret_cast<uintptr_t*>(collision+0x1d8)=region;
        const auto& range=op.request.region_property.levels;
        reinterpret_cast<void(__fastcall*)(void*,int32_t,int32_t)>(image+0x729ae0)(reinterpret_cast<void*>(widget),range[0],range[1]);
        op.attempted=true;
        reinterpret_cast<CursorProperty>(image+0x5d7cc0)(reinterpret_cast<void*>(op.level_brush),reinterpret_cast<void*>(cursor),op.request.region_property.hit.data());
        op.level_graph=*reinterpret_cast<uintptr_t*>(op.level_brush+0x170);
        reinterpret_cast<CursorProperty>(image+0x5d83a0)(reinterpret_cast<void*>(op.level_brush),reinterpret_cast<void*>(cursor),op.request.region_property.hit.data());op.returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){original=GetExceptionCode();}
    if(borrowed)restore=level_restore(image,collision,prior_region,widget,prior);
    // An uncertain original graph is retained for inspection, never destroyed or replayed.
    if(!original&&!restore&&op.level_constructed){__try{reinterpret_cast<void(__fastcall*)(void*)>(image+0x5d7dc0)(reinterpret_cast<void*>(op.level_brush));op.level_destroyed=true;
        reinterpret_cast<void(__fastcall*)(void*,size_t)>(image+0x1264b60)(reinterpret_cast<void*>(op.level_brush),0x178);}__except(EXCEPTION_EXECUTE_HANDLER){original=GetExceptionCode();}}
    return original;
}
bool level_inverse(Runtime& r,const LocalView& view,const action::RegionPropertyState& before,const undo::UndoWitness& a,const undo::UndoWitness& b,uintptr_t expected,std::string& e){
    undo::GraphWitness graph;uintptr_t data=0,op=0,ap=0,region=0;int32_t type=-1;std::array<int32_t,2> range{};
    if(b.undo.empty()||b.undo.back()!=expected)return fail(e,"region level original Undo graph absent or differs");
    if(!undo::read_graph(r.memory,r.image,expected,graph,e)||!undo::validate_undo_commit(a,b,graph,e))return false;
    if(graph.operations!=1||!at(r,expected,0x20,data,e)||!at(r,data,0,op,e)||!at(r,op,0,ap,e)||
       !at(r,op,0xb8,region,e)||!at(r,op,0xc0,type,e)||!at(r,op,0xc4,range,e))return fail(e,e.empty()?"region level Undo requires one DoConfigureRegion":e);
    return (ap==r.image+0x1723230&&region==view.region&&type==before.type&&range==std::array<int32_t,2>{before.level,before.max_level})||fail(e,"region level original Undo inverse Region/type/range differs");
}
action::Outcome level_perform(Runtime& r,const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    const auto& intent=request.region_property;LocalView view;action::RegionPropertyState before;action::Ledger money;action::RegionLevelState actors;uintptr_t global=0;
    uintptr_t cursor=0,collision=0,window=0,widget=0,map=0,found=0;std::array<int32_t,3> prior{},domain{};undo::UndoWitness undo_before;
    if(!property_read(r,intent.id,0,before,view,e)||!bound(view,binding,request.snapshot_uuid,e)||!ledger(money,e)||
       !level_inputs(r,0,cursor,collision,window,widget,prior,domain,e)||!level_domain(intent.levels,domain,e)||
       !level_state(r,view,intent.levels,actors,global,e)||!undo::read_undo(r.memory,r.image,undo_before,e))return action::Outcome::RejectedNoEffect;
    before.actors=actors;
    if(!before.owned||undo_before.active_graph||(before.level==intent.levels[0]&&before.max_level==intent.levels[1])){e="region level action requires owned Region, changed range and idle Undo";return action::Outcome::RejectedNoEffect;}
    if(!at(r,r.image,0x13df8c0,map,e))return action::Outcome::RejectedNoEffect;
    const auto hit=property_hit_region(r.image,map,intent.hit.data(),found);
    if(hit||found!=view.region){e="region level hit resolves another Region SEH="+std::to_string(hit);return action::Outcome::RejectedNoEffect;}
    if(authority&&(authority->before!=money||!property_same_before(before,authority->property_before))){e="region level Client actor roster/levels/Region/Ledger differs from Host before";return action::Outcome::RejectedNoEffect;}
    PropertyOperation operation;operation.binding=binding;operation.request=request;operation.evidence.before=money;operation.evidence.property_before=authority?authority->property_before:before;
    auto& saved=properties.operations.emplace(request.action_uuid,std::move(operation)).first->second;
    auto unknown=[&](const std::string& cause){e=saved.cause=cause;return action::Outcome::Unknown;};
    if(authority){std::memcpy(reinterpret_cast<void*>(view.random),authority->property_before.content.random.data(),32);
        std::memcpy(reinterpret_cast<void*>(view.region+0x268),authority->property_before.type_random.data(),32);
        std::memcpy(reinterpret_cast<void*>(global),authority->property_before.actors->global_random.data(),32);}
    properties.replaying=true;uint32_t restore=0;const auto original=level_original(r.image,cursor,collision,window,widget,view.region,saved,restore);properties.replaying=false;
    if(original||restore||!saved.attempted||!saved.returned||!saved.level_destroyed)return unknown("region level original Press/Drag/Release/cleanup failed SEH="+std::to_string(original)+" restore="+std::to_string(restore));
    action::RegionPropertyState after;LocalView after_view;action::RegionLevelState after_actors;uintptr_t after_global=0;undo::UndoWitness undo_after;
    if(!property_read(r,intent.id,view.region,after,after_view,e)||!bound(after_view,binding,request.snapshot_uuid,e)||!level_state(r,after_view,intent.levels,after_actors,after_global,e)||
       !ledger(saved.evidence.after,e)||!undo::read_undo(r.memory,r.image,undo_after,e)||!level_inverse(r,view,before,undo_before,undo_after,saved.level_graph,e))return unknown("region level native consequence/inverse readback failed: "+e);
    after.actors=after_actors;saved.evidence.property_after=after;saved.evidence.undo_confirmed=true;
    if(!sha256(json::array().dump(),saved.evidence.region_retired_sha256,e))return unknown(e);
    auto expected=before;expected.level=intent.levels[0];expected.max_level=intent.levels[1];expected.content.random=after.content.random;expected.type_random=after.type_random;expected.actors=after.actors;
    if(after!=expected||actors.roster_sha256!=after_actors.roster_sha256||actors.counts!=after_actors.counts||actors.expected_sha256!=after_actors.levels_sha256||after_actors.expected_sha256!=after_actors.levels_sha256||money!=saved.evidence.after)
        return unknown("region level original actor clamp/roster/Region/Ledger differs");
    if(authority&&(saved.evidence.property_after!=authority->property_after||saved.evidence.after!=authority->after||saved.evidence.region_retired_sha256!=authority->region_retired_sha256))return unknown("region level original consequence differs from Host");
    saved.confirmed=true;return action::Outcome::Confirmed;
}
uint32_t level_ui_input(uintptr_t image,unsigned step,const placement_ui_probe::Binding& b,const action::RegionPropertyIntent& intent,uintptr_t widget){
    __try{if(step==0)reinterpret_cast<void(__fastcall*)(void*)>(image+0x580870)(reinterpret_cast<void*>(b.button));
        else if(step==1)reinterpret_cast<void(__fastcall*)(void*,int32_t,int32_t)>(image+0x729ae0)(reinterpret_cast<void*>(widget),intent.levels[0],intent.levels[1]);
        else reinterpret_cast<CursorProperty>(image+(step==2?0x5d7cc0:0x5d83a0))(reinterpret_cast<void*>(b.behaviour),reinterpret_cast<void*>(b.cursor),intent.hit.data());return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t level_ui_borrow(uintptr_t collision,uintptr_t region,bool& borrowed){
    __try{*reinterpret_cast<uintptr_t*>(collision+0x1d8)=region;borrowed=true;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool level_ui(Runtime& r,const std::string& token,const action::RegionPropertyIntent& intent,json& out,std::string& e){
    if(properties.ui_attempts.contains(token))return fail(e,"region level UI token already attempted; inspect receipt before retry");
    action::RegionPropertyState before;LocalView view;uintptr_t cursor=0,collision=0,window=0,widget=0,map=0,found=0,prior_region=0;std::array<int32_t,3> prior{},domain{};
    if(intent.name!=""||intent.type!=-1||!property_read(r,intent.id,0,before,view,e)||!before.owned||
       !level_inputs(r,0,cursor,collision,window,widget,prior,domain,e)||!level_domain(intent.levels,domain,e)||
       !at(r,r.image,0x13df8c0,map,e)||!at(r,collision,0x1d8,prior_region,e))return fail(e,e.empty()?"region level UI requires owned Region and native range":e);
    for(float x:intent.hit)if(!std::isfinite(x)||std::abs(x)>1000000)return fail(e,"region level UI hit outside finite bounds");
    const auto hit=property_hit_region(r.image,map,intent.hit.data(),found);if(hit||found!=view.region)return fail(e,"region level UI hit resolves another Region");
    placement_ui_probe::Context context{r.image,uintptr_t(view.world.origin.state),view.world.origin.world_epoch,view.world.origin.generation,r.thread};
    placement_ui_probe::Request request;request.token=token;request.family=placement_ui_probe::Family::RegionLevel;
    placement_ui_probe::ProcessMemory memory;placement_ui_probe::Binding tool;
    if(!placement_ui_probe::read_existing_tool(memory,context,request,tool,e))return false;
    properties.ui_attempts.insert(token);const auto captured=properties.captured;out={{"attempted",0},{"returned",0},{"captured_once",false},{"input_restored",false}};
    auto call=[&](unsigned step){out["attempted"]=out.at("attempted").get<unsigned>()+1;const auto seh=level_ui_input(r.image,step,tool,intent,widget);if(seh)return fail(e,"region level UI callback failed step="+std::to_string(step)+" SEH="+std::to_string(seh));out["returned"]=out.at("returned").get<unsigned>()+1;return true;};
    bool ok=call(0);uintptr_t active=0,actual_window=0;
    if(ok)ok=at(r,cursor,0x190,active,e)&&active==tool.behaviour&&at(r,active,0x148,actual_window,e)&&actual_window==window;
    if(ok)ok=call(1);
    bool borrowed=false;if(ok){const auto seh=level_ui_borrow(collision,view.region,borrowed);ok=!seh;if(seh)e="region level UI collision borrow SEH="+std::to_string(seh);}
    if(ok)ok=call(2);
    if(borrowed){const auto original=e;const bool released=call(3);if(!original.empty())e=original+(released?"":"; "+e);ok=ok&&released;}
    const auto restore=level_restore(r.image,collision,prior_region,widget,prior);out["input_restored"]=restore==0;
    if(restore){e+="; region level UI restoration SEH="+std::to_string(restore);ok=false;}
    const bool once=properties.captured==captured+1&&properties.pending&&*properties.pending==intent;out["captured_once"]=once;
    action::RegionPropertyState after;LocalView after_view;if(!property_read(r,intent.id,view.region,after,after_view,e))ok=false;
    out["local_state_unchanged"]=after==before;
    if(!ok||!once||after!=before)return fail(e,e.empty()?"region level original UI did not capture one unchanged Host-ordered request: "+properties.cause:e);
    return true;
}
