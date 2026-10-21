namespace modular {
namespace {
struct Presentation {action::Binding binding;std::string snapshot;uintptr_t receiver=0;bool attempted=false,confirmed=false;std::string cause;};
std::map<std::string,Presentation> presentations;
bool capture_input(Runtime& r,uintptr_t receiver,uintptr_t cursor,const void* position,bool prefab,Pending& out,std::string& e,bool right=false){
    out={};const auto state=typed_action_runtime::controller_status();bool own=false;
    if(!installed||!r.ready||queued||!state.value("started",false)||state.value("locked",true)||state.value("pending",true)||!scenery_kiss::live_cursor(r.memory.process,r.image,receiver,own,e)||!own||!q::typed(r.memory,r.image,receiver,0x17414c0,e)||!code(r,e))return q::fail(e,e.empty()?"Modular input requires the exact live cursor and idle Controller":e);
    int32_t mode=-1;if(!q::at(r.memory,receiver,0x168,mode,e)||mode<0||mode>4||prefab!=(mode==0)||(right&&mode!=1))return q::fail(e,e.empty()?"Modular native cursor mode differs":e);if(right)mode=2;out.intent.mode=uint8_t(mode);
    out.snapshot=net::current_snapshot_id();if(!typed_action_runtime::network_binding(out.snapshot,out.binding,e))return false;
    host::WorldState world_value;if(!world(r,world_value,e)||world_value.origin.world_epoch!=out.binding.native_epoch||world_value.origin.generation!=out.binding.native_generation||world_value.saves||world_value.edits||world_value.active_graph)return q::fail(e,e.empty()?"Modular input has a foreign Save/Edit/Graph or World":e);
    uintptr_t region=0;json source_records=nullptr;
    if(prefab){uintptr_t preview=0,layout=0,plan=0,theme=0,blueprint=0;uint8_t commit=0,box_valid=0;
        if(!q::at(r.memory,receiver,0x671,commit,e)||commit!=1||!q::at(r.memory,receiver,0x6c8,preview,e)||!preview||!q::at(r.memory,preview,0x100,out.intent.pose,e)||!q::at(r.memory,receiver,0x378,layout,e)||!layout||!q::at(r.memory,receiver,0x380,plan,e)||!plan||!q::at(r.memory,receiver,0x6d0,theme,e)||!theme||!prop_text::text(r,theme+0x20,out.intent.theme,e)||!q::at(r.memory,receiver,0x6d8,blueprint,e)||!blueprint||!q::at(r.memory,blueprint,0x48,box_valid,e)||box_valid!=1||!q::at(r.memory,blueprint,0x4c,out.intent.clearance,e))return q::fail(e,e.empty()?"Modular native Prefab commit/Layout/Plan/Theme/clearance owner differs":e);
        for(size_t i=0;i<3;++i)out.intent.position[i]=out.intent.pose[4+i];std::string digest;if(!owned_record(r,layout,false,out.intent.layout,digest,e)||!owned_record(r,plan,true,out.intent.plan,digest,e))return false;
        const calls::String name{out.intent.theme.data(),out.intent.theme.size(),{}};calls::Work lookup;lookup.op=calls::Op::Theme;lookup.input=&name;if(!call(r,lookup,e)||lookup.result!=theme)return q::fail(e,e.empty()?"Modular native selected Theme lacks unique canonical name":e);
        calls::Work locate;locate.op=calls::Op::Region;locate.object=uintptr_t(world_value.origin.map);locate.input=out.intent.position.data();if(!call(r,locate,e)||!locate.result)return q::fail(e,e.empty()?"Modular Prefab preview has no original Region":e);region=locate.result;
    }else{if(!position||!r.memory.read(uintptr_t(position),out.intent.position.data(),12,e))return false;calls::Work find;find.op=calls::Op::Find;find.object=receiver;find.other=cursor;find.input=out.intent.position.data();find.number=mode==1?1:0;if(!call(r,find,e)||!find.result)return q::fail(e,e.empty()?"Modular original hit has no modular Building":e);
        uint64_t uid=0;if(!q::at(r.memory,find.result,0x18,uid,e)||!q::at(r.memory,find.result,0x178,region,e)||!region)return false;
        duplicate_edit::Sample current;std::string diagnostic_key;
        const auto select=[&](duplicate_edit::Sample& sampled,std::string& why){const auto target=std::find_if(sampled.handles.begin(),sampled.handles.end(),[&](const auto& row){return row.second.object==find.result&&row.second.uid==uid;});if(target==sampled.handles.end())return q::fail(why,"Modular hit has no exact existing canonical Building owner");if(sampled.objects.at(target->first).family!=Family::Building)return q::fail(why,"Modular hit canonical owner is not a Building");diagnostic_key=target->first;return true;};
        if(!duplicate_edit::sample(r,out.binding,nullptr,{},current,e,select,false,false,diagnostic_key))return false;
        out.intent.target=diagnostic_key;out.intent.before_record_sha=current.objects.at(diagnostic_key).record_sha;source_records=std::move(current.selected_records);
    }
    if(!q::at(r.memory,region,0x834,out.intent.region,e))return false;uintptr_t checked=0;if(!scenery_stroke::region(r,world_value,out.intent.region,region,checked,e)||!action::valid_modular(out.intent))return q::fail(e,e.empty()?"Modular native complete input domain differs":e);
    host::WorldState final;if(!world(r,final,e)||!host::same_world(world_value.origin,final.origin)||final.saves||final.edits||final.active_graph)return q::fail(e,e.empty()?"Modular input capture changed its World admission":e);
    out.id=uuid(e);if(out.id.empty())return false;auto message=std::make_unique<action::Message>();auto& proposal=*message;proposal.kind=action::Kind::Request;proposal.session=1;proposal.request.action=action::Action::EditModularBuilding;proposal.request.action_uuid=out.id;proposal.request.snapshot_uuid=out.snapshot;proposal.request.peer=1;proposal.request.sequence=1;proposal.request.canonical_record_key="modular:"+out.id;proposal.request.modular=out.intent;std::vector<uint8_t> wire;if(!action::encode(proposal,wire,e))return false;
    last_source_records=Config::instance().diagnostic_mailbox_enabled&&out.intent.mode?json{{"action_uuid",out.id},{"snapshot_uuid",out.snapshot},{"native_epoch",out.binding.native_epoch},{"native_generation",out.binding.native_generation},{"phase","source_capture"},{"target",out.intent.target},{"before_record_sha",out.intent.before_record_sha},{"records",std::move(source_records)}}:json(nullptr);return true;
}
void press_entry(void* receiver,void* cursor,const void* position){auto* r=published.load(std::memory_order_acquire);const auto original=reinterpret_cast<void(*)(void*,void*,const void*)>(originals[0]);
    if(!r||!original){reject("Modular Press has no published original");return;}if(!net::session_wire::connected()||!typed_action_runtime::status().value("started",false)){original(receiver,cursor,position);return;}if(r->thread!=GetCurrentThreadId()){reject("Modular joined Press requires the bound Mainthread");return;}
    try{Pending input;std::string e;if(!capture_input(*r,uintptr_t(receiver),uintptr_t(cursor),position,false,input,e)){reject(e);return;}queued=std::move(input);++captures;}catch(const std::exception& error){reject(std::string("Modular Press capture: ")+error.what());}
}
void right_entry(void* receiver,void* cursor,const void* position){auto* r=published.load(std::memory_order_acquire);const auto original=reinterpret_cast<void(*)(void*,void*,const void*)>(originals[2]);
    if(!r||!original){reject("Modular RightPress has no published original");return;}if(!net::session_wire::connected()||!typed_action_runtime::status().value("started",false)){original(receiver,cursor,position);return;}if(r->thread!=GetCurrentThreadId()){reject("Modular joined RightPress requires the bound Mainthread");return;}
    try{int32_t mode=-1;std::string e;if(!q::at(r->memory,uintptr_t(receiver),0x168,mode,e)||mode<0||mode>4){reject(e.empty()?"Modular native RightPress mode differs":e);return;}if(mode!=1)return;
        // The original mode-1 right gesture is the same block removal as left mode 2.
        Pending input;if(!capture_input(*r,uintptr_t(receiver),uintptr_t(cursor),position,false,input,e,true)){reject(e);return;}queued=std::move(input);++captures;
    }catch(const std::exception& error){reject(std::string("Modular RightPress capture: ")+error.what());}
}
void prefab_entry(void* receiver,float dt){auto* r=published.load(std::memory_order_acquire);const auto original=reinterpret_cast<void(*)(void*,float)>(originals[1]);
    if(!r||!original){reject("Modular Prefab Update has no published original");return;}if(!net::session_wire::connected()||!typed_action_runtime::status().value("started",false)){original(receiver,dt);return;}if(r->thread!=GetCurrentThreadId()){reject("Modular joined Prefab Update requires the bound Mainthread");return;}
    try{uint8_t commit=0;std::string e;if(!q::at(r->memory,uintptr_t(receiver),0x671,commit,e)||commit>1){reject(e.empty()?"Modular native commit flag differs":e);return;}if(!commit){original(receiver,dt);return;}
        Pending input;if(!capture_input(*r,uintptr_t(receiver),0,nullptr,true,input,e)){reject(e);return;}calls::Work consume;consume.op=calls::Op::ConsumePrefab;consume.object=uintptr_t(receiver);if(!call(*r,consume,e)||!consume.accepted){reject(e.empty()?"Modular source Prefab flag changed before original placement cleanup":e);return;}
        presentations.emplace(input.id,Presentation{input.binding,input.snapshot,uintptr_t(receiver)});queued=std::move(input);++captures;original(receiver,dt);
    }catch(const std::exception& error){reject(std::string("Modular Prefab capture: ")+error.what());}
}
}
bool enable(std::string& e){auto* r=published.load(std::memory_order_acquire);if(installed)return r&&code(*r,e);if(installation_attempted||!r||!r->ready||r->thread!=GetCurrentThreadId())return q::fail(e,"Modular routes require one ready Mainthread installation");installation_attempted=true;if(!code(*r,e))return false;
    const std::array<uintptr_t,3> rvas{0x5bad60,0x5bc0c0,0x5ba2c0},detours{uintptr_t(&press_entry),uintptr_t(&prefab_entry),uintptr_t(&right_entry)};std::array<bool,3> enabled{};
    const auto failed=[&](){for(size_t i=0;i<enabled.size();++i)if(enabled[i]){const auto result=MH_DisableHook(reinterpret_cast<void*>(r->image+rvas[i]));if(result!=MH_OK)e+="; Modular owned route disable="+std::to_string(result);}reject(e);return false;};
    for(size_t i=0;i<rvas.size();++i){void* original=nullptr;const auto target=reinterpret_cast<void*>(r->image+rvas[i]);const auto guard=std::find_if(std::begin(modular_code::guards),std::end(modular_code::guards),[&](const auto& row){return row.rva==rvas[i];});if(guard==std::end(modular_code::guards)){e="Modular original route guard is absent";return failed();}route_owners[i]={r->image+rvas[i],detours[i],0,1,modular_code::route_stolen(rvas[i]),std::vector<unsigned char>(guard->original,guard->original+guard->bytes)};
        if(!hooks::create_hook(target,reinterpret_cast<void*>(detours[i]),&original)||!original){e="Modular named CreateHook failed index="+std::to_string(i);return failed();}originals[i]=uintptr_t(original);route_owners[i].trampoline=uintptr_t(original);const auto status=MH_EnableHook(target);if(status!=MH_OK){e="Modular named EnableHook failed index="+std::to_string(i)+" original="+MH_StatusToString(status);return failed();}enabled[i]=true;
#ifdef MT2MP_FRAMEWORK_PLUGIN
        MT2FW_MT2_RouteReceipt receipt{};auto& backend=hooks::framework::process_backend();if(!backend.read_route(target,receipt)||!receipt.generation){e="Modular enabled route receipt unavailable: "+backend.error();return failed();}route_owners[i].generation=receipt.generation;
#endif
    }
    if(!code(*r,e))return failed();installed=true;e.clear();return true;
}
bool pending(){return bool(queued);}
}
