namespace zone_action_runtime {
namespace {
namespace a=mt2mp::action_session;
namespace d=zone_probe_detail;
namespace q=npc_quest_targets;
namespace poly=ui_terrain_runtime::poly;
namespace selection=monster_zone_selection;
using Unary=void(*)(void*);using Binary=void(*)(void*,const void*);
using Press=void(*)(void*,void*,const float*);using GetSelection=void*(*)(void*);
struct Pin {host::WorldState world;q::World region;uintptr_t window=0;selection::Catalogue catalogue;int32_t min_level=0,max_level=0;};
struct Capture {
    uintptr_t brush=0,cursor=0,graph=0;Pin pin;a::Binding binding;std::string snapshot,cause;
    a::ZoneIntent intent;d::Sample before;bool cleanup_attempted=false;std::string ui_token;
};
struct alignas(16) Brush {std::array<unsigned char,0x750> bytes{};};
struct Operation {
    a::Binding binding;a::Request request;Pin pin;a::Evidence evidence;
    std::unique_ptr<Brush> brush=std::make_unique<Brush>();
    uintptr_t graph=0,created=0;uint64_t uid=0;
    bool constructed=false,attempted=false,returned=false,confirmed=false,retired=false;
    unsigned stamps=0;std::string cause;
};
struct Runtime {
    uintptr_t image=0;uint32_t thread=0;bool install_attempted=false,ready=false,capture_fault=false;
    Press press=nullptr;Binary paint=nullptr,finished=nullptr;GetSelection get=nullptr;
    const selection::Binding* selection=nullptr;uintptr_t private_brush=0;bool inside_paint=false;
    std::optional<Capture> capture,pending;std::map<std::string,Operation> operations;
    uint64_t captures=0,submits=0,rejections=0;std::string cause,last_action,install_cause;
    std::string ui_token;std::map<std::string,json> ui_jobs;
};
Runtime state;
void reject(const std::string& e){++state.rejections;if(state.cause!=e){state.cause=e;LOG_ERROR("[UIMonsterZone] {}",e);}}
bool same_origin(const Pin& x,const Pin& y){return action_runtime::same_world(x.world,y.world)&&
    x.region.region==y.region.region&&x.region.saved_region==y.region.saved_region&&
    x.window==y.window&&x.catalogue==y.catalogue&&x.min_level==y.min_level&&x.max_level==y.max_level;}
a::Ledger ledger(const native::SimulationState& m){return {m.cash,m.clock.flat_day,int32_t(m.record_present),
    m.record_day,m.record_cash,m.record_income,m.record_expense,m.line_counts,m.line_income};}
bool code(Memory&,size_t,bool,std::string&);
struct ThemeNative {
    uintptr_t image;alignas(8) unsigned char text[32]{};
    uint32_t construct()noexcept{
        __try{reinterpret_cast<void(*)(void*,const char*)>(image+0x49e090)(text,"character_theme");return 0;}
        __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    }
    uint32_t find(uintptr_t& window)noexcept{
        __try{window=uintptr_t(reinterpret_cast<void*(*)(const void*)>(image+0x81aa90)(text));return 0;}
        __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    }
    uint32_t cleanup()noexcept{
        __try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(text);return 0;}
        __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    }
};
bool theme_owner(Memory& m,uintptr_t& window,std::string& e){
    if(state.capture_fault)return fail(e,"Zone unresolved native lifetime blocks Window.Find; no replay");
    for(size_t i=12;i<16;++i)if(!code(m,i,false,e))return false;
    ThemeNative calls{state.image};monster_zone_theme_find::Receipt receipt;
    if(!monster_zone_theme_find::resolve(calls,window,receipt,e)){state.capture_fault=receipt.unknown;return false;}
    if(!window)return fail(e,"Zone original Window.Find character_theme absent");
    uintptr_t ap=0;std::string key;
    if(!field(m,window,0,ap,e)||!name(m,window+0x1b0,key,e))return false;
    if(ap!=state.image+0x17321a0)return fail(e,"Zone original Window.Find CharacterTheme type differs");
    if(key!="character_theme")return fail(e,"Zone original Window.Find CharacterTheme name differs");
    return true;
}

bool pin(Memory& m,const a::Binding& binding,const std::string& snapshot,int32_t id,int32_t type,Pin& out,std::string& e,uintptr_t expected_window=0){
    if(!state.ready||state.thread!=GetCurrentThreadId())return fail(e,"Zone action requires installed bound Mainthread");
    host::LookupApi api;if(!host::resolve_lookup(m.process,state.image,exe_sha,api,e))return false;
    host::NativeOrigin origin;host::Reader reader(m.process,origin,state.image,state.thread,api);
    if(!reader.current(out.world,true,e)||!action_runtime::bound(out.world,binding,snapshot,e))return false;
    const auto at=[&](uintptr_t p,size_t o,auto& v,std::string& cause){return field(m,p,o,v,cause);};
    out.region.image=state.image;out.region.root=out.world.root;out.region.map=uintptr_t(out.world.origin.map);out.region.saved_region=id;
    if(!region_saved_identity::resolve(state.image,out.region.map,out.region.root,id,0,out.region.region,at,e))return false;
    uint8_t owned=0;if(!field(m,out.region.region,0x850,owned,e)||owned!=1)return fail(e,e.empty()?"Zone action requires owned Region":e);
    if(!field(m,out.region.region,0x838,out.min_level,e)||!field(m,out.region.region,0x83c,out.max_level,e)||
       out.min_level<1||out.max_level<out.min_level)return fail(e,e.empty()?"Zone original Region level range differs":e);
    d::ReadMemory bounded{m};if(!selection::read(bounded,state.image,type,out.catalogue,e))return false;
    if(!theme_owner(m,out.window,e))return false;
    if(expected_window&&out.window!=expected_window)return fail(e,"Zone original Window.Find differs from actual brush-linked window");
    host::WorldState after;if(!reader.current(after,true,e)||!action_runtime::same_world(after,out.world))return fail(e,e.empty()?"Zone full Origin changed during pin":e);
    return true;
}
bool effect(Memory& m,const Pin& p,d::Sample& sample,a::ZoneState& out,std::string& e){
    if(!d::sample(m,p.region,sample,e))return false;
    npc_quest_target_runtime::CurrentTargets targets;
    if(!npc_quest_target_runtime::read_current(p.region.saved_region,targets,e)||targets.monster_zone_keys.size()!=sample.state.zones.size())return fail(e,e.empty()?"Zone canonical membership count differs":e);
    json content=d::state_json(sample.state);content.erase("local_identity_scope");
    content.erase("global_random");content.erase("region_random");content.erase("next_monster_tag");content.erase("created_monsters");
    std::map<uint64_t,std::string> zone_keys,monster_keys;
    for(size_t i=0;i<sample.state.zones.size();++i)zone_keys.emplace(sample.state.zones[i].value.uid,targets.monster_zone_keys[i]);
    for(const auto& row:sample.state.monsters){
        const auto z=std::find_if(sample.state.zones.begin(),sample.state.zones.end(),[&](const auto& v){return v.value.pointer==row.zone;});
        if(z==sample.state.zones.end())return fail(e,"Zone canonical Monster lacks reciprocal Zone");
        const auto member=std::find(z->monsters.begin(),z->monsters.end(),row.uid);
        if(member==z->monsters.end())return fail(e,"Zone canonical Monster missing member ordinal");
        monster_keys.emplace(row.uid,zone_keys.at(z->value.uid)+":monster:"+std::to_string(member-z->monsters.begin()));
    }
    for(auto& row:content["zones"]){const auto uid=row.at("local_uid").get<uint64_t>();row.erase("local_uid");row["key"]=zone_keys.at(uid);
        json members=json::array();for(const auto& v:row.at("monster_local_uids"))members.push_back(monster_keys.at(v.get<uint64_t>()));row.erase("monster_local_uids");row["monsters"]=std::move(members);}
    for(auto& row:content["monsters"]){const auto uid=row.at("local_uid").get<uint64_t>(),zone=row.at("zone_local_uid").get<uint64_t>();
        row.erase("local_uid");row.erase("zone_local_uid");row["key"]=monster_keys.at(uid);row["zone"]=zone_keys.at(zone);}
    out.zones=uint32_t(sample.state.zones.size());out.monsters=uint32_t(sample.state.monsters.size());out.undo_count=uint32_t(sample.undo.undo.size());
    out.next_tag=sample.state.next_monster_tag;out.created_monsters=sample.state.created_monsters;out.global_random=sample.state.global_random;
    out.region_random=sample.state.region_random;out.release_edits=sample.release_edits;
    return quest_bridge::sha256(content.dump(),out.content_sha256,e);
}
void press_entry(void*,void*,const float*);void paint_entry(void*,const void*);void finished_entry(void*,const void*);void* selection_entry(void*);
bool code(Memory& m,size_t index,bool hooked,std::string& e){
    const auto& c=monster_zone_action_codes::values[index];std::string bytes(c.size,'\0'),digest;
    if(!m.copy(state.image+c.rva,bytes.data(),bytes.size(),e))return false;
    if(hooked&&index<4){
        const uintptr_t originals[]{uintptr_t(state.press),uintptr_t(state.paint),uintptr_t(state.finished),uintptr_t(state.get)};
        const uintptr_t entries[]{uintptr_t(&press_entry),uintptr_t(&paint_entry),uintptr_t(&finished_entry),uintptr_t(&selection_entry)};
        std::array<unsigned char,16> prefix{};std::array<unsigned char,6> jump{};uintptr_t target=0;
        constexpr std::array<unsigned char,6> ff25{0xff,0x25,0,0,0,0};
        if(uint8_t(bytes[0])!=0xe9||!originals[index]||!m.copy(originals[index],prefix.data(),c.span,e)||
           std::memcmp(prefix.data(),c.prefix.data(),c.span)||!m.copy(originals[index]+c.span,jump.data(),jump.size(),e)||jump!=ff25||
           !m.copy(originals[index]+c.span+6,&target,8,e)||target!=state.image+c.rva+c.span)return fail(e,e.empty()?"Zone original trampoline ownership differs":e);
        int32_t relative=0;std::memcpy(&relative,bytes.data()+1,4);const auto relay=uintptr_t(int64_t(state.image+c.rva+5)+relative);
        if(!m.copy(relay,jump.data(),jump.size(),e)||jump!=ff25||!m.copy(relay+6,&target,8,e)||target!=entries[index])return fail(e,e.empty()?"Zone owned hook relay differs":e);
    }else if(std::memcmp(bytes.data(),c.prefix.data(),16))return fail(e,"Zone original entry differs RVA="+std::to_string(c.rva));
    if(!quest_bridge::sha256(c.span?bytes.substr(5):bytes,digest,e))return false;
    return digest==c.tail_sha||fail(e,"Zone immutable native body differs RVA="+std::to_string(c.rva));
}
bool all_code(Memory& m,bool hooked,std::string& e){for(size_t i=0;i<std::size(monster_zone_action_codes::values);++i)if(!code(m,i,hooked,e))return false;return true;}
uint32_t unary(Unary f,void* p)noexcept{__try{f(p);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t binary(Binary f,void* p,const void* v)noexcept{__try{f(p,v);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t press_call(void* brush,void* cursor,const float* p)noexcept{__try{state.press(brush,cursor,p);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool native_ok(uint32_t seh,const char* operation,std::string& e){return !seh||fail(e,std::string("Zone original ")+operation+" exception="+std::to_string(seh));}
uint32_t shape_make(poly::Storage& out,const poly::VectorView& paths,bool& made)noexcept{
    __try{reinterpret_cast<Unary>(state.image+0xf1810)(&out);made=true;auto* data=*reinterpret_cast<void**>(out.bytes.data()+0x18);
        reinterpret_cast<Binary>(state.image+0xf7ed0)(data,&paths);reinterpret_cast<Unary>(state.image+0xf3b60)(&out);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t intersects(const poly::Storage& shape,uintptr_t zone,bool& result)noexcept{
    __try{result=reinterpret_cast<bool(*)(const void*,const void*)>(state.image+0xf1750)(&shape,reinterpret_cast<void*>(zone+0x2f0));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool no_overlap(Memory& m,const a::ZoneIntent& intent,const d::Sample& before,bool& unknown,std::string& e){
    unknown=false;
    for(const auto& stamp:intent.stamps){poly::BorrowedPaths paths(stamp.shape);poly::Storage shape;bool made=false;
        const auto seh=shape_make(shape,paths.view,made);bool okay=native_ok(seh,"PolyShape construction",e);unknown=seh!=0;
        a::TerrainShape actual;if(okay)okay=poly::read(m,uintptr_t(&shape),actual,e)&&actual==stamp.shape;
        if(okay)for(const auto& zone:before.state.zones){bool hit=false;const auto code=intersects(shape,zone.value.pointer,hit);
            if(!native_ok(code,"PolyShape intersection",e)||hit){okay=false;unknown=code!=0;if(hit)e="Zone new stroke intersects an existing Zone";break;}}
        const auto cleanup=made?unary(reinterpret_cast<Unary>(state.image+0xf15a0),&shape):0;
        if(cleanup){unknown=true;e+="; Zone PolyShape destructor exception="+std::to_string(cleanup);return false;}
        if(!okay)return fail(e,e.empty()?"Zone native shape reconstruction differs":e);
    }return true;
}
void* selection_entry(void* receiver){
    if(!selection::matches(state.selection,state.private_brush,uintptr_t(receiver),uintptr_t(_ReturnAddress()),GetCurrentThreadId(),state.inside_paint))return state.get(receiver);
    return reinterpret_cast<void*>(state.selection->character);
}
uint32_t construct(Operation& op)noexcept{
    __try{auto* p=op.brush->bytes.data();reinterpret_cast<Unary>(state.image+0x5f1a30)(p);op.constructed=true;
        *reinterpret_cast<uintptr_t*>(p+0x170)=op.pin.region.region;*reinterpret_cast<int32_t*>(p+0x178)=1;
        *reinterpret_cast<int32_t*>(p+0x17c)=op.request.zone.level;*reinterpret_cast<uintptr_t*>(p+0x5d8)=op.pin.window;
        op.graph=uintptr_t(reinterpret_cast<void*(*)(size_t)>(state.image+0xa02670)(0x110));
        reinterpret_cast<Unary>(state.image+0x5400f0)(reinterpret_cast<void*>(op.graph));
        *reinterpret_cast<uintptr_t*>(p+0x740)=op.graph;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t stamp_call(Operation& op,const a::ZoneStamp& stamp,const poly::Storage& shape)noexcept{
    __try{std::memcpy(op.brush->bytes.data()+0x5cc,stamp.position.data(),12);state.paint(op.brush->bytes.data(),&shape);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool paint_private(Operation& op,const a::ZoneStamp& stamp,std::string& e){
    Memory m;Pin actual;if(!pin(m,op.binding,op.request.snapshot_uuid,op.request.zone.region,op.request.zone.character_type,actual,e)||!same_origin(actual,op.pin))return fail(e,e.empty()?"Zone private paint full owner changed":e);
    selection::Binding bound{uintptr_t(op.brush->bytes.data()),op.pin.window,op.pin.catalogue.character,state.image,state.thread};
    uintptr_t ap=0,region=0,window=0,graph=0;int32_t kind=-1,level=0;
    if(!op.constructed||!field(m,bound.brush,0,ap,e)||ap!=state.image+0x172fa80||
       !field(m,bound.brush,0x170,region,e)||region!=op.pin.region.region||
       !field(m,bound.brush,0x178,kind,e)||kind!=1||!field(m,bound.brush,0x17c,level,e)||level!=op.request.zone.level||
       !field(m,bound.brush,0x5d8,window,e)||window!=op.pin.window||!field(m,bound.brush,0x740,graph,e)||graph!=op.graph)
        return fail(e,e.empty()?"Zone private original brush ownership differs":e);
    poly::BorrowedPaths paths(stamp.shape);poly::Storage shape;bool made=false;const auto built=shape_make(shape,paths.view,made);
    bool okay=native_ok(built,"stamp shape construction",e);a::TerrainShape exact;
    if(okay)okay=poly::read(m,uintptr_t(&shape),exact,e)&&exact==stamp.shape;
    if(okay){
        selection::Scope scope(state.selection,bound);selection::PaintScope painting(state.private_brush,state.inside_paint,bound.brush);
        const auto seh=stamp_call(op,stamp,shape);
        okay=native_ok(seh,"AddPaint",e);if(okay)++op.stamps;
    }
    const auto cleanup=made?unary(reinterpret_cast<Unary>(state.image+0xf15a0),&shape):0;
    if(cleanup){e+="; Zone stamp shape destructor exception="+std::to_string(cleanup);return false;}
    return okay||fail(e,e.empty()?"Zone original stamp shape differs":e);
}
bool capture_unchanged(Memory& m,const Capture& c,const d::Sample& before,std::string& e){
    Pin current;if(!pin(m,c.binding,c.snapshot,c.intent.region,c.intent.character_type,current,e)||!same_origin(current,c.pin))return fail(e,e.empty()?"Zone capture full owner changed":e);
    d::Sample after;if(!d::sample(m,current.region,after,e))return false;
    return d::sample_json(after)==d::sample_json(before)||fail(e,"Zone suppressed callback changed native state/ledger/RNG/Undo");
}
void press_entry(void* brush,void* cursor,const float* point)try{
    if(!net::session_wire::connected()){state.press(brush,cursor,point);return;}
    const auto controller=typed_action_runtime::controller_status();std::string e;
    if(!state.ready||GetCurrentThreadId()!=state.thread||state.capture||state.pending||state.capture_fault||
       !controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)){reject("Zone Press requires idle bound Controller/capture");return;}
    Memory m;uintptr_t owned_brush=0,owned_cursor=0,region=0,window=0,collision=0,zone=0,graph=0;int32_t painting=0;
    if(!d::tool(m,state.image,owned_brush,owned_cursor,e)||owned_brush!=uintptr_t(brush)||owned_cursor!=uintptr_t(cursor)||
       !field(m,owned_brush,0x170,region,e)||!field(m,owned_brush,0x6fc,painting,e)||painting||
       !field(m,owned_brush,0x740,graph,e)||graph||!field(m,owned_cursor,0x230,collision,e)){
        reject(e.empty()?"Zone Press actual inventory brush/cursor or idle state differs":e);return;}
    const auto collision_call=[&]()noexcept->uint32_t{__try{zone=uintptr_t(reinterpret_cast<void*(*)(void*)>(state.image+0x523950)(reinterpret_cast<void*>(collision)));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}};
    if(!native_ok(collision_call(),"collision GetZone",e)||zone){reject(e.empty()?"Zone creation Press currently targets an existing Zone":e);return;}
    Capture c;c.brush=owned_brush;c.cursor=owned_cursor;c.snapshot=net::current_snapshot_id();c.ui_token=state.ui_token;
    std::vector<Option> choices;json selected;
    if(!d::options(m,state.image,c.brush,window,choices,selected,e)||!selected.is_array()||selected.size()!=2||selected[0]!=1||
       !field(m,region,0x834,c.intent.region,e)||!field(m,c.brush,0x17c,c.intent.level,e)||
       !typed_action_runtime::network_binding(c.snapshot,c.binding,e)){reject(e.empty()?"Zone Press Monster selection/Region/level differs":e);return;}
    c.intent.character_type=selected[1].get<int32_t>();
    if(!pin(m,c.binding,c.snapshot,c.intent.region,c.intent.character_type,c.pin,e,window)||c.pin.window!=window||
       !d::sample(m,c.pin.region,c.before,e)||c.before.undo.active_graph){reject(e.empty()?"Zone Press full owner/Undo differs":e);return;}
    state.capture=std::move(c);const auto seh=press_call(brush,cursor,point);
    if(!native_ok(seh,"capture Press",e)){state.capture_fault=true;state.capture->cause=e;reject(e);return;}
    int32_t operations=-1;
    if(!field(m,owned_brush,0x740,state.capture->graph,e)||!state.capture->graph||
       !field(m,state.capture->graph,0x28,operations,e)||operations||!capture_unchanged(m,*state.capture,state.capture->before,e)){
        state.capture_fault=true;state.capture->cause=e.empty()?"Zone capture lacks unchanged original empty graph":e;reject(state.capture->cause);}
}catch(const std::exception& x){const auto e=std::string("Zone Press boundary: ")+x.what();if(state.capture){state.capture_fault=true;state.capture->cause=e;}reject(e);}
 catch(...){if(state.capture){state.capture_fault=true;state.capture->cause="Zone Press non-standard boundary exception";}reject("Zone Press non-standard boundary exception");}
void paint_entry(void* brush,const void* shape)try{
    if(!net::session_wire::connected()){state.paint(brush,shape);return;}
    if(!state.capture||state.capture->brush!=uintptr_t(brush)){reject("Zone AddPaint lacks owned suppressed capture");return;}
    auto& c=*state.capture;if(!c.cause.empty())return;Memory m;a::ZoneStamp stamp;std::string e;
    if(GetCurrentThreadId()!=state.thread||!field(m,c.brush,0x5cc,stamp.position,e)||!poly::read(m,uintptr_t(shape),stamp.shape,e)){
        c.cause=e.empty()?"Zone AddPaint thread/shape differs":e;reject(c.cause);return;}
    c.intent.stamps.push_back(std::move(stamp));if(!a::valid_zone_intent(c.intent)){c.cause="Zone captured geometry exceeds typed action domain";reject(c.cause);}
}catch(const std::exception& x){if(state.capture)state.capture->cause=std::string("Zone AddPaint boundary: ")+x.what();reject(state.capture?state.capture->cause:"Zone AddPaint boundary without capture");}catch(...){if(state.capture)state.capture->cause="Zone AddPaint non-standard boundary exception";reject("Zone AddPaint non-standard boundary exception");}
void finished_entry(void* brush,const void* shape)try{
    if(!net::session_wire::connected()){state.finished(brush,shape);return;}
    if(!state.capture||state.capture->brush!=uintptr_t(brush)){reject("Zone Finish lacks owned capture");return;}
    auto& c=*state.capture;Memory m;std::string e;uintptr_t graph=0;int32_t operations=-1;
    if(state.capture_fault||c.cleanup_attempted||GetCurrentThreadId()!=state.thread||!field(m,c.brush,0x740,graph,e)||!graph||graph!=c.graph||
       !field(m,graph,0x28,operations,e)||operations!=0){reject(e.empty()?"Zone capture empty graph ownership differs; no cleanup replay":e);return;}
    // Host simulation may advance between mouse callbacks. The empty native
    // Finish must preserve the immediate state, not the old Press-time economy.
    d::Sample before_finish;if(!d::sample(m,c.pin.region,before_finish,e)){reject(e);return;}
    c.cleanup_attempted=true;const auto seh=binary(state.finished,brush,shape);
    if(!native_ok(seh,"empty capture Finish",e)||!capture_unchanged(m,c,before_finish,e)){state.capture_fault=true;c.cause=e;reject(e);return;}
    auto done=std::move(c);state.capture.reset();
    if(!done.cause.empty()){reject(done.cause);return;}
    if(!a::valid_zone_intent(done.intent)){reject("Zone completed capture has no valid original stamps");return;}
    if(!done.ui_token.empty())state.ui_jobs.at(done.ui_token)["captured"]=true;
    state.pending=std::move(done);++state.captures;
}catch(const std::exception& x){state.capture_fault=true;reject(std::string("Zone Finish boundary: ")+x.what());}catch(...){state.capture_fault=true;reject("Zone Finish non-standard boundary exception");}
}
bool verify_owned_entry(uintptr_t rva,std::string& e){
    if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(e,"Zone owned code check requires installed bound Mainthread");
    Memory m;for(size_t i=0;i<4;++i)if(monster_zone_action_codes::values[i].rva==rva)return code(m,i,true,e);
    return fail(e,"Zone owned code check requested unrelated entry");
}
bool enable(std::string& e){
    if(state.ready){
        if(GetCurrentThreadId()!=state.thread||state.capture||state.pending)return fail(e,"Zone previous capture/Mainthread unresolved");
        for(const auto& [id,op]:state.operations)if(!op.retired)return fail(e,"Zone previous action unretired="+id);
        Memory m;if(!all_code(m,true,e))return false;state.operations.clear();state.ui_jobs.clear();state.ui_token.clear();state.cause.clear();state.capture_fault=false;return true;
    }
    if(state.install_attempted)return fail(e,"Zone previous hook installation failed: "+state.install_cause);
    state.install_attempted=true;std::string sha;uint32_t thread=0;
    const auto failed=[&](const std::string& cause){state.install_cause=cause;e=cause;return false;};
    if(!native::read_resolver_receipt(state.image,thread,sha,e)||sha!=exe_sha||!thread||thread!=GetCurrentThreadId())return failed("Zone exact EXE/Mainthread differs: "+e);
    state.thread=thread;Memory m;if(!all_code(m,false,e))return failed(e);
    struct Hook {uintptr_t rva;void* entry;void** original;};
    const Hook hooks[]{
        {0x5ef6a0,reinterpret_cast<void*>(&press_entry),reinterpret_cast<void**>(&state.press)},
        {0x5f1510,reinterpret_cast<void*>(&paint_entry),reinterpret_cast<void**>(&state.paint)},
        {0x5f0990,reinterpret_cast<void*>(&finished_entry),reinterpret_cast<void**>(&state.finished)},
        {0x67fb40,reinterpret_cast<void*>(&selection_entry),reinterpret_cast<void**>(&state.get)}};
    for(const auto& h:hooks){const auto result=MH_CreateHook(reinterpret_cast<void*>(state.image+h.rva),h.entry,h.original);
        if(result!=MH_OK||!*h.original)return failed("Zone MH_CreateHook RVA="+std::to_string(h.rva)+" status="+std::to_string(result));}
    for(const auto& h:hooks){const auto result=MH_EnableHook(reinterpret_cast<void*>(state.image+h.rva));
        if(result!=MH_OK)return failed("Zone MH_EnableHook RVA="+std::to_string(h.rva)+" status="+std::to_string(result));}
    if(!all_code(m,true,e))return failed(e);state.ready=true;return true;
}
void tick(){
    const auto controller=typed_action_runtime::controller_status();
    for(auto& [token,job]:state.ui_jobs){(void)token;if(!job.value("submitted",false)||job.value("terminal",false))continue;
        if(controller.value("action_uuid",std::string{})==job.value("action_uuid",std::string{})&&!controller.value("pending",true)){
            job["terminal"]=true;job["outcome"]=controller.value("outcome",0);job["confirmed"]=controller.value("outcome",0)==int(a::Outcome::Confirmed);
            job["cause"]=controller.value("cause",std::string{});}}
    if(!state.pending)return;
    auto c=std::move(*state.pending);state.pending.reset();std::string e;a::Binding binding;Memory m;Pin current;
    const auto rejected=[&](const std::string& cause){
        reject(cause);if(!c.ui_token.empty()){auto& job=state.ui_jobs.at(c.ui_token);job["terminal"]=true;
            job["confirmed"]=false;job["cause"]=cause;job["outcome"]=int(a::Outcome::RejectedNoEffect);}};
    if(!state.ready||GetCurrentThreadId()!=state.thread){rejected("Zone pending capture lost Mainthread");return;}
    if(!typed_action_runtime::network_binding(c.snapshot,binding,e)||binding!=c.binding||
       !pin(m,binding,c.snapshot,c.intent.region,c.intent.character_type,current,e)||!same_origin(current,c.pin)){
        rejected(e.empty()?"Zone pending capture changed full owner":e);return;}
    const auto id=action_runtime::uuid(e);
    if(id.empty()||!typed_action_runtime::submit_zone(id,c.intent,e)){rejected(e.empty()?"Zone Controller submit failed":e);return;}
    state.last_action=id;++state.submits;if(!c.ui_token.empty()){auto& job=state.ui_jobs.at(c.ui_token);job["submitted"]=true;job["action_uuid"]=id;}
}
bool pending_ui(){return bool(state.capture)||bool(state.pending);}
bool begin_ui_capture(const std::string& token,std::string& e){
    if(!state.ready||GetCurrentThreadId()!=state.thread||pending_ui()||state.capture_fault||!state.ui_token.empty()||
       state.ui_jobs.contains(token)||state.ui_jobs.size()>=64)return fail(e,"Zone fixed UI capture owner/token/pending state differs");
    state.ui_token=token;state.ui_jobs.emplace(token,json{{"captured",false},{"submitted",false},{"confirmed",false},{"terminal",false},{"action_uuid",""}});return true;
}
void end_ui_capture(){state.ui_token.clear();}
json ui_receipt(const std::string& token){const auto found=state.ui_jobs.find(token);return found==state.ui_jobs.end()?json(nullptr):found->second;}
json status(){
    json operations=json::object();for(const auto& [id,op]:state.operations)operations[id]={
        {"attempted",op.attempted},{"returned",op.returned},{"confirmed",op.confirmed},{"retired",op.retired},
        {"original_stamps",op.stamps},{"local_zone_uid",op.uid},{"zone_key",op.request.canonical_record_key},
        {"zones_before",op.evidence.zone_before.zones},{"zones_after",op.evidence.zone_after.zones},
        {"monsters_before",op.evidence.zone_before.monsters},{"monsters_after",op.evidence.zone_after.monsters},
        {"content_before",op.evidence.zone_before.content_sha256},{"content_after",op.evidence.zone_after.content_sha256},{"cause",op.cause}};
    return {{"enabled",state.ready},{"pending_ui",pending_ui()},{"capture_active",bool(state.capture)},{"capture_fault",state.capture_fault},{"captured",state.captures},{"submitted",state.submits},
        {"rejected",state.rejections},{"last_action",state.last_action},{"cause",state.cause},{"operations",std::move(operations)},{"ui_jobs",state.ui_jobs}};
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& e)try{
    if(request.action!=a::Action::PlaceMonsterZone||!a::valid_zone_intent(request.zone)||
       !state.ready||state.thread!=GetCurrentThreadId()){e="Zone runtime/Mainthread/typed intent unavailable";return a::Outcome::RejectedNoEffect;}
    if(state.operations.contains(request.action_uuid)){e="Zone action already attempted; no replay";return a::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){e="Zone lacks exact pending Controller request";return a::Outcome::RejectedNoEffect;}
    Memory m;Pin before;d::Sample sample;a::ZoneState value;
    if(!all_code(m,true,e)||!pin(m,binding,request.snapshot_uuid,request.zone.region,request.zone.character_type,before,e)||
       !effect(m,before,sample,value,e))return state.capture_fault?a::Outcome::Unknown:a::Outcome::RejectedNoEffect;
    if(sample.undo.active_graph||sample.state.zones.size()>=128||sample.state.monsters.size()>=4096){e="Zone action requires idle Undo and bounded native census";return a::Outcome::RejectedNoEffect;}
    if(request.zone.level<before.min_level||request.zone.level>before.max_level){e="Zone requested level is outside original Region level range";return a::Outcome::RejectedNoEffect;}
    if(request.zone.character_type>3){e="Zone basic Monster action does not yet adopt the original achievement effect for catalogue IDs above 3";return a::Outcome::RejectedNoEffect;}
    for(const auto& stamp:request.zone.stamps){uintptr_t region=0;const auto seh=shard_region(state.image,before.world.root,stamp.position,region);
        if(seh||region!=before.region.region){e="Zone stamp original Region differs exception="+std::to_string(seh);return a::Outcome::RejectedNoEffect;}}
    bool preparation_unknown=false;
    if(!no_overlap(m,request.zone,sample,preparation_unknown,e)){
        if(!preparation_unknown)return a::Outcome::RejectedNoEffect;
        auto& failed=state.operations.try_emplace(request.action_uuid).first->second;
        failed.binding=binding;failed.request=request;failed.pin=before;failed.attempted=true;failed.cause=e;
        return a::Outcome::Unknown;
    }
    const auto local_region_random=value.region_random;
    if(authority&&(!a::zone_shared_content(value,authority->zone_before)||ledger(sample.money)!=authority->before)){
        e="Zone Client before native census/shape/Undo/Release/counters/full Ledger differs from Host";return a::Outcome::RejectedNoEffect;}
    if(!npc_quest_target_runtime::begin_zone_append(request.zone.region,request.action_uuid,e))return a::Outcome::RejectedNoEffect;
    auto& op=state.operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;op.pin=before;
    const auto unknown=[&](const std::string& cause){e=op.cause=cause;return a::Outcome::Unknown;};
    if(authority){const auto seh=action_runtime::seed(state.image,authority->zone_before.global_random);
        if(!native_ok(seh,"Host global RNG adoption",e))return unknown(e);
        if(!effect(m,before,sample,value,e))return unknown(e);
        if(value.region_random!=local_region_random)return unknown("Zone Host global RNG adoption changed local Region RNG");
        if(!a::zone_shared_state(value,authority->zone_before)||ledger(sample.money)!=authority->before)return unknown("Zone Host global RNG adoption/shared readback differs");}
    op.evidence.before=ledger(sample.money);op.evidence.zone_before=value;op.attempted=true;
    if(!native_ok(construct(op),"private brush/graph construction",e))return unknown(e);
    for(const auto& stamp:request.zone.stamps)if(!paint_private(op,stamp,e))return unknown(e);
    if(!field(m,uintptr_t(op.brush->bytes.data()),0x710,op.created,e)||!op.created||!field(m,op.created,0x18,op.uid,e)||!op.uid)
        return unknown(e.empty()?"Zone original AddPaint created no owned Zone":e);
    const auto finish=binary(state.finished,op.brush->bytes.data(),nullptr);
    if(!native_ok(finish,"PaintingFinished",e))return unknown(e);op.returned=true;
    uintptr_t graph=0;if(!field(m,uintptr_t(op.brush->bytes.data()),0x740,graph,e)||graph)return unknown(e.empty()?"Zone original Finish retained private graph":e);
    const auto destroy=unary(reinterpret_cast<Unary>(state.image+0x5f1c20),op.brush->bytes.data());
    if(!native_ok(destroy,"private brush destructor",e))return unknown(e);op.constructed=false;
    Pin after;if(!pin(m,binding,request.snapshot_uuid,request.zone.region,request.zone.character_type,after,e)||!same_origin(before,after))return unknown(e.empty()?"Zone original full owner changed":e);
    d::Sample result;if(!d::sample(m,after.region,result,e))return unknown(e);
    if(!monster_zone_effect::created(sample.state,result.state,op.created,op.uid,request.zone.character_type,request.zone.level,e))return unknown(e);
    const auto created=std::find_if(result.state.zones.begin(),result.state.zones.end(),[&](const auto& row){return row.value.pointer==op.created&&row.value.uid==op.uid;});
    if(created==result.state.zones.end()||created->value.content.type!=1||created->value.content.level!=request.zone.level||
       created->value.content.monster_type!=request.zone.character_type)return unknown("Zone original result type/level/CharacterType differs");
    std::string key;if(!npc_quest_target_runtime::finish_zone_append(request.zone.region,request.action_uuid,op.created,op.uid,created->value.content,key,e)||key!=request.canonical_record_key)return unknown(e.empty()?"Zone appended canonical key differs":e);
    a::ZoneState final;if(!effect(m,after,result,final,e))return unknown(e);
    op.evidence.after=ledger(result.money);op.evidence.zone_after=final;
    if(!a::zone_transition(value,final)||op.evidence.before!=op.evidence.after||result.undo.active_graph||
       result.undo.undo.empty()||result.undo.undo.back()!=op.graph)return unknown("Zone original shape/spawn/counter/Release/Undo/full Ledger transition differs");
    op.evidence.undo_confirmed=true;
    if(authority&&(!a::zone_shared_state(op.evidence.zone_after,authority->zone_after)||op.evidence.after!=authority->after))return unknown("Zone Client original result differs from Host exact shape/spawn/RNG/Release/Undo/full Ledger");
    op.confirmed=true;e.clear();return a::Outcome::Confirmed;
}catch(const std::exception& x){e=std::string("Zone native action boundary: ")+x.what();const auto found=state.operations.find(request.action_uuid);if(found!=state.operations.end())found->second.cause=e;return a::Outcome::Unknown;}
 catch(...){e="Zone native action non-standard boundary exception";const auto found=state.operations.find(request.action_uuid);if(found!=state.operations.end())found->second.cause=e;return a::Outcome::Unknown;}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& e){
    const auto found=state.operations.find(request.action_uuid);
    if(found==state.operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)return fail(e,"Zone observe lacks confirmed exact original action");
    const auto& op=found->second;Memory m;Pin current;d::Sample sample;a::ZoneState value;
    if(!pin(m,binding,request.snapshot_uuid,request.zone.region,request.zone.character_type,current,e)||!same_origin(current,op.pin)||
       !effect(m,current,sample,value,e))return false;
    if(value!=op.evidence.zone_after||ledger(sample.money)!=op.evidence.after||sample.undo.active_graph)return fail(e,"Zone confirmed original state/ledger changed before ACK");
    out=op.evidence;out.retirement_confirmed=op.retired;e.clear();return true;
}
bool retire(const a::Binding& binding,const a::Request& request,std::string& e){
    a::Evidence actual;if(!observe(binding,request,actual,e))return false;auto& op=state.operations.at(request.action_uuid);
    if(op.retired)return fail(e,"Zone original action already retired");op.retired=true;op.evidence.retirement_confirmed=true;return true;
}
}
