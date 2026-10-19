namespace quest_list {
namespace {
namespace qa=quest_authoring;
namespace guards=npc_quest_list_ui_codes;
namespace vr=npc_quest_visitor_reader;
using Command=a::ui_diagnostic::UiCommand;
struct Hook {Command original=nullptr;uint64_t generation=0;};
Hook npc_hook,selector_hook;
bool ready=false,installed=false,fault=false,replaying=false;
uintptr_t replay_receiver=0;bool replay_remove=false;a::QuestListEdit replay_edit=a::QuestListEdit::List;uint32_t original_calls=0;
std::string cause,ui_token,last_action;uint64_t captures=0,submitted=0,rejected=0;
struct Pending {a::Binding binding;std::string snapshot,owner,token;a::NpcQuestListIntent intent;};
struct References {
    uintptr_t manager=0,data=0;
    std::vector<std::array<uintptr_t,3>> local;
    json rows=json::array();
};
struct Operation {
    a::Binding binding;a::Request request;Pin pin;uintptr_t owner=0,graph=0;
    qr::Witness before,after;References refs_before,refs_after;d::Sample sample;a::Evidence evidence;
    bool attempted=false,returned=false,seed_attempted=false,confirmed=false,retired=false;
    uint32_t calls=0;std::string cause;
};
std::optional<Pending> pending;std::map<std::string,Operation> operations;std::map<std::string,json> jobs;
bool npc_entry(void*,void*);bool selector_entry(void*,void*);
bool reject(const std::string& why){++rejected;if(cause!=why){cause=why;LOG_ERROR("[NPCQuestList] {}",why);}return false;}
bool inner_codes(Memory& m,std::string& e){
    for(const auto& code:npc_quest_list_codes::values){std::string bytes(code.size,'\0'),sha;
        if(!m.copy(state.image+code.rva,bytes.data(),bytes.size(),e)||!quest_bridge::sha256(bytes,sha,e))return false;
        if(sha!=code.sha)return fail(e,"Quest list original transaction body differs RVA="+std::to_string(code.rva));}
    return true;
}
bool read_route(uintptr_t entry,MT2FW_MT2_RouteReceipt& route,std::string& e){
#if defined(MT2MP_FRAMEWORK_PLUGIN)
    auto& backend=mt2mp::hooks::framework::process_backend();
    if(!backend.read_route(reinterpret_cast<void*>(entry),route))return fail(e,backend.error());return true;
#else
    (void)entry;(void)route;return fail(e,"Quest list owned route requires the Framework backend");
#endif
}
bool verify_code(uintptr_t entry,const void* bytes,size_t count,std::string& e){
#if defined(MT2MP_FRAMEWORK_PLUGIN)
    return mt2mp::hooks::framework::process_backend().verify_code(entry,bytes,count,e);
#else
    (void)entry;(void)bytes;(void)count;return fail(e,"Quest list owned code verification requires Framework");
#endif
}
guards::Owned owned(uintptr_t rva){
    const auto& hook=rva==guards::npc.rva?npc_hook:selector_hook;
    return hook.generation?guards::Owned{rva==guards::npc.rva?uintptr_t(&npc_entry):uintptr_t(&selector_entry),uintptr_t(hook.original),hook.generation}:guards::Owned{};
}
bool bind_owner(Memory& m,const a::Binding& binding,const std::string& snapshot,const std::string& key,int32_t region,
                action_runtime::ui::LocalBinding& local,Pin& p,std::string& e){
    uintptr_t native_region=0;npc_census::State census;
    if(!action_runtime::target_binding(npc_target_key::argument(key),binding,snapshot,local,native_region,census,e)||
       !pin(m,binding,snapshot,region,p,e)||native_region!=p.region.region)return fail(e,e.empty()?"Quest list owner Region differs":e);
    const auto found=std::find_if(census.rows.begin(),census.rows.end(),[&](const auto& row){return row.local_pointer==local.object&&row.local_uid==local.uid;});
    return (found!=census.rows.end()&&found->type==0&&found->quest_count>=0&&found->quest_count<=5)||fail(e,"Quest list requires a current QuestGiver");
}
json link_json(const vr::Link& link){return {{"state",int(link.state)},{"key",link.diagnostic_key}};}
bool references(Memory& m,const a::Binding& binding,const std::string& snapshot,const qr::Witness& witness,References& out,std::string& e){
    out={};uintptr_t ap=0;int32_t count=0,capacity=0;
    if(!field(m,state.image,0x13df690,out.manager,e)||!field(m,out.manager,0,ap,e)||ap!=state.image+0x1725f40||
       !field(m,out.manager,0x40,out.data,e)||!field(m,out.manager,0x48,count,e)||!field(m,out.manager,0x4c,capacity,e)||
       count<0||capacity<count||count>1000000||(count&&!out.data))return fail(e,e.empty()?"Quest list Subscriber census differs":e);
    a::Context context;std::vector<action_runtime::StateOwner> owners;
    if(!a::selected_session().current(context,e)||context.snapshot_uuid!=snapshot||!action_runtime::state_owners(context,binding,owners,e))
        return fail(e,e.empty()?"Quest list current active-reference owner context differs":e);
    std::map<std::pair<vr::Kind,uintptr_t>,std::string> identities;
    for(const auto& owner:owners){
        if(!identities.emplace(std::make_pair(vr::Kind::Npc,owner.npc),owner.key).second)return fail(e,"Quest list active NPC owner duplicated");
        for(const auto& [slot,pointer]:owner.quests){
            const auto found=witness.owners.find(owner.npc);
            if(found==witness.owners.end()||found->second.key!=owner.key||slot<0||size_t(slot)>=found->second.state.local.size()||
               found->second.state.local[size_t(slot)].quest!=pointer||!identities.emplace(std::make_pair(vr::Kind::Quest,pointer),owner.key+"/quest/"+std::to_string(slot)).second)
                return fail(e,"Quest list active Quest owner/current slot differs from actual global witness");
        }
    }
    std::map<int32_t,json> rows;std::set<uintptr_t> unique;
    for(int32_t i=0;i<count;++i){uintptr_t subscriber=0,toon=0;int32_t id=-1;
        if(!field(m,out.data,size_t(i)*8,subscriber,e)||!subscriber||!unique.insert(subscriber).second||
           !field(m,subscriber,0,ap,e)||ap!=state.image+0x16e75f0||!field(m,subscriber,0x20,id,e)||id<0||
           !field(m,subscriber,0x80,toon,e)||rows.contains(id))return fail(e,e.empty()?"Quest list Subscriber identity duplicated or differs":e);
        out.local.push_back({subscriber,toon,uintptr_t(id)});
        json row={{"id",id},{"online",bool(toon)},{"progress",json::array()}};
        if(toon){vr::State value;
            const auto identify=[&](vr::Kind kind,uintptr_t pointer,std::string& key,std::string& why){
                const auto found=identities.find({kind,pointer});if(found==identities.end())return fail(why,"Quest list active reference lacks current canonical owner/slot");key=found->second;return true;};
            if(!vr::read(m,state.image,subscriber,identify,value,e))return false;
            row["gold"]=value.gold;row["xp_bits"]=std::bit_cast<uint32_t>(value.xp);row["level"]=value.level;row["quests_completed"]=value.quests_completed;
            for(const auto& progress:value.progress)row["progress"].push_back({{"npc",link_json(progress.npc)},{"quest",link_json(progress.quest)},
                {"completed",progress.completed},{"monsters_defeated",progress.monsters_defeated},{"visited",progress.visited},
                {"action_performed",progress.action_performed},{"complete",progress.complete}});
        }
        rows.emplace(id,std::move(row));
    }
    for(const auto& [id,row]:rows){(void)id;out.rows.push_back(row);}
    uintptr_t manager=0,data=0;int32_t final_count=0;
    if(!field(m,state.image,0x13df690,manager,e)||manager!=out.manager||!field(m,manager,0x40,data,e)||data!=out.data||
       !field(m,manager,0x48,final_count,e)||final_count!=count)return fail(e,e.empty()?"Quest list Subscriber census changed during read":e);
    for(size_t i=0;i<out.local.size();++i){uintptr_t pointer=0,toon=0;int32_t id=-1;
        if(!field(m,data,i*8,pointer,e)||pointer!=out.local[i][0]||!field(m,pointer,0x80,toon,e)||toon!=out.local[i][1]||
           !field(m,pointer,0x20,id,e)||uintptr_t(id)!=out.local[i][2])return fail(e,e.empty()?"Quest list Subscriber local binding changed during read":e);}
    return true;
}
bool state_read(Memory& m,const a::Binding& binding,const std::string& snapshot,const Pin& p,const action_runtime::ui::LocalBinding& local,
                const std::string& key,qr::Witness& witness,References& refs,d::Sample& sample,a::NpcQuestListState& value,std::string& e,const qr::Witness* previous=nullptr){
    qr::Witness seed;if(!previous){qr::Owner owner;owner.pointer=local.object;owner.uid=local.uid;owner.key=key;seed.owners.emplace(local.object,owner);previous=&seed;}
    if(!read_remove_quests(m,binding,snapshot,0,{},witness,e,previous)||!d::sample(m,p.region,sample,e)||!references(m,binding,snapshot,witness,refs,e))return false;
    const auto found=witness.owners.find(local.object);if(found==witness.owners.end()||found->second.uid!=local.uid||found->second.key!=key)return fail(e,"Quest list owner absent from current full witness");
    value={};value.records=found->second.state.records;value.count=uint32_t(witness.quests.size());value.completed=witness.completed;
    value.undo_count=sample.undo.undo.size();value.global_random=sample.state.global_random;
    return quest_digest(witness,value.all_sha256,e)&&quest_bridge::pool(p.region.saved_region,value.targets_sha256,e)&&quest_bridge::sha256(refs.rows.dump(),value.references_sha256,e);
}
bool supported(const a::NpcQuestListIntent& intent,const a::NpcQuestListState& value,std::string& e){
    if(value.records!=intent.before||intent.remove_slot< -1||value.records.size()>5||
       (intent.remove_slot>=0&&size_t(intent.remove_slot)>=value.records.size())||
       (intent.remove_slot==-1&&(value.records.size()>=5||value.count>=4096)))return fail(e,"Quest list stale selected current slot, full before list or count limit");
    if(quest_bridge::records_json(value.records).dump().size()*3+4096>a::max_payload)return fail(e,"Quest list receipt exceeds current transport before effect");
    if(intent.edit!=a::QuestListEdit::List){
        if(intent.remove_slot<0||size_t(intent.remove_slot)>=value.records.size())return fail(e,"Quest edit current slot absent");
        const auto& row=value.records[size_t(intent.remove_slot)];
        if((intent.edit==a::QuestListEdit::Title&&(intent.title.empty()||intent.title==row.name))||
           (intent.edit==a::QuestListEdit::Kills&&intent.kills==row.monsters_to_kill))return fail(e,"Quest edit original command has no effect");
        return true;
    }
    if(intent.remove_slot==-1){npc_quest_target_runtime::CurrentTargets targets;
        if(!npc_quest_target_runtime::read_current(intent.region,targets,e))return false;
        if(targets.building_keys.empty())return fail(e,"Quest Add requires a real original Building fallback destination before registration");}
    return true;
}
bool selector(Memory& m,uintptr_t window,uintptr_t npc,uintptr_t quest,uintptr_t& selected,std::string& e){
    uintptr_t list=0,ap=0,data=0,owner=0,proxy=0,weak=0;int32_t count=0,capacity=0;selected=0;
    if(!field(m,window,0x530,list,e)||!field(m,list,0,ap,e)||ap!=state.image+0x16e02b0||
       !field(m,list,0x360,owner,e)||owner!=npc||!field(m,list,0x368,proxy,e)||!field(m,proxy,0,weak,e)||weak!=npc+0x20||
       !field(m,list,0x340,data,e)||!field(m,list,0x348,count,e)||!field(m,list,0x34c,capacity,e)||count<1||count>6||capacity<count||capacity>100000||!data)
        return fail(e,e.empty()?"Quest list original owner/List bounds differ":e);
    for(int32_t i=0;i<count;++i){uintptr_t row=0,target=0;
        if(!field(m,data,size_t(i)*8,row,e)||!field(m,row,0,ap,e)||ap!=state.image+0x17088a0||!field(m,row,0x3e0,target,e))return fail(e,e.empty()?"Quest list original selector class differs":e);
        if(target!=quest)continue;
        if(selected||!field(m,row,0x320,owner,e)||owner!=list||!field(m,row,0x3e8,proxy,e)||!field(m,proxy,0,weak,e)||weak!=quest+0x18)
            return fail(e,e.empty()?"Quest list selected row duplicate or weak binding differs":e);
        selected=row;
    }
    return selected!=0||fail(e,"Quest list original selected row absent");
}
const char* edit_label(a::QuestListEdit edit){
    switch(edit){case a::QuestListEdit::Title:return "SetTitle";case a::QuestListEdit::Kills:return "SetKills";
    case a::QuestListEdit::Rewards:return "SetRewards";default:return "Destroy";}
}
bool edit_args(const json& args,a::NpcQuestListIntent& out,std::string& e)try{
    if(!args.is_object()||args.size()!=6||!args.contains("kind")||!args.at("kind").is_number_integer()||
       !args.contains("title")||!args.at("title").is_string()||!args.contains("kills")||!args.at("kills").is_number_integer()||
       !args.contains("xp")||!args.at("xp").is_number_integer()||!args.contains("gold")||!args.at("gold").is_number_integer()||
       !args.contains("defaults")||!args.at("defaults").is_boolean())return fail(e,"Quest editor requires exact kind/title/kills/xp/gold/defaults");
    const auto kind=args.at("kind").get<int64_t>(),kills=args.at("kills").get<int64_t>(),xp=args.at("xp").get<int64_t>(),gold=args.at("gold").get<int64_t>();
    if(kind<1||kind>3||kills<0||kills>1000000||xp<0||xp>INT32_MAX||gold<0||gold>INT32_MAX)return fail(e,"Quest editor values outside native integer domain");
    out.edit=a::QuestListEdit(kind);out.title=args.at("title").get<std::string>();out.kills=int32_t(kills);out.xp=int32_t(xp);out.gold=int32_t(gold);out.defaults=args.at("defaults").get<bool>();
    if(out.title.size()>1024||!npc_census::utf8(out.title)||out.title.find('\0')!=std::string::npos||
       (out.edit==a::QuestListEdit::Title&&(out.title.empty()||out.kills||out.xp||out.gold||out.defaults))||
       (out.edit==a::QuestListEdit::Kills&&(!out.title.empty()||out.xp||out.gold||out.defaults))||
       (out.edit==a::QuestListEdit::Rewards&&(!out.title.empty()||out.kills||(out.defaults&&(out.xp||out.gold)))))return fail(e,"Quest editor carries values outside its selected command");
    return true;
}catch(const std::exception& x){return fail(e,std::string("Quest editor arguments: ")+x.what());}
uint32_t external_value(uintptr_t slider,int32_t raw,float& value)noexcept{
    __try{value=reinterpret_cast<float(*)(void*,int32_t)>(state.image+0x7b95a0)(reinterpret_cast<void*>(slider),raw);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool read_edit(Memory& m,uintptr_t row,a::NpcQuestListIntent& intent,std::string& e){
    uintptr_t control=0;
    if(intent.edit==a::QuestListEdit::Title)return field(m,row,0x338,control,e)&&control&&
        npc_quest_records::read_string(m,control,0x2d0,"Quest title editor",intent.title,e);
    if(intent.edit==a::QuestListEdit::Kills){int32_t raw=0;float value=0;
        if(!field(m,row,0x3a8,control,e)||!control||!field(m,control,0x4d0,raw,e))return fail(e,e.empty()?"Quest kill-count Slider absent":e);
        const auto seh=external_value(control,raw,value);if(seh||!std::isfinite(value)||value<0||value>1000000)return fail(e,"Quest kill-count Slider conversion differs SEH="+std::to_string(seh));
        intent.kills=int32_t(value);return true;}
    if(intent.edit==a::QuestListEdit::Rewards){uint8_t checked=2;uintptr_t xp=0,gold=0;
        if(!field(m,row,0x378,control,e)||!control||!field(m,control,0x346,checked,e)||checked>1||
           !field(m,row,0x380,xp,e)||!xp||!field(m,row,0x388,gold,e)||!gold)return fail(e,e.empty()?"Quest reward controls absent":e);
        intent.defaults=checked!=0;if(intent.defaults)return true;
        return field(m,xp,0x44c,intent.xp,e)&&field(m,gold,0x44c,intent.gold,e)&&
            ((intent.xp>=0&&intent.gold>=0)||fail(e,"Quest custom rewards negative"));}
    return fail(e,"Quest editor capture has no native control binding");
}
// The pinned setter immediately copies data/length from its const GNU string
// argument. This borrowed layout never crosses an ownership or destructor call.
struct QuestTitleView {const char* data;uint64_t size,capacity=0,padding=0;};
uint32_t set_edit_controls(uintptr_t row,const a::NpcQuestListIntent* intent,uintptr_t first,uintptr_t second,uintptr_t third)noexcept{
    __try{
        if(intent->edit==a::QuestListEdit::Title){const QuestTitleView text{intent->title.c_str(),intent->title.size()};
            reinterpret_cast<void(*)(void*,const QuestTitleView&)>(state.image+0x7f1630)(reinterpret_cast<void*>(first),text);}
        else if(intent->edit==a::QuestListEdit::Kills)reinterpret_cast<void(*)(void*,float,bool)>(state.image+0x7bc040)(reinterpret_cast<void*>(first),float(intent->kills),false);
        else {reinterpret_cast<void(*)(void*,bool)>(state.image+0x6a1780)(reinterpret_cast<void*>(first),intent->defaults);
            if(!intent->defaults){reinterpret_cast<void(*)(void*,int32_t)>(state.image+0x7853a0)(reinterpret_cast<void*>(second),intent->xp);
                reinterpret_cast<void(*)(void*,int32_t)>(state.image+0x7853a0)(reinterpret_cast<void*>(third),intent->gold);}}
        (void)row;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool prepare_edit(Memory& m,uintptr_t row,const a::NpcQuestListIntent& intent,std::string& e){
    uintptr_t first=0,second=0,third=0,bound=0;
    const auto offset=intent.edit==a::QuestListEdit::Title?0x338:intent.edit==a::QuestListEdit::Kills?0x3a8:0x378;
    if(!field(m,row,offset,first,e)||!first)return fail(e,e.empty()?"Quest editor native control absent":e);
    if(intent.edit==a::QuestListEdit::Kills){
        if(!field(m,first,0x420,bound,e)||bound||!field(m,first,0x428,bound,e)||bound)return fail(e,e.empty()?"Quest kill-count Slider has a gameplay value binding":e);}
    if(intent.edit==a::QuestListEdit::Rewards){
        if(!field(m,first,0x3c0,bound,e)||bound||!field(m,first,0x3c8,bound,e)||bound||
           !field(m,row,0x380,second,e)||!second||!field(m,row,0x388,third,e)||!third)return fail(e,e.empty()?"Quest reward controls absent or have a gameplay binding":e);}
    const auto seh=set_edit_controls(row,&intent,first,second,third);if(seh)return fail(e,"Quest original UI preparation SEH="+std::to_string(seh));
    auto actual=intent;actual.title.clear();actual.kills=actual.xp=actual.gold=0;actual.defaults=false;
    if(!read_edit(m,row,actual,e)||actual!=intent)return fail(e,e.empty()?"Quest prepared native controls differ from ordered intent":e);
    return true;
}

bool prepare_command(Memory& m,const a::Binding& binding,const action_runtime::ui::LocalBinding& local,const qr::Witness& witness,const a::NpcQuestListIntent& intent,
                     a::ui_diagnostic::Api& api,a::ui_diagnostic::Window& window,std::string& e){
    const auto slot=intent.remove_slot;
    if(!a::ui_diagnostic::resolve_api(m,state.image,exe_sha,api,e))return false;
    json tool;if(!info_default_tool(m,tool,e))return false;
    a::ui_diagnostic::Context context{state.image,local.manager,local.object,local.uid,binding.native_epoch,binding.native_generation,exe_sha,a::ui_diagnostic::Family::Building};
    a::ui_diagnostic::Receipt receipt;json window_receipt;
    if(!a::ui_diagnostic::process_calls().select(api,context,receipt,e)||!action_runtime::live_npc_window(m,state.image,local,window,window_receipt,e))return false;
    if(slot<0){api.command=reinterpret_cast<Command>(state.image+guards::npc.rva);api.destroy_label="AddQuest";}
    else {uintptr_t row=0;const auto& owner=witness.owners.at(local.object);
        if(size_t(slot)>=owner.state.local.size()||!selector(m,window.window,local.object,owner.state.local[size_t(slot)].quest,row,e))return false;
        window.window=row;api.command=reinterpret_cast<Command>(state.image+guards::selector.rva);api.destroy_label=edit_label(intent.edit);if(intent.edit!=a::QuestListEdit::List&&!prepare_edit(m,row,intent,e))return false;}
    return true;
}
uint32_t label(a::ui_diagnostic::RecordEquals equals,void* record,const char* wanted,bool& result)noexcept{
    __try{result=equals(record,wanted);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool capture(bool remove,void* receiver,void* record)try{
    auto& hook=remove?selector_hook:npc_hook;
    if(!hook.original){fault=true;reject("Quest list original unavailable");return false;}
    if(!net::session_wire::connected())return hook.original(receiver,record);
    Memory m;std::string e;a::ui_diagnostic::Api api;bool relevant=false;
    if(!a::ui_diagnostic::resolve_api(m,state.image,exe_sha,api,e)){fault=true;reject(e);return false;}
    a::QuestListEdit editing=a::QuestListEdit::List;
    auto seh=label(api.label_equals,record,remove?"Destroy":"AddQuest",relevant);
    if(remove&&!relevant&&!seh)for(const auto kind:{a::QuestListEdit::Title,a::QuestListEdit::Kills,a::QuestListEdit::Rewards}){
        seh=label(api.label_equals,record,edit_label(kind),relevant);if(relevant){editing=kind;break;}if(seh)break;}
    if(seh){fault=true;reject("Quest list original command label SEH="+std::to_string(seh));return false;}
    if(!relevant)return hook.original(receiver,record);
    if(replaying){
        if(replay_receiver!=uintptr_t(receiver)||replay_remove!=remove||replay_edit!=editing||original_calls){fault=true;reject("Quest list original-once replay receiver or command differs");return false;}
        ++original_calls;return hook.original(receiver,record);
    }
    const auto controller=typed_action_runtime::controller_status();const auto snapshot=net::current_snapshot_id();a::Binding binding;
    if(!ready||fault||pending||qa::pending_ui()||character_rename::pending_ui()||ability_edit::pending_ui()||costume_edit::pending_ui()||dungeon_config::pending_ui()||dungeon_sound::pending_ui()||dungeon_gizmo::pending_ui()||dungeon_palette::pending_ui()||dungeon_light::pending_ui()||building_status::pending_ui()||GetCurrentThreadId()!=state.thread||!controller.value("started",false)||
       controller.value("locked",true)||controller.value("pending",true)||!inner_codes(m,e)||!verify_ui(remove?guards::selector.rva:guards::npc.rva,false,e)||
       !typed_action_runtime::network_binding(snapshot,binding,e)){reject(e.empty()?"Quest list capture requires ready idle Mainthread Controller":e);return false;}
    uintptr_t npc=0,quest=0,region=0;uint64_t uid=0;int32_t region_id=-1,slot=-1;std::string key;
    if(remove){if(!field(m,uintptr_t(receiver),0x3e0,quest,e)||!field(m,quest,0xb8,npc,e)||!field(m,quest,0x84,slot,e)){reject(e);return false;}}
    else if(!field(m,uintptr_t(receiver),0x550,npc,e)){reject(e);return false;}
    if(!field(m,npc,0x18,uid,e)||!action_runtime::local_key(npc,uid,key,e)||!field(m,npc,0x178,region,e)||!field(m,region,0x834,region_id,e)){reject(e);return false;}
    Pin p;action_runtime::ui::LocalBinding local;qr::Witness witness;References refs;d::Sample sample;a::NpcQuestListState value;
    if(!bind_owner(m,binding,snapshot,key,region_id,local,p,e)||local.object!=npc||!state_read(m,binding,snapshot,p,local,key,witness,refs,sample,value,e)||sample.undo.active_graph){reject(e.empty()?"Quest list captured owner or Undo differs":e);return false;}
    a::ui_diagnostic::Window window;json window_receipt;uintptr_t selected=0;
    if(!action_runtime::live_npc_window(m,state.image,local,window,window_receipt,e)||
       (!remove&&window.window!=uintptr_t(receiver))||
       (remove&&(slot<0||size_t(slot)>=witness.owners.at(npc).state.local.size()||witness.owners.at(npc).state.local[size_t(slot)].quest!=quest||
                 !selector(m,window.window,npc,quest,selected,e)||selected!=uintptr_t(receiver)))){reject(e.empty()?"Quest list live native menu/selected slot differs":e);return false;}
    a::NpcQuestListIntent intent{region_id,slot,value.records};intent.edit=editing;
    if(editing!=a::QuestListEdit::List&&!read_edit(m,uintptr_t(receiver),intent,e)){reject(e);return false;}
    if(editing!=a::QuestListEdit::List&&((editing==a::QuestListEdit::Title&&(intent.title.empty()||intent.title==value.records[size_t(slot)].name))||(editing==a::QuestListEdit::Kills&&intent.kills==value.records[size_t(slot)].monsters_to_kill)))return hook.original(receiver,record);
    if(!supported(intent,value,e)){reject(e);return false;}
    pending=Pending{binding,snapshot,key,ui_token,intent};++captures;
    if(!ui_token.empty())jobs.at(ui_token)["captured"]=true;
    return true;
}catch(const std::exception& x){fault=true;reject(std::string("Quest list capture boundary: ")+x.what());return false;}
 catch(...){fault=true;reject("Quest list capture non-standard exception");return false;}
bool npc_entry(void* receiver,void* record){return capture(false,receiver,record);}
bool selector_entry(void* receiver,void* record){return capture(true,receiver,record);}
bool inverse(Memory& m,const Operation& op,const d::Sample& after,std::string& e){
    if(after.undo.undo.empty())return fail(e,"Quest list original Undo absent");
    const auto graph=after.undo.undo.back();undo_probe::GraphWitness witness;uintptr_t data=0,inverse=0;
    return undo_probe::read_graph(m.process,state.image,graph,witness,e)&&undo_probe::validate_undo_commit(op.sample.undo,after.undo,witness,e)&&
        ((witness.operations==1&&!witness.nodes&&!witness.pending_delete)||fail(e,"Quest list original inverse graph differs"))&&
        field(m,graph,0x20,data,e)&&field(m,data,0,inverse,e)&&qr::inverse(m,state.image,inverse,op.before.owners.at(op.owner),e);
}
bool transition(Memory& m,const Operation& op,const qr::Witness& after,const References& refs,const d::Sample& sample,const a::NpcQuestListState& value,std::string& e){
    const auto& intent=op.request.npc_quest_list;
    auto protected_state=sample.state;protected_state.global_random=op.sample.state.global_random;
    if(refs.manager!=op.refs_before.manager||refs.data!=op.refs_before.data||refs.local!=op.refs_before.local||
       !npc_quest_list_read::references(op.refs_before.rows,refs.rows,op.request.canonical_record_key,intent.edit==a::QuestListEdit::List?intent.remove_slot:-1,e)||
       !a::npc_quest_list_transition(intent,op.evidence.quest_list_before,value)||ledger(sample.money)!=op.evidence.before||
       protected_state!=op.sample.state||sample.release_edits!=op.sample.release_edits||!inverse(m,op,sample,e))
        return fail(e,e.empty()?"Quest list native effect, ledger, Region state/RNG, release edits or inverse differs":e);
    if(intent.edit!=a::QuestListEdit::List)return npc_quest_list_read::edited(op.before,after,op.owner,intent.remove_slot,e);
    if(intent.remove_slot==-1)return npc_quest_list_read::added(op.before,after,op.owner,e);
    return qr::survivors(op.before,after,e)&&qr::absent(m,state.image,op.before,e);
}
}

bool verify_ui(uintptr_t rva,bool alignment,std::string& e){
    if(rva!=guards::npc.rva&&rva!=guards::selector.rva)return fail(e,"Quest list code guard has no exact named UI route");
    Memory m;const auto& code=rva==guards::npc.rva?guards::npc:alignment?guards::selector_with_alignment:guards::selector;
    return guards::verify(m,state.image,code,owned(rva),read_route,verify_code,quest_bridge::sha256,e);
}
bool verify_npc_command(uintptr_t command,std::string& e){Memory m;
    return guards::verify_npc_command(m,state.image,command,owned(guards::npc.rva),read_route,verify_code,quest_bridge::sha256,e);
}
bool pending_ui(){return pending.has_value()||!ui_token.empty()||replaying;}
bool retire_lobby(std::string& e){
    if(!ready){e.clear();return true;}
    if(GetCurrentThreadId()!=state.thread)return fail(e,"Quest list lobby retirement requires Mainthread");
    native::WorldStatus world;if(!native::read_world_status(world,e))return false;
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0)return fail(e,"Quest list lobby retirement requires actual idle Title");
    if(replaying)return fail(e,"Quest list lobby retirement cannot interrupt original execution");
    quest_history::clear();pending.reset();operations.clear();ui_token.clear();jobs.clear();replay_remove=false;replay_edit=a::QuestListEdit::List;replay_receiver=0;original_calls=0;
    fault=false;cause.clear();last_action.clear();e.clear();return true;
}
bool enable(std::string& e){
    if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(e,"Quest list requires bound Monster/NPC runtime Mainthread");
    if(ready){if(pending_ui()||fault)return fail(e,"Quest list retains unresolved capture");
        for(const auto& [id,op]:operations)if(!op.retired)return fail(e,"Quest list unretired action="+id);
        if(!verify_ui(guards::npc.rva,false,e)||!verify_ui(guards::selector.rva,true,e))return false;operations.clear();jobs.clear();cause.clear();return true;}
    if(installed)return fail(e,"Quest list earlier installation failed: "+cause);installed=true;
#if !defined(MT2MP_FRAMEWORK_PLUGIN)
    return fail(e,"Quest list normal capture requires exact Framework named routes");
#else
    Memory m;if(!inner_codes(m,e)||!verify_ui(guards::npc.rva,false,e)||!verify_ui(guards::selector.rva,true,e))return false;
    const auto install=[&](uintptr_t rva,void* detour,Hook& hook){
        const auto entry=reinterpret_cast<void*>(state.image+rva);auto code=MH_CreateHook(entry,detour,reinterpret_cast<void**>(&hook.original));
        if(code!=MH_OK||!hook.original)return fail(e,"Quest list prepare failed status="+std::to_string(code)+" cause="+mt2mp::hooks::framework::process_backend().error());
        MT2FW_MT2_RouteReceipt receipt{sizeof receipt};if(!read_route(uintptr_t(entry),receipt,e))return false;hook.generation=receipt.generation;
        code=MH_EnableHook(entry);if(code!=MH_OK)return fail(e,"Quest list enable failed status="+std::to_string(code)+" cause="+mt2mp::hooks::framework::process_backend().error());
        return verify_ui(rva,false,e);
    };
    if(!install(guards::npc.rva,reinterpret_cast<void*>(&npc_entry),npc_hook)||!install(guards::selector.rva,reinterpret_cast<void*>(&selector_entry),selector_hook)){
        cause=e;fault=true;return false;}ready=true;return true;
#endif
}
void tick(){
    const auto controller=typed_action_runtime::controller_status();
    for(auto& [token,job]:jobs){(void)token;if(job.value("submitted",false)&&!job.value("terminal",false)&&
       controller.value("action_uuid",std::string{})==job.value("action_uuid",std::string{})&&!controller.value("pending",true)){
        job["terminal"]=true;job["outcome"]=controller.value("outcome",0);job["confirmed"]=controller.value("outcome",0)==int(a::Outcome::Confirmed);
        job["unknown"]=controller.value("outcome",0)==int(a::Outcome::Unknown);job["cause"]=controller.value("cause",std::string{});}}
    if(!pending)return;auto saved=*pending;pending.reset();std::string e;a::Binding binding;
    const auto failed=[&](){fault=true;reject(e);if(!saved.token.empty()){auto& job=jobs.at(saved.token);job["terminal"]=true;job["unknown"]=true;job["cause"]=e;}};
    if(!ready||fault||saved.snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(saved.snapshot,binding,e)||binding!=saved.binding){if(e.empty())e="Quest list captured World or Mainthread changed";failed();return;}
    const auto id=action_runtime::uuid(e);if(id.empty()||!typed_action_runtime::submit_npc_quest_list(id,saved.owner,saved.intent,e)){failed();return;}
    ++submitted;last_action=id;if(!saved.token.empty()){jobs.at(saved.token)["submitted"]=true;jobs.at(saved.token)["action_uuid"]=id;}
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& e)try{
    if(!ready||fault||GetCurrentThreadId()!=state.thread||(request.action!=a::Action::SetNpcQuestList&&request.action!=a::Action::EditNpcQuest)){e="Quest list runtime/action unavailable";return a::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){e="Quest list already attempted; no replay";return a::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("started",false)||controller.value("locked",true)||!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){e="Quest list requires exact pending Controller";return a::Outcome::RejectedNoEffect;}
    Memory m;Pin p;action_runtime::ui::LocalBinding local;qr::Witness before;References refs;d::Sample sample;a::NpcQuestListState value;
    const auto& intent=request.npc_quest_list;
    if(!inner_codes(m,e)||!verify_ui(guards::npc.rva,false,e)||!verify_ui(guards::selector.rva,true,e)||!bind_owner(m,binding,request.snapshot_uuid,request.canonical_record_key,intent.region,local,p,e)||
       !state_read(m,binding,request.snapshot_uuid,p,local,request.canonical_record_key,before,refs,sample,value,e)||!supported(intent,value,e))return a::Outcome::RejectedNoEffect;
    if(sample.undo.active_graph){e="Quest list execution requires idle original Undo graph";return a::Outcome::RejectedNoEffect;}
    if(authority){auto local_before=value;local_before.global_random=authority->quest_list_before.global_random;
        if(local_before!=authority->quest_list_before||ledger(sample.money)!=authority->before){e="Quest list Client full before state differs from Host";return a::Outcome::RejectedNoEffect;}}
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;op.pin=p;op.owner=local.object;
    const auto unknown=[&](const std::string& why){replaying=false;replay_receiver=0;e=op.cause=why.empty()?"Quest list failed without cause":why;return a::Outcome::Unknown;};
    if(authority){op.seed_attempted=true;const auto seh=action_runtime::seed(state.image,authority->quest_list_before.global_random);if(seh)return unknown("Quest list Host RNG adoption SEH="+std::to_string(seh));
        const auto region_random=sample.state.region_random;
        if(!state_read(m,binding,request.snapshot_uuid,p,local,request.canonical_record_key,before,refs,sample,value,e)||value!=authority->quest_list_before||sample.state.region_random!=region_random)return unknown(e.empty()?"Quest list Host RNG readback differs":e);}
    op.before=before;op.refs_before=refs;op.sample=sample;op.evidence.before=ledger(sample.money);op.evidence.quest_list_before=value;
    if(intent.edit==a::QuestListEdit::List&&intent.remove_slot>=0)op.before.removed={before.owners.at(local.object).state.local[size_t(intent.remove_slot)].quest};
    a::ui_diagnostic::Api api;a::ui_diagnostic::Window window;
    if(!prepare_command(m,binding,local,before,intent,api,window,e))return unknown(e);
    qr::Witness prepared;References prepared_refs;d::Sample prepared_sample;a::NpcQuestListState prepared_value;
    if(!state_read(m,binding,request.snapshot_uuid,p,local,request.canonical_record_key,prepared,prepared_refs,prepared_sample,prepared_value,e,&before)||prepared_value!=value||
       !qr::unchanged(before,prepared,e)||prepared_refs.local!=refs.local||!no_gameplay_change(sample,prepared_sample))return unknown(e.empty()?"Quest list original menu preparation changed gameplay":e);
    a::ui_diagnostic::Receipt receipt;op.attempted=true;replaying=true;replay_receiver=window.window;replay_remove=intent.remove_slot>=0;replay_edit=intent.edit;original_calls=0;
    const bool returned=a::ui_diagnostic::process_calls().command(api,window,receipt,e);op.calls=original_calls;replaying=false;replay_receiver=0;op.returned=receipt.ui_command_returned;
    if(!returned||!op.returned||op.calls!=1||!receipt.record_cleanup_confirmed)return unknown(e.empty()?"Quest list original menu transaction lacks exact once/cleanup receipt":e);
    Pin after_pin;action_runtime::ui::LocalBinding after_local;d::Sample after;a::NpcQuestListState final;
    if(!bind_owner(m,binding,request.snapshot_uuid,request.canonical_record_key,intent.region,after_local,after_pin,e)||after_local.object!=op.owner||!same_world(after_pin.world,p.world)||
       !state_read(m,binding,request.snapshot_uuid,after_pin,after_local,request.canonical_record_key,op.after,op.refs_after,after,final,e,&before)||
       !transition(m,op,op.after,op.refs_after,after,final,e))return unknown(e);
    op.graph=after.undo.undo.back();op.evidence.after=ledger(after.money);op.evidence.quest_list_after=final;op.evidence.undo_confirmed=true;
    if(authority&&op.evidence!=*authority){auto expected=*authority;expected.retirement_confirmed=false;if(op.evidence!=expected)return unknown("Quest list Client original result differs from Host");}
    op.confirmed=true;return a::Outcome::Confirmed;
}catch(const std::exception& x){replaying=false;replay_receiver=0;e=std::string("Quest list perform boundary: ")+x.what();if(auto it=operations.find(request.action_uuid);it!=operations.end())it->second.cause=e;return a::Outcome::Unknown;}
 catch(...){replaying=false;replay_receiver=0;e="Quest list perform non-standard exception";return a::Outcome::Unknown;}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& e){
    const auto found=operations.find(request.action_uuid);
    if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.returned||found->second.calls!=1)
        return fail(e,"Quest list observation lacks exact confirmed original-once receipt");
    const auto& op=found->second;Memory m;Pin p;action_runtime::ui::LocalBinding local;qr::Witness now;References refs;d::Sample sample;a::NpcQuestListState value;
    if(!bind_owner(m,binding,request.snapshot_uuid,request.canonical_record_key,request.npc_quest_list.region,local,p,e)||local.object!=op.owner||!same_world(p.world,op.pin.world)||
       !state_read(m,binding,request.snapshot_uuid,p,local,request.canonical_record_key,now,refs,sample,value,e,&op.before)||value!=op.evidence.quest_list_after||
       sample.undo.undo.empty()||sample.undo.undo.back()!=op.graph||!transition(m,op,now,refs,sample,value,e))return fail(e,e.empty()?"Quest list current receipt differs before ACK":e);
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const a::Binding& binding,const a::Request& request,std::string& e){a::Evidence out;if(!observe(binding,request,out,e))return false;
    auto& op=operations.at(request.action_uuid);if(op.retired)return fail(e,"Quest list receipt already retired");
    if(!quest_history::remember(binding,request,op.owner,op.graph,op.before,op.after,e))return false;
    op.retired=true;return true;
}
json status(){json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"attempted",op.attempted},{"returned",op.returned},{"original_calls",op.calls},
    {"confirmed",op.confirmed},{"retired",op.retired},{"seed_attempted",op.seed_attempted},{"owner_key",op.request.canonical_record_key},{"remove_slot",op.request.npc_quest_list.remove_slot},
    {"global_before",op.before.quests.size()},{"global_after",op.after.quests.size()},{"completed_before",op.before.completed},{"completed_after",op.after.completed},
    {"owner_before",op.evidence.quest_list_before.records.size()},{"owner_after",op.evidence.quest_list_after.records.size()},
    {"references_before_sha256",op.evidence.quest_list_before.references_sha256},{"references_after_sha256",op.evidence.quest_list_after.references_sha256},{"cause",op.cause}};
    return {{"enabled",ready},{"fault",fault},{"pending_ui",pending_ui()},{"captures",captures},{"submitted",submitted},{"rejected",rejected},
        {"last_action",last_action},{"cause",cause},{"operations",rows},{"ui_jobs",jobs}};
}
bool native_ui(const json& args,json& result,std::string& e)try{
    result=json::object();
    if(!args.is_object()||args.size()!=(args.contains("edit")?5u:4u)||!args.contains("token")||!args.at("token").is_string()||!args.contains("owner_key")||!args.at("owner_key").is_string()||
       !args.contains("region_id")||!args.at("region_id").is_number_integer()||!args.contains("remove_slot")||!args.at("remove_slot").is_number_integer())return fail(e,"Quest list UI requires exact token, owner_key, region_id and remove_slot");
    const auto token=args.at("token").get<std::string>(),key=args.at("owner_key").get<std::string>();const auto region=args.at("region_id").get<int64_t>(),slot=args.at("remove_slot").get<int64_t>();
    if(!token_valid(token)||!npc_target_key::valid(key)||region<0||region>INT32_MAX||slot< -1||slot>4||jobs.contains(token)||jobs.size()>=64)return fail(e,"Quest list UI token, owner or current slot outside domain/used");
    const auto controller=typed_action_runtime::controller_status();a::Binding binding;const auto snapshot=net::current_snapshot_id();Memory m;Pin p;action_runtime::ui::LocalBinding local;
    if(!ready||fault||pending_ui()||qa::pending_ui()||character_rename::pending_ui()||ability_edit::pending_ui()||costume_edit::pending_ui()||dungeon_config::pending_ui()||dungeon_sound::pending_ui()||dungeon_gizmo::pending_ui()||dungeon_palette::pending_ui()||dungeon_light::pending_ui()||building_status::pending_ui()||GetCurrentThreadId()!=state.thread||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||
       !typed_action_runtime::network_binding(snapshot,binding,e)||!bind_owner(m,binding,snapshot,key,int32_t(region),local,p,e))return fail(e,e.empty()?"Quest list UI requires idle bound Mainthread":e);
    qr::Witness before;References refs;d::Sample sample;a::NpcQuestListState value;
    if(!state_read(m,binding,snapshot,p,local,key,before,refs,sample,value,e)||sample.undo.active_graph)return fail(e,e.empty()?"Quest list UI requires idle Undo":e);
    a::NpcQuestListIntent intent{int32_t(region),int32_t(slot),value.records};
    if(args.contains("edit")&&!edit_args(args.at("edit"),intent,e))return false;
    if(!supported(intent,value,e))return false;
    auto& job=jobs.emplace(token,json{{"token",token},{"owner_key",key},{"region_id",region},{"remove_slot",slot},{"attempted",false},{"returned",false},
        {"captured",false},{"submitted",false},{"terminal",false},{"confirmed",false},{"unknown",false}}).first->second;
    const auto unknown=[&](const std::string& why){ui_token.clear();fault=true;e=why.empty()?"Quest list UI failed without cause":why;job["cause"]=e;job["unknown"]=true;job["terminal"]=true;result=job;return false;};
    job["attempted"]=true;a::ui_diagnostic::Api api;a::ui_diagnostic::Window window;
    if(!prepare_command(m,binding,local,before,intent,api,window,e))return unknown(e);
    qr::Witness prepared;References prepared_refs;d::Sample prepared_sample;a::NpcQuestListState prepared_value;
    if(!state_read(m,binding,snapshot,p,local,key,prepared,prepared_refs,prepared_sample,prepared_value,e,&before)||prepared_value!=value||
       !qr::unchanged(before,prepared,e)||prepared_refs.local!=refs.local||!no_gameplay_change(sample,prepared_sample))return unknown(e.empty()?"Quest list UI preparation changed gameplay":e);
    a::ui_diagnostic::Receipt receipt;const auto count=captures;ui_token=token;
    const bool returned=a::ui_diagnostic::process_calls().command(api,window,receipt,e);ui_token.clear();job["returned"]=receipt.ui_command_returned;
    job["record_cleanup_confirmed"]=receipt.record_cleanup_confirmed;
    if(!returned||captures!=count+1||!pending||pending->token!=token||pending->owner!=key||pending->intent!=intent)return unknown(e.empty()?"Quest list original UI command lacks exact deferred capture: "+cause:e);
    qr::Witness after;References after_refs;d::Sample after_sample;a::NpcQuestListState after_value;
    if(!state_read(m,binding,snapshot,p,local,key,after,after_refs,after_sample,after_value,e,&before)||after_value!=value||
       !qr::unchanged(before,after,e)||after_refs.local!=refs.local||!no_gameplay_change(sample,after_sample))return unknown(e.empty()?"Quest list captured UI command changed gameplay before Host order":e);
    result=job;return true;
}catch(const std::exception& x){ui_token.clear();fault=true;e=std::string("Quest list UI boundary: ")+x.what();reject(e);return false;}
 catch(...){ui_token.clear();fault=true;e="Quest list UI non-standard exception";reject(e);return false;}
}
