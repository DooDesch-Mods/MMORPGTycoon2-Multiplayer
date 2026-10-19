namespace {
using RenameProperty=void(__fastcall*)(void*,const NativeString*);
using CursorProperty=void(__fastcall*)(void*,void*,const float*);
struct PropertyOperation {
    action::Binding binding;
    action::Request request;
    action::Evidence evidence;
    bool attempted=false,returned=false,confirmed=false,retired=false,removed_lookup_absent=false;
    uintptr_t level_brush=0,level_graph=0;
    uintptr_t ownership_graph=0;
    std::optional<action::RegionPropertyState> ownership_before,ownership_after;
    bool level_constructed=false,level_destroyed=false;
    std::vector<uint64_t> removed_uids;
    std::vector<std::string> retired_keys;
    std::string building_uids_sha256_before,building_uids_sha256_after;
    std::string cause;
};
struct PropertyRuntime {
    std::atomic<RenameProperty> rename{nullptr};
    std::atomic<CursorProperty> buy{nullptr},lift{nullptr};
    bool ready=false,replaying=false;
    std::optional<action::RegionPropertyIntent> pending;
    action::Binding pending_binding;
    std::string pending_snapshot,last_action,cause;
    uint64_t captured=0,submitted=0,rejected=0;
    std::map<std::string,PropertyOperation> operations;
    std::set<std::string> ui_attempts;
};
PropertyRuntime properties;
bool property_pending(){return properties.pending.has_value();}
bool appearance_read_native(Runtime&,uintptr_t,action::RegionAppearance&,std::string&);
bool appearance_capture(Runtime&,void*,void*,bool&,std::string&);
uint32_t appearance_original(uintptr_t,uintptr_t,const action::RegionPropertyIntent&,bool&);
bool level_state(Runtime&,const LocalView&,const std::array<int32_t,2>&,action::RegionLevelState&,uintptr_t&,std::string&);
bool install_levels(Runtime&,std::string&);
action::Outcome level_perform(Runtime&,const action::Binding&,const action::Request&,const action::Evidence*,std::string&);
bool level_ui(Runtime&,const std::string&,const action::RegionPropertyIntent&,json&,std::string&);
uint32_t property_native_building_count(uintptr_t image,uintptr_t manager,int32_t& count) {
    __try{count=reinterpret_cast<int32_t(__fastcall*)(void*)>(image+0x24d740)(reinterpret_cast<void*>(manager));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t property_registered_building(uintptr_t image,uint64_t uid,uintptr_t& object) {
    __try {
        auto* base=reinterpret_cast<void*(__fastcall*)(uint64_t)>(image+0x47ce20)(uid);
        object=base?uintptr_t(reinterpret_cast<void*(__fastcall*)(void*,const void*,const void*,ptrdiff_t)>(image+0x12654f0)(
            base,reinterpret_cast<void*>(image+0x1673900),reinterpret_cast<void*>(image+0x1655090),0)):0;
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool property_buildings(Runtime& r,LocalView& view,std::string& e) {
    uintptr_t manager=0,ap=0,owner=0,shard=0,array_ap=0,slots=0;
    int32_t count=-1,capacity=-1,native_count=-1;
    if(!at(r,view.region,0x950,manager,e)||!at(r,manager,0,ap,e)||!at(r,manager,0x458,owner,e)||
       !at(r,manager,0x450,shard,e)||!at(r,manager+0x408,0,array_ap,e)||
       !at(r,manager,0x410,slots,e)||!at(r,manager,0x418,count,e)||!at(r,manager,0x41c,capacity,e))return false;
    if(ap!=r.image+0x17183c0||array_ap!=r.image+0x172d7c0||owner!=view.region||shard!=view.world.root||
       count<0||count>1000000||capacity<count||(count&&!slots))return fail(e,"region BuildingManager owner, class or array differs");
    const auto seh=property_native_building_count(r.image,manager,native_count);
    if(seh||native_count!=count)return fail(e,"region native building count differs SEH="+std::to_string(seh)+
        " native="+std::to_string(native_count)+" array="+std::to_string(count));
    std::set<uint64_t> unique;
    for(int32_t i=0;i<count;++i){uintptr_t object=0,actual=0,region=0,actual_shard=0;uint64_t uid=0;
        if(!at(r,slots,size_t(i)*8,object,e)||!at(r,object,0x18,uid,e)||!at(r,object,0x178,region,e)||
           !at(r,object,0x170,actual_shard,e))return false;
        if(!uid||region!=view.region||actual_shard!=view.world.root||!unique.insert(uid).second)
            return fail(e,"region building UID or membership differs");
        const auto lookup=property_registered_building(r.image,uid,actual);
        if(lookup||actual!=object)return fail(e,"region building typed registry differs UID="+std::to_string(uid)+
            " SEH="+std::to_string(lookup));
        view.building_uids.push_back(uid);
    }
    std::sort(view.building_uids.begin(),view.building_uids.end());
    uintptr_t final_manager=0,final_slots=0;int32_t final_count=-1;
    if(!at(r,view.region,0x950,final_manager,e)||!at(r,manager,0x410,final_slots,e)||
       !at(r,manager,0x418,final_count,e))return false;
    return (final_manager==manager&&final_slots==slots&&final_count==count)||
        fail(e,"region BuildingManager changed during census");
}
bool property_clear_members(Runtime& r,LocalView& view,action::RegionPropertyState& state,std::string& e) {
    if(!property_buildings(r,view,e))return false;
    state.building_count=view.building_uids.size();
    if(!ui_path_runtime::region_members(view.state.id,view.path_uids,view.path_node_uids,e))return false;
    state.path_count=view.path_uids.size();state.path_node_count=view.path_node_uids.size();
    connected_network_native::Graph network;
    if(!connected_network_native::graph(r.memory,r.image,view.world.root,network,e))return false;
    for(const auto& node:network.nodes)if(node.region_id==view.state.id)view.network_uids.push_back(node.uid);
    for(const auto& pipe:network.pipe_values)if(pipe.region_id==view.state.id)view.network_uids.push_back(pipe.uid);
    state.network_node_count=std::count_if(network.nodes.begin(),network.nodes.end(),[&](const auto& node){return node.region_id==view.state.id;});
    state.network_pipe_count=std::count_if(network.pipe_values.begin(),network.pipe_values.end(),[&](const auto& pipe){return pipe.region_id==view.state.id;});
    std::sort(view.network_uids.begin(),view.network_uids.end());
    std::set<uint64_t> all;
    for(const auto* group:{&view.building_uids,&view.scenery_uids,&view.path_uids,&view.path_node_uids,&view.network_uids})
        for(const auto uid:*group)if(!uid||!all.insert(uid).second)return fail(e,"region clear member UID zero or shared by two families");
    return true;
}
bool property_read(Runtime& r,int32_t id,uintptr_t expected,action::RegionPropertyState& out,LocalView& view,std::string& e,bool clear_members=false) {
    if(!read(r,id,expected,view,e))return false;
    action::RegionPropertyState state;state.content=view.state;
    uint8_t owned=0;
    if(!at(r,view.region,0x268,state.type_random,e)||!text(r,view.region+0x810,state.name,e)||!at(r,view.region,0x830,state.type,e)||
       !at(r,view.region,0x838,state.level,e)||!at(r,view.region,0x83c,state.max_level,e)||
       !at(r,view.region,0x850,owned,e)||!at(r,view.region,0x854,state.status,e))return false;
    if(owned>1)return fail(e,"region ownership outside native Boolean domain");
    state.owned=owned!=0;
    if(clear_members&&!property_clear_members(r,view,state,e))return false;
    out=std::move(state);return true;
}
void property_rejected(const std::string& e) {
    ++properties.rejected;
    if(properties.cause!=e){properties.cause=e;LOG_ERROR("[UIRegionProperty] Rejected cause={}",e);}
}
bool property_admit(Runtime& r,std::string& e) {
    const auto controller=typed_action_runtime::status();
    if(GetCurrentThreadId()!=r.thread||!controller.value("started",false)||controller.value("locked",true)||
       controller.value("pending",true)||r.pending||properties.pending){e="region property UI requires idle bound Controller";return false;}
    return true;
}
bool property_capture(Runtime& r,const action::RegionPropertyIntent& intent,uintptr_t expected,std::string& e) {
    if(!property_admit(r,e))return false;
    LocalView view;if(!identity(r,intent.id,expected,view,e))return false;
    properties.pending=intent;properties.pending_binding={view.world.origin.world_epoch,view.world.origin.generation};
    properties.pending_snapshot=net::current_snapshot_id();++properties.captured;return true;
}
void property_rename_entry(void* widget,const NativeString* value) {
    auto* r=published.load(std::memory_order_acquire);
    auto original=properties.rename.load(std::memory_order_acquire);
    if(!r||!properties.ready||properties.replaying||!net::session_wire::connected()){
        original(widget,value);return;
    }
    std::string e;uintptr_t region=0,owner=0,ap=0;action::RegionPropertyIntent intent;
    intent.kind=action::RegionPropertyKind::Name;
    if(!at(*r,uintptr_t(widget),0,ap,e)||ap!=r->image+0x17024b0||
       !at(*r,uintptr_t(widget),0x178,region,e)||!at(*r,region,0x370,owner,e)||owner!=uintptr_t(widget)||
       !at(*r,region,0x834,intent.id,e)||!text(*r,uintptr_t(value),intent.name,e)||
       !property_capture(*r,intent,region,e)){property_rejected(e);return;}
}
void property_cursor_entry(action::RegionPropertyKind kind,void* behavior,void* cursor,const float* hit) {
    auto* r=published.load(std::memory_order_acquire);
    auto original=(kind==action::RegionPropertyKind::Purchase?properties.buy:properties.lift).load(std::memory_order_acquire);
    if(!r||!properties.ready||properties.replaying||!net::session_wire::connected()){
        original(behavior,cursor,hit);return;
    }
    std::string e;uintptr_t region=0,ap=0;uint8_t valid=0,mode=0;action::RegionPropertyIntent intent;
    intent.kind=kind;
    if(!at(*r,uintptr_t(behavior),0,ap,e)||
       ap!=r->image+(kind==action::RegionPropertyKind::Purchase?0x173c170:0x173d840)||
       !at(*r,uintptr_t(behavior),0x158,valid,e)||!at(*r,uintptr_t(behavior),0x150,region,e)||
       !at(*r,uintptr_t(behavior),0x148,mode,e)||!hit||!r->memory.copy(uintptr_t(hit),intent.hit.data(),12,e)){
        property_rejected(e.empty()?"region property cursor receiver or hit invalid":e);return;
    }
    if(!valid||!region){original(behavior,cursor,hit);return;}
    if(kind==action::RegionPropertyKind::Purchase){if(mode>1){property_rejected("region buy/sell cursor mode invalid");return;}if(mode)intent.kind=action::RegionPropertyKind::Sale;}
    else {if(mode>1){property_rejected("region type cursor mode invalid");return;}intent.type=mode?0:1;}
    if(!at(*r,region,0x834,intent.id,e)||!property_capture(*r,intent,region,e)){property_rejected(e);return;}
}
void property_buy_entry(void* b,void* c,const float* h){property_cursor_entry(action::RegionPropertyKind::Purchase,b,c,h);}
void property_lift_entry(void* b,void* c,const float* h){property_cursor_entry(action::RegionPropertyKind::Type,b,c,h);}
bool install_properties(Runtime& r,std::string& e) {
    constexpr unsigned char rename_bytes[]{0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57};
    constexpr unsigned char buy_bytes[]{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54};
    constexpr unsigned char lift_bytes[]{0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53};
    const std::array<std::pair<uintptr_t,std::pair<const unsigned char*,size_t>>,3> checks{{
        {0x36bad0,{rename_bytes,sizeof rename_bytes}},{0x5956f0,{buy_bytes,sizeof buy_bytes}},
        {0x5b9610,{lift_bytes,sizeof lift_bytes}}}};
    for(const auto& [offset,bytes]:checks){std::array<unsigned char,8> actual{};
        if(!r.memory.copy(r.image+offset,actual.data(),actual.size(),e))return false;
        if(std::memcmp(actual.data(),bytes.first,bytes.second))return fail(e,"region property native handler bytes differ RVA="+std::to_string(offset));
    }
    RenameProperty rename=nullptr;CursorProperty buy=nullptr,lift=nullptr;
    const auto a=MH_CreateHook(reinterpret_cast<void*>(r.image+0x36bad0),reinterpret_cast<void*>(&property_rename_entry),reinterpret_cast<void**>(&rename));
    if(a!=MH_OK||!rename)return fail(e,"region Rename hook create status="+std::to_string(a));
    properties.rename.store(rename,std::memory_order_release);
    const auto b=MH_CreateHook(reinterpret_cast<void*>(r.image+0x5956f0),reinterpret_cast<void*>(&property_buy_entry),reinterpret_cast<void**>(&buy));
    if(b!=MH_OK||!buy)return fail(e,"region Buy hook create status="+std::to_string(b));
    properties.buy.store(buy,std::memory_order_release);
    const auto c=MH_CreateHook(reinterpret_cast<void*>(r.image+0x5b9610),reinterpret_cast<void*>(&property_lift_entry),reinterpret_cast<void**>(&lift));
    if(c!=MH_OK||!lift)return fail(e,"region Lift hook create status="+std::to_string(c));
    properties.lift.store(lift,std::memory_order_release);
    for(const auto offset:{0x36bad0,0x5956f0,0x5b9610}){
        const auto status=MH_EnableHook(reinterpret_cast<void*>(r.image+offset));
        if(status!=MH_OK)return fail(e,"region property hook enable status="+std::to_string(status)+" RVA="+std::to_string(offset));
    }
    if(!install_levels(r,e))return false;
    properties.ready=true;return true;
}
void property_tick() {
    auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready||!properties.pending)return;
    std::string e;const auto intent=*properties.pending;properties.pending.reset();LocalView view;
    if(!identity(*r,intent.id,0,view,e)||!bound(view,properties.pending_binding,properties.pending_snapshot,e)){
        property_rejected(e);return;
    }
    const auto id=uuid(e);if(id.empty()){property_rejected(e);return;}
    properties.last_action=id;
    if(!typed_action_runtime::submit_region_property(id,intent,e)){property_rejected(e);return;}
    ++properties.submitted;
}
uint32_t property_hit_region(uintptr_t image,uintptr_t map,const float* hit,uintptr_t& found) {
    __try{found=uintptr_t(reinterpret_cast<void*(__fastcall*)(void*,const float*,void*)>(image+0x32a990)(
        reinterpret_cast<void*>(map),hit,nullptr));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t property_construct(uintptr_t image,uintptr_t object,uintptr_t offset,bool& returned) {
    __try{reinterpret_cast<void(__fastcall*)(void*)>(image+offset)(reinterpret_cast<void*>(object));returned=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t property_allocate(uintptr_t image,uintptr_t& object) {
    __try{object=uintptr_t(reinterpret_cast<void*(__fastcall*)(size_t)>(image+0xa02670)(0x160));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t property_delete(uintptr_t image,uintptr_t object) {
    __try{reinterpret_cast<void(__fastcall*)(void*,size_t)>(image+0x1264b60)(reinterpret_cast<void*>(object),0x160);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t property_cursor_call(CursorProperty fn,uintptr_t object,const float* hit,bool& returned) {
    __try{fn(reinterpret_cast<void*>(object),nullptr,hit);returned=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t property_rename_call(RenameProperty fn,uintptr_t widget,const NativeString* value,bool& returned) {
    __try{fn(reinterpret_cast<void*>(widget),value);returned=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool property_same_before(action::RegionPropertyState left,action::RegionPropertyState right) {
    left.content.random={};right.content.random={};
    left.type_random={};right.type_random={};
    if(left.actors&&right.actors){left.actors->global_random={};right.actors->global_random={};}return left==right;
}
#include "region_sale_runtime.inc"
#include "region_ownership_native.inc"
action::Outcome property_perform(const action::Binding& binding,const action::Request& request,
                                 const action::Evidence* authority,std::string& e) {
    auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||!properties.ready){e="region property runtime unavailable";return action::Outcome::RejectedNoEffect;}
    if(properties.operations.contains(request.action_uuid)){e="region property native action already attempted; no replay";return action::Outcome::Unknown;}
    const auto controller=typed_action_runtime::status();
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){
        e="region property operation lacks exact Controller request";return action::Outcome::RejectedNoEffect;
    }
    if(request.region_property.kind==action::RegionPropertyKind::LevelRange)return level_perform(*r,binding,request,authority,e);
    action::RegionPropertyState before;LocalView view;
    const bool sale=request.region_property.kind==action::RegionPropertyKind::Sale;
    const bool ownership=sale||request.region_property.kind==action::RegionPropertyKind::Purchase;
    const bool clear_members=request.region_property.kind==action::RegionPropertyKind::Type;
    const bool member_census=clear_members||sale;
    action::RegionLevelState sale_actors;uintptr_t sale_global=0;sale_native::UndoWitness sale_undo;int32_t refund=0;
    if(!property_read(*r,request.region_property.id,0,before,view,e,member_census)||!bound(view,binding,request.snapshot_uuid,e))return action::Outcome::RejectedNoEffect;
    if(clear_members){json shared;if(!remove_diagnostics::bindings(shared,e))return action::Outcome::RejectedNoEffect;}
    action::Ledger money;if(!ledger(money,e))return action::Outcome::RejectedNoEffect;
    if(sale){
        if(!level_state(*r,view,{0,0},sale_actors,sale_global,e)||!sale_native::read_undo(r->memory,r->image,sale_undo,e))return action::Outcome::RejectedNoEffect;
        const auto price_error=sale_price(r->image,refund);
        if(price_error||refund<0||sale_undo.active_graph){e="Region sale price/idle Undo invalid SEH="+std::to_string(price_error)+" refund="+std::to_string(refund);return action::Outcome::RejectedNoEffect;}
        before.actors=sale_actors;
    }
    if(sale||action::appearance_kind(request.region_property.kind)){action::RegionAppearance value;
        if(!appearance_read_native(*r,view.region,value,e))return action::Outcome::RejectedNoEffect;before.appearance=value;}
    if(authority&&(authority->before!=money||!property_same_before(before,authority->property_before))){
        e="region property client content/ledger differs from Host before state";return action::Outcome::RejectedNoEffect;
    }
    const auto& intent=request.region_property;
    if(action::appearance_kind(intent.kind)&&action::appearance_matches(intent,*before.appearance)){
        e="Region appearance already equals requested value";return action::Outcome::RejectedNoEffect;}
    if(intent.kind==action::RegionPropertyKind::Name){
        if(before.name==intent.name){e="region name already equals requested value";return action::Outcome::RejectedNoEffect;}
    }else if(!action::appearance_kind(intent.kind)) {
        if(sale&&(!before.owned||before.type<=0)){e="Region sale requires owned land";return action::Outcome::RejectedNoEffect;}
        if(intent.kind==action::RegionPropertyKind::Purchase&&before.owned){e="region already owned";return action::Outcome::RejectedNoEffect;}
        if(intent.kind==action::RegionPropertyKind::Type&&(before.owned||before.type==intent.type)){
            e="region type unavailable or already selected";return action::Outcome::RejectedNoEffect;
        }
        uintptr_t map=0,found=0;if(!at(*r,r->image,0x13df8c0,map,e))return action::Outcome::RejectedNoEffect;
        const auto seh=property_hit_region(r->image,map,intent.hit.data(),found);
        if(seh||found!=view.region){e="region hit position resolves to another native region SEH="+std::to_string(seh);return action::Outcome::RejectedNoEffect;}
    }
    PropertyOperation operation;operation.binding=binding;operation.request=request;operation.evidence.before=money;
    operation.evidence.property_before=authority?authority->property_before:before;
    sale_native::UndoWitness ownership_undo;action::RegionPropertyState ownership_before;LocalView ownership_view;uintptr_t ownership_global=0;
    const std::array<int32_t,2> ownership_range=sale?std::array<int32_t,2>{0,0}:std::array<int32_t,2>{1,1};
    if(ownership){
        if(!ownership_read(*r,intent.id,view.region,ownership_range,ownership_before,ownership_view,ownership_global,e)||
           !sale_native::read_undo(r->memory,r->image,ownership_undo,e))return action::Outcome::RejectedNoEffect;
        if(ownership_undo.active_graph){e="Region ownership requires idle original Undo";return action::Outcome::RejectedNoEffect;}
        operation.ownership_before=ownership_before;
    }
    auto& saved=properties.operations.emplace(request.action_uuid,std::move(operation)).first->second;
    const auto unknown=[&](const std::string& cause){e=saved.cause=cause;return action::Outcome::Unknown;};
    if(clear_members&&!sha256(json(view.building_uids).dump(),saved.building_uids_sha256_before,e))
        return unknown("region native Building UID diagnostic failed: "+e);
    if(action::appearance_kind(intent.kind)){
        properties.replaying=true;saved.attempted=true;
        const auto seh=appearance_original(r->image,view.region,intent,saved.returned);
        properties.replaying=false;
        if(seh||!saved.returned)return unknown("region original appearance failed SEH="+std::to_string(seh));
    }else if(intent.kind==action::RegionPropertyKind::Name){
        uintptr_t widget=0,owner=0,ap=0;std::string widget_name;
        if(!at(*r,view.region,0x370,widget,e)||!at(*r,widget,0,ap,e)||ap!=r->image+0x17024b0||
           !at(*r,widget,0x178,owner,e)||owner!=view.region||!text(*r,widget+0x38,widget_name,e)||widget_name!=before.name){
            saved.cause="region name widget owner or old label differs: "+e;return action::Outcome::RejectedNoEffect;
        }
        NativeString value{intent.name.data(),intent.name.size(),{}};
        if(authority){
            std::memcpy(reinterpret_cast<void*>(view.random),authority->property_before.content.random.data(),32);
            std::memcpy(reinterpret_cast<void*>(view.region+0x268),authority->property_before.type_random.data(),32);
            if(sale)std::memcpy(reinterpret_cast<void*>(sale_global),authority->property_before.actors->global_random.data(),32);
        }
        properties.replaying=true;saved.attempted=true;
        const auto seh=property_rename_call(properties.rename.load(std::memory_order_acquire),widget,&value,saved.returned);
        properties.replaying=false;
        if(seh||!saved.returned)return unknown("region original Rename failed SEH="+std::to_string(seh));
    }else {
        const bool buy=intent.kind==action::RegionPropertyKind::Purchase||sale;
        const auto ctor=buy?0x595490:0x5b9340,dtor=buy?0xc1e510:0xc1eb80,press=buy?0x595630:0x5b95a0;
        const auto release=buy?properties.buy.load(std::memory_order_acquire):properties.lift.load(std::memory_order_acquire);
        uintptr_t object=0;const auto allocation=property_allocate(r->image,object);
        if(allocation||!object){saved.cause="region temporary native behavior allocation failed SEH="+std::to_string(allocation);return action::Outcome::RejectedNoEffect;}
        bool constructed=false,pressed=false,destroyed=false;
        auto seh=property_construct(r->image,object,ctor,constructed);
        if(seh||!constructed)return unknown("region native behavior constructor failed SEH="+std::to_string(seh)+" retained allocation");
        *reinterpret_cast<uint8_t*>(object+0x148)=uint8_t(sale||(!buy&&intent.type==0));
        seh=property_cursor_call(reinterpret_cast<CursorProperty>(r->image+press),object,intent.hit.data(),pressed);
        uint8_t valid=0;uintptr_t selected=0;
        if(!seh&&pressed&&(!at(*r,object,0x158,valid,e)||!at(*r,object,0x150,selected,e)))seh=0xE0010003;
        if(seh||!pressed||!valid||selected!=view.region){
            bool ignored=false;const auto cleanup=property_construct(r->image,object,dtor,ignored);destroyed=ignored;
            const auto deletion=destroyed?property_delete(r->image,object):0;
            if(deletion)seh=deletion;
            if(seh||cleanup||!destroyed)return unknown("region native Press/readback or behavior cleanup failed SEH="+std::to_string(seh)+" cleanup="+std::to_string(cleanup)+" cause="+e);
            e=saved.cause="region native Press rejected hit/mode, valid="+std::to_string(valid)+" selected_match="+std::to_string(selected==view.region);
            return action::Outcome::RejectedNoEffect;
        }
        if(authority){
            std::memcpy(reinterpret_cast<void*>(view.random),authority->property_before.content.random.data(),32);
            std::memcpy(reinterpret_cast<void*>(view.region+0x268),authority->property_before.type_random.data(),32);
            if(sale)std::memcpy(reinterpret_cast<void*>(sale_global),authority->property_before.actors->global_random.data(),32);
        }
        properties.replaying=true;saved.attempted=true;
        seh=property_cursor_call(release,object,intent.hit.data(),saved.returned);
        properties.replaying=false;
        bool cleanup_returned=false;const auto cleanup=property_construct(r->image,object,dtor,cleanup_returned);
        if(cleanup_returned){const auto deletion=property_delete(r->image,object);
            if(deletion)return unknown("region native behavior delete failed SEH="+std::to_string(deletion));}
        if(seh||cleanup||!saved.returned||!cleanup_returned)return unknown("region original cursor Release/cleanup failed SEH="+std::to_string(seh)+" cleanup="+std::to_string(cleanup));
        saved.evidence.undo_confirmed=true;
    }
    action::RegionPropertyState after;LocalView after_view;
    if(!property_read(*r,intent.id,view.region,after,after_view,e,member_census)||!bound(after_view,binding,request.snapshot_uuid,e)||
       !ledger(saved.evidence.after,e))return unknown("region property native readback failed: "+e);
    if(sale||action::appearance_kind(intent.kind)){action::RegionAppearance value;
        if(!appearance_read_native(*r,view.region,value,e))return unknown("region appearance readback failed: "+e);after.appearance=value;}
    if(sale){action::RegionLevelState current;uintptr_t global=0;sale_native::UndoWitness undo_after;
        if(!level_state(*r,after_view,{0,0},current,global,e)||global!=sale_global||
           !sale_native::read_undo(r->memory,r->image,undo_after,e)||!sale_inverse(*r,view,before,sale_undo,undo_after,refund,intent.hit,e)||
           !sale_members(view,after_view,e))return unknown(e.empty()?"Region sale global RNG owner changed":e);
        after.actors=current;
        auto expected=before;expected.owned=false;expected.status=0;expected.level=expected.max_level=0;
        expected.content.random=after.content.random;expected.type_random=after.type_random;expected.actors=current;
        if(after!=expected||current.counts!=sale_actors.counts||current.roster_sha256!=sale_actors.roster_sha256||
           current.levels_sha256!=sale_actors.expected_sha256||current.expected_sha256!=current.levels_sha256||
           int64_t(saved.evidence.after.cash)-money.cash!=refund)return unknown("Region sale ownership/actor clamp/roster/native refund differs");
    }
    saved.evidence.property_after=after;
    if(ownership){
        action::RegionPropertyState full;LocalView full_view;uintptr_t global=0;sale_native::UndoWitness undo_after;
        const int64_t amount=int64_t(money.cash)-saved.evidence.after.cash;
        if(amount<INT32_MIN||amount>INT32_MAX||amount==INT32_MIN)return unknown("Region ownership inverse cash outside signed native domain");
        if(!ownership_read(*r,intent.id,view.region,ownership_range,full,full_view,global,e)||global!=ownership_global||
           !sale_members(ownership_view,full_view,e)||!sale_native::read_undo(r->memory,r->image,undo_after,e)||undo_after.undo.empty())
            return unknown(e.empty()?"Region ownership complete readback or original Undo missing":e);
        sale_native::GraphWitness graph;saved.ownership_graph=undo_after.undo.back();
        if(!sale_native::read_graph(r->memory,r->image,saved.ownership_graph,graph,e)||
           !sale_native::validate_undo_commit(ownership_undo,undo_after,graph,e)||
           !ownership_inverse(*r,saved.ownership_graph,view.region,ownership_before,int32_t(amount),intent.hit,e))return unknown(e);
        auto expected=ownership_before;expected.owned=!sale;expected.status=sale?0:1;
        expected.level=expected.max_level=sale?0:1;expected.content.random=full.content.random;
        expected.type_random=full.type_random;expected.actors=full.actors;
        if(full!=expected||ownership_before.actors->counts!=full.actors->counts||
           ownership_before.actors->roster_sha256!=full.actors->roster_sha256||
           ownership_before.actors->expected_sha256!=full.actors->levels_sha256||
           full.actors->expected_sha256!=full.actors->levels_sha256)
            return unknown("Region ownership original full state or actor clamp differs");
        saved.ownership_after=std::move(full);
    }
    if(clear_members&&!sha256(json(after_view.building_uids).dump(),saved.building_uids_sha256_after,e))
        return unknown("region native Building UID readback diagnostic failed: "+e);
    std::vector<uint64_t> removed;
    if(clear_members){
        if(after.building_count||after.content.scenery_count||after.path_count||after.path_node_count||
           after.network_node_count||after.network_pipe_count)
            return unknown("region native ClearContents retained supported manager members");
        for(const auto* group:{&view.building_uids,&view.scenery_uids,&view.path_uids,&view.path_node_uids,&view.network_uids})
            removed.insert(removed.end(),group->begin(),group->end());
        std::sort(removed.begin(),removed.end());
    }else {
        std::set_difference(view.scenery_uids.begin(),view.scenery_uids.end(),after_view.scenery_uids.begin(),after_view.scenery_uids.end(),std::back_inserter(removed));
    }
    if(!remove_diagnostics::retire_region_scenery(request.snapshot_uuid,request.action_uuid,removed,saved.retired_keys,e)||
       !sha256(json(saved.retired_keys).dump(),saved.evidence.region_retired_sha256,e))return unknown("region property member retirement failed: "+e);
    saved.removed_lookup_absent=true;
    saved.removed_uids=removed;
    if(authority&&(saved.evidence.property_after!=authority->property_after||saved.evidence.after!=authority->after||
                   saved.evidence.region_retired_sha256!=authority->region_retired_sha256))
        return unknown("region property result differs from Host authority");
    saved.confirmed=true;return action::Outcome::Confirmed;
}
bool property_observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e) {
    auto* r=published.load(std::memory_order_acquire);if(!r)return fail(e,"region property observation runtime absent");
    const auto found=properties.operations.find(request.action_uuid);
    if(found==properties.operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)
        return fail(e,"region property observation lacks exact confirmed operation");
    const auto& op=found->second;action::RegionPropertyState actual;LocalView view;action::Ledger money;
    if(!property_read(*r,request.region_property.id,0,actual,view,e,
        (request.region_property.kind==action::RegionPropertyKind::Type||request.region_property.kind==action::RegionPropertyKind::Sale))||!bound(view,binding,request.snapshot_uuid,e)||!ledger(money,e))return false;
    if(request.region_property.kind==action::RegionPropertyKind::LevelRange||request.region_property.kind==action::RegionPropertyKind::Sale){action::RegionLevelState state;uintptr_t global=0;
        const auto range=request.region_property.kind==action::RegionPropertyKind::Sale?std::array<int32_t,2>{0,0}:request.region_property.levels;
        if(!level_state(*r,view,range,state,global,e))return false;
        actual.actors=state;
        // Receipt RNG is synchronous. Later global draws are observational.
        actual.actors->global_random=op.evidence.property_after.actors->global_random;
    }
    if(request.region_property.kind==action::RegionPropertyKind::Sale||action::appearance_kind(request.region_property.kind)){action::RegionAppearance value;
        if(!appearance_read_native(*r,view.region,value,e))return false;actual.appearance=value;}
    if(actual!=op.evidence.property_after||money!=op.evidence.after)return fail(e,"region property observation changed after native result");
    if(op.ownership_after){
        action::RegionPropertyState full;LocalView full_view;uintptr_t global=0;
        const std::array<int32_t,2> range{op.ownership_after->level,op.ownership_after->max_level};
        if(!ownership_read(*r,request.region_property.id,view.region,range,full,full_view,global,e))return false;
        full.actors->global_random=op.ownership_after->actors->global_random;
        if(full!=*op.ownership_after)return fail(e,"Region ownership complete result changed before ACK");
    }
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool property_retire(const action::Binding& binding,const action::Request& request,std::string& e) {
    action::Evidence actual;if(!property_observe(binding,request,actual,e))return false;
    auto& op=properties.operations.at(request.action_uuid);if(op.retired)return fail(e,"region property receipt already retired");
    if(request.region_property.kind==action::RegionPropertyKind::LevelRange&&
       !building_history_slice::remember_region(request.action_uuid,request.region_property.id,op.level_graph,e))return false;
    if((request.region_property.kind==action::RegionPropertyKind::Purchase||request.region_property.kind==action::RegionPropertyKind::Sale)&&
       !building_history_slice::remember_region(request.action_uuid,request.region_property.id,op.ownership_graph,e))return false;
    op.retired=true;return true;
}
bool property_retired_keys(const action::Binding& binding,const action::Request& request,std::vector<std::string>& keys,std::string& e) {
    action::Evidence actual;if(!property_observe(binding,request,actual,e))return false;
    keys=properties.operations.at(request.action_uuid).retired_keys;return true;
}
bool property_ui_impl(const std::string& token,const action::RegionPropertyIntent& intent,json& out,std::string& e) {
    out=json::object();auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||!properties.ready||!net::session_wire::connected()||GetCurrentThreadId()!=r->thread)
        return fail(e,"region property UI diagnostic requires connected Mainthread");
    if(token.empty()||token.size()>64||intent.id<0||!property_admit(*r,e))return fail(e,e.empty()?"region property UI diagnostic token or region ID invalid":e);
    if(intent.kind==action::RegionPropertyKind::LevelRange)return level_ui(*r,token,intent,out,e);
    if(properties.ui_attempts.contains(token))return fail(e,"region property UI diagnostic token already attempted; inspect receipt before retry");
    action::RegionPropertyState before;LocalView view;
    if(!property_read(*r,intent.id,0,before,view,e,intent.kind==action::RegionPropertyKind::Type))return false;
    if(intent.kind==action::RegionPropertyKind::Name){
        if(intent.name.empty()||intent.name.size()>256||intent.name.find('\0')!=std::string::npos||before.name==intent.name)
            return fail(e,"region diagnostic name is empty, too long or unchanged");
    }else {
        if(intent.kind!=action::RegionPropertyKind::Purchase&&intent.kind!=action::RegionPropertyKind::Type&&intent.kind!=action::RegionPropertyKind::Sale)
            return fail(e,"region property diagnostic kind invalid");
        if(intent.kind==action::RegionPropertyKind::Type&&(intent.type!=0&&intent.type!=1))
            return fail(e,"region type diagnostic requires 0 or 1");
        for(const float x:intent.hit)if(!std::isfinite(x)||std::abs(x)>1000000.0f)
            return fail(e,"region diagnostic hit coordinate is not finite or bounded");
        uintptr_t map=0,found=0;
        if(!at(*r,r->image,0x13df8c0,map,e))return false;
        const auto seh=property_hit_region(r->image,map,intent.hit.data(),found);
        if(seh||found!=view.region)return fail(e,"region diagnostic hit does not resolve requested region SEH="+std::to_string(seh));
    }
    properties.ui_attempts.insert(token);out["attempted"]=true;
    const auto captured=properties.captured;
    if(intent.kind==action::RegionPropertyKind::Name){
        uintptr_t widget=0,ap=0,owner=0;std::string label;
        if(!at(*r,view.region,0x370,widget,e)||!at(*r,widget,0,ap,e)||ap!=r->image+0x17024b0||
           !at(*r,widget,0x178,owner,e)||owner!=view.region||!text(*r,widget+0x38,label,e)||label!=before.name)
            return fail(e,"region diagnostic widget label/owner differs: "+e);
        NativeString name{intent.name.data(),intent.name.size(),{}};bool returned=false;
        const auto seh=property_rename_call(reinterpret_cast<RenameProperty>(r->image+0x36bad0),widget,&name,returned);
        out["handler_returned"]=returned;if(seh||!returned)return fail(e,"region diagnostic Rename UI handler SEH="+std::to_string(seh));
    }else {
        const bool sale=intent.kind==action::RegionPropertyKind::Sale;
        const bool buy=intent.kind==action::RegionPropertyKind::Purchase||sale;
        const auto ctor=buy?0x595490:0x5b9340,dtor=buy?0xc1e510:0xc1eb80,press=buy?0x595630:0x5b95a0;
        const auto release=buy?0x5956f0:0x5b9610;
        uintptr_t object=0;const auto allocation=property_allocate(r->image,object);
        if(allocation||!object)return fail(e,"region diagnostic behavior allocation failed SEH="+std::to_string(allocation));
        bool constructed=false,pressed=false,returned=false,destroyed=false;
        auto seh=property_construct(r->image,object,ctor,constructed);
        if(seh||!constructed)return fail(e,"region diagnostic behavior ctor failed SEH="+std::to_string(seh)+" retained allocation");
        *reinterpret_cast<uint8_t*>(object+0x148)=uint8_t(sale||(!buy&&intent.type==0));
        seh=property_cursor_call(reinterpret_cast<CursorProperty>(r->image+press),object,intent.hit.data(),pressed);
        uint8_t valid=0;uintptr_t selected=0;
        if(!seh&&pressed&&(!at(*r,object,0x158,valid,e)||!at(*r,object,0x150,selected,e)))seh=0xE0010003;
        if(!seh&&pressed&&valid&&selected==view.region)
            seh=property_cursor_call(reinterpret_cast<CursorProperty>(r->image+release),object,intent.hit.data(),returned);
        const auto cleanup=property_construct(r->image,object,dtor,destroyed);
        const auto deletion=destroyed?property_delete(r->image,object):0;
        out["press_admitted"]=bool(valid&&selected==view.region);out["handler_returned"]=returned;out["cleanup_returned"]=destroyed;
        if(seh||cleanup||deletion||!pressed||!valid||selected!=view.region||!returned||!destroyed)
            return fail(e,"region diagnostic cursor UI handler/cleanup failed SEH="+std::to_string(seh)+
                " cleanup="+std::to_string(cleanup)+" delete="+std::to_string(deletion));
    }
    const bool once=properties.captured==captured+1&&properties.pending&&*properties.pending==intent;
    out["captured_once"]=once;
    if(!once)return fail(e,"region diagnostic UI did not capture exactly one intent: "+properties.cause);
    action::RegionPropertyState after;LocalView after_view;
    if(!property_read(*r,intent.id,view.region,after,after_view,e,intent.kind==action::RegionPropertyKind::Type))return false;
    out["local_state_unchanged"]=after==before;
    if(after!=before)return fail(e,"region diagnostic UI changed native state before Host authority");
    out["pending_intent"]=true;return true;
}
#include "region_appearance_runtime.inc"
#include "region_level_runtime.inc"
#include "region_history_runtime.inc"
}
action::Outcome perform_history(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){return region_history_perform(binding,request,authority,e);}
bool observe_history(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){return region_history_observe(binding,request,out,e);}
bool retire_history(const action::Binding& binding,const action::Request& request,std::string& e){return region_history_retire(binding,request,e);}
bool property_ui(const std::string& token,const action::RegionPropertyIntent& intent,json& out,std::string& e){
    return property_ui_impl(token,intent,out,e);
}
