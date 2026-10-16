namespace {
namespace quest_state=subscriber_quest_state;
using QuestOwner=npc_probe::action_runtime::StateOwner;
class QuestMemory final:public npc_census::Memory {
public:
    bool copy(uintptr_t address,void* out,size_t size,std::string& e)override{return memory.copy(address,out,size,e);}
};
bool quest_npcs(const Pin& p,const std::vector<QuestOwner>& owners,
                std::vector<quest_state::Npc>& out,std::string& e){
    QuestMemory read;out.clear();
    for(const auto& owner:owners){
        quest_state::Npc row;row.key=owner.key;uintptr_t region=0;
        if(!ap(owner.npc,p.image,0x1742980,e)||!at(owner.npc,0x178,region,e)||
           !at(region,0x834,row.pose.region,e)||!at(owner.npc,0x6f8,row.level,e)||
           !at(owner.npc,0x80c,row.type,e))return false;
        if(!memory.copy(owner.npc+0x100,row.pose.orientation.data(),sizeof row.pose.orientation,e)||
           !memory.copy(owner.npc+0x110,row.pose.position.data(),sizeof row.pose.position,e)||
           !memory.copy(owner.npc+0x11c,row.pose.scale.data(),sizeof row.pose.scale,e))return false;
        if(row.type==0){
            if(!npc_quest_records::read_daily(read,p.image,owner.npc,0x898,"NPC accepted",row.accepted,e)||
               !npc_quest_records::read_daily(read,p.image,owner.npc,0x908,"NPC completed",row.completed,e)||
               !at(owner.npc,0x8f8,row.highest_cache[0],e)||!at(owner.npc,0x968,row.highest_cache[1],e))return false;
            for(const auto& [sequence,quest]:owner.quests){
                quest_state::Counters count;count.sequence_id=sequence;
                if(!npc_quest_records::read_daily(read,p.image,quest,0x100,"accepted",count.accepted,e)||
                   !npc_quest_records::read_daily(read,p.image,quest,0x170,"completed",count.completed,e)||
                   !at(quest,0x160,count.highest_cache[0],e)||!at(quest,0x1d0,count.highest_cache[1],e))return false;
                row.quests.push_back(std::move(count));
            }
        }else if(!owner.quests.empty())return fail(e,"NPC state non-QuestGiver owns Quest rows");
        out.push_back(std::move(row));
    }
    return true;
}
bool quest_toon(const Pin& p,const std::vector<QuestOwner>& owners,uintptr_t subscriber,
                std::optional<quest_state::Toon>& out,std::string& e){
    apply_timing::PageReadScope pages(apply_timing::page_read_scope==nullptr);
    out.reset();uintptr_t toon=0;if(!at(subscriber,0x80,toon,e))return false;
    if(!toon)return true;
    std::map<uintptr_t,quest_state::Link> links;
    for(const auto& owner:owners){
        links.emplace(owner.npc,quest_state::Link{quest_state::LinkState::Live,owner.key,-1});
        for(const auto& [sequence,quest]:owner.quests)
            links.emplace(quest,quest_state::Link{quest_state::LinkState::Live,owner.key,sequence});
    }
    std::map<std::string,quest_state::Link> observed_links;
    npc_quest_visitor_reader::State observed;QuestMemory read;
    const auto identify=[&](npc_quest_visitor_reader::Kind kind,uintptr_t target,std::string& key,std::string& error){
        const auto found=links.find(target);
        if(found==links.end()||(kind==npc_quest_visitor_reader::Kind::Npc)!=(found->second.sequence_id==-1))
            return fail(error,"Quest progress target lacks current canonical NPC/Quest binding");
        const auto& link=found->second;
        key=link.npc_key+(link.sequence_id<0?"":"/quest/"+std::to_string(link.sequence_id));
        observed_links.emplace(key,link);return true;
    };
    if(!npc_quest_visitor_reader::read(read,p.image,subscriber,identify,observed,e))return false;
    quest_state::Toon next;next.gold=observed.gold;next.xp_bits=std::bit_cast<uint32_t>(observed.xp);
    next.level=observed.level;next.quests_completed=observed.quests_completed;
    constexpr size_t attributes[]={0x750,0x778,0x7a0};
    for(size_t i=0;i<3;++i)if(!at(toon,attributes[i],next.attributes[i],e))return false;
    if(!at(toon,0x878,next.seconds_to_level_bits,e)||!read_toon_weapon(toon,next,e))return false;
    const auto translate=[&](const npc_quest_visitor_reader::Link& source,quest_state::Link& target){
        using ReadState=npc_quest_visitor_reader::LinkState;
        if(source.state==ReadState::Empty){target.state=quest_state::LinkState::Empty;return true;}
        if(source.state==ReadState::Expired){target.state=quest_state::LinkState::Expired;return true;}
        // The reader only publishes keys returned by identify in this read pass.
        const auto found=observed_links.find(source.diagnostic_key);
        if(found==observed_links.end())return fail(e,"Quest reader returned an unobserved identity key");
        target=found->second;return true;
    };
    for(const auto& progress:observed.progress){
        quest_state::Progress row;if(!translate(progress.npc,row.npc)||!translate(progress.quest,row.quest))return false;
        row.completed=progress.completed;row.monsters_defeated=progress.monsters_defeated;
        row.visited=progress.visited;row.action_performed=progress.action_performed;row.complete=progress.complete;
        next.progress.push_back(std::move(row));
    }
    out=std::move(next);return true;
}
bool quest_restore_pause(const Pin& p,std::string& e){
    action_session::Context session;building_mixed_identity::opened_file::CurrentWorld world;
    if(GetCurrentThreadId()!=p.thread)return fail(e,"Quest pause restore requires bound Mainthread");
    if(!action_session::selected_session().current(session,e))return false;
    if(session!=p.context)return fail(e,"Quest pause restore current session differs");
    if(!native_open_runtime::read_current_world(world,e))return false;
    if(uintptr_t(world.world)!=p.world||world.world_epoch!=p.binding.native_epoch||world.generation!=p.binding.native_generation||
       net::current_world_epoch()!=p.common_epoch)return fail(e,"Quest pause restore current World binding changed");
    native::WorldStatus status;if(!native::read_world_status(status,e))return false;
    if(status.saves_in_progress||status.edit_operations)return fail(e,"Quest pause restore requires closed Save/Edit scope actual_saves="+
        std::to_string(status.saves_in_progress)+" actual_edits="+std::to_string(status.edit_operations));
    uintptr_t clock=0;float pause=0,selected=0;
    if(!at(p.image,0x13df9a0,clock,e)||clock!=p.clock||!ap(clock,p.image,0x17499d0,e)||
       !at(clock,0x58,selected,e)||!at(clock,0x5c,pause,e))return fail(e,e.empty()?"Quest pause restore actual Clock owner differs":e);
    if(selected!=p.selected)return fail(e,"Quest pause restore selected speed changed");
    if(pause==p.pause)return true;
    if(p.pause!=1.0f||pause!=0.0f)return fail(e,"Quest pause restore unexpected transition actual="+std::to_string(pause)+" required="+std::to_string(p.pause));
    // The native SaveScope clears pause; Mode normally restores it next frame.
    constexpr std::array<unsigned char,10> expected{0x56,0x53,0x48,0x83,0xec,0x48,0x0f,0x2f,0x49,0x5c};
    std::array<unsigned char,10> code{};
    if(!memory.copy(p.image+0x259b70,code.data(),code.size(),e))return false;
    if(code!=expected)return fail(e,"Quest pause restore original setter bytes differ");
    const auto exception=call_capture_pause(p.image+0x259b70,clock,p.pause);
    if(exception)return fail(e,"Quest pause restore original setter exception="+std::to_string(exception)+"; no replay");
    if(!at(clock,0x58,selected,e)||!at(clock,0x5c,pause,e)||selected!=p.selected||pause!=p.pause)
        return fail(e,e.empty()?"Quest pause restore native Clock readback differs; no replay":e);
    return true;
}
uint32_t quest_daily_reserve(uintptr_t address,uintptr_t array,int32_t capacity){
    __try{reinterpret_cast<void(*)(uintptr_t,int32_t)>(address)(array,capacity);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct QuestDailyWork {
    std::function<bool(std::string&)> body,guard;
};
bool quest_daily_body(void* user,std::string& e){return static_cast<QuestDailyWork*>(user)->body(e);}
bool quest_daily_guard(void* user,std::string& e){return static_cast<QuestDailyWork*>(user)->guard(e);}
bool store_npc_counters(const Pin& p,const QuestOwner& owner,const quest_state::Npc& old,
                        const quest_state::Npc& target,const std::function<bool(std::string&)>& fence,std::string& e){
    if(old.type!=0)return true;
    bool changed=old.accepted!=target.accepted||old.completed!=target.completed||old.highest_cache!=target.highest_cache;
    for(size_t i=0;i<old.quests.size();++i)if(old.quests[i]!=target.quests[i])changed=true;
    if(!changed)return true;
    building_mixed_identity::opened_file::CurrentWorld world;
    if(!native_open_runtime::read_current_world(world,e))return false;
    const auto guard=[&](std::string& error){
        if(!fence(error))return false;
        std::vector<QuestOwner> current;
        if(!npc_probe::action_runtime::state_owners(p.context,p.binding,current,error))return false;
        const auto found=std::find_if(current.begin(),current.end(),[&](const auto& item){return item.key==owner.key;});
        return (found!=current.end()&&found->npc==owner.npc&&found->quests==owner.quests)||
            fail(error,"DailyCount current canonical NPC/Quest owners changed");
    };
    const auto compare=[&](uintptr_t address,const void* expected,const void* desired,size_t size,std::string& error){
        if(!size||size%4||address>UINTPTR_MAX-size)return fail(error,"DailyCount checked span differs");
        for(size_t i=0;i<size;i+=4){int32_t a=0,b=0;
            std::memcpy(&a,static_cast<const unsigned char*>(expected)+i,4);
            std::memcpy(&b,static_cast<const unsigned char*>(desired)+i,4);
            if(!compare_fixed_i32(address+i,a,b,error))return false;
        }
        return true;
    };
    const auto reserve=[&](uintptr_t array,int32_t capacity,std::string& error){
        constexpr std::array<unsigned char,16> expected{0x57,0x56,0x53,0x48,0x83,0xec,0x20,0x48,0xb8,0xfe,0xff,0xff,0xff,0xff,0xff,0x1f};
        std::array<unsigned char,16> bytes{};
        if(!memory.copy(p.image+0xe8080,bytes.data(),bytes.size(),error))return false;
        if(bytes!=expected)return fail(error,"DailyCount original Reserve entry differs");
        const auto exception=quest_daily_reserve(p.image+0xe8080,array,capacity);
        return !exception||fail(error,"DailyCount original Reserve exception="+std::to_string(exception)+"; no replay");
    };
    const auto body=[&](std::string& error){
        int32_t day=0;if(!at(p.clock,0x44,day,error))return false;
        const auto apply=[&](uintptr_t object,size_t offset,const npc_quest_records::DailyCount& a,
                             const npc_quest_records::DailyCount& b,int32_t highest_before,int32_t highest){
            if(a==b&&highest_before==highest)return true;
            npc_quest_daily_apply::Receipt receipt;
            return npc_quest_daily_apply::apply_daily(memory,p.image,object,offset,a,b,highest,day,
                compare,reserve,guard,receipt,error);
        };
        if(!apply(owner.npc,0x898,old.accepted,target.accepted,old.highest_cache[0],target.highest_cache[0])||
           !apply(owner.npc,0x908,old.completed,target.completed,old.highest_cache[1],target.highest_cache[1]))return false;
        for(size_t i=0;i<old.quests.size();++i){
            const auto& a=old.quests[i];const auto& b=target.quests[i];uintptr_t quest=0;
            for(const auto& [sequence,pointer]:owner.quests)if(sequence==a.sequence_id)quest=pointer;
            if(!quest)return fail(error,"DailyCount canonical Quest sequence missing");
            if(!apply(quest,0x100,a.accepted,b.accepted,a.highest_cache[0],b.highest_cache[0])||
               !apply(quest,0x170,a.completed,b.completed,a.highest_cache[1],b.highest_cache[1]))return false;
        }
        return true;
    };
    QuestDailyWork work{body,guard};mt2mp::subscriber_lifecycle::SaveScopeReceipt receipt;
    const mt2mp::subscriber_lifecycle::SaveScopeApi api{
        reinterpret_cast<mt2mp::subscriber_lifecycle::SaveScopeCall>(p.image+0x4a03a0),
        reinterpret_cast<mt2mp::subscriber_lifecycle::SaveScopeCall>(p.image+0x49f6f0)};
    if(!mt2mp::subscriber_lifecycle::with_save_scope(api,world.mode,quest_daily_body,quest_daily_guard,&work,receipt,e))return false;
    return quest_restore_pause(p,e);
}
struct QuestFresh {
    const Pin* pin=nullptr;
    uintptr_t subscriber=0,toon=0,progress=0;
    int32_t id=-1;
    bool instance_member=false;
    bool absent_npc=false;
    bool clear_quest=false;
};
bool quest_fresh_link(void* user,const mt2mp::subscriber_lifecycle::ExternalLink& link,std::string& e){
    const auto& check=*static_cast<QuestFresh*>(user);const auto& p=*check.pin;
    action_session::Context session;
    if(!action_session::selected_session().current(session,e)||session!=p.context||p.thread!=GetCurrentThreadId())
        return fail(e,e.empty()?"Quest apply current session/Mainthread differs":e);
    building_mixed_identity::opened_file::CurrentWorld world;
    if(!native_open_runtime::read_current_world(world,e))return false;
    if(uintptr_t(world.world)!=p.world||world.world_epoch!=p.binding.native_epoch||world.generation!=p.binding.native_generation||
       net::current_world_epoch()!=p.common_epoch)return fail(e,"Quest apply current World/epoch/generation differs");
    uintptr_t manager=0,data=0,toon=0,backlink=0;int32_t count=-1,capacity=-1,id=-1;
    if(!at(p.image,0x13df690,manager,e)||!ap(manager,p.image,0x1725f40,e)||
       !at(manager,0x40,data,e)||!at(manager,0x48,count,e)||!at(manager,0x4c,capacity,e)||
       count<0||capacity<count||count>1000000||(count&&!data))return fail(e,e.empty()?"Quest apply Subscriber census differs":e);
    size_t matches=0;
    for(int32_t i=0;i<count;++i){uintptr_t pointer=0;if(!at(data,size_t(i)*8,pointer,e))return false;if(pointer==check.subscriber)++matches;}
    if(matches!=1||!ap(check.subscriber,p.image,0x16e75f0,e)||!at(check.subscriber,0x20,id,e)||id!=check.id||
       !at(check.subscriber,0x80,toon,e)||toon!=check.toon||!ap(toon,p.image,0x1744e90,e)||
       !at(toon,0x940,backlink,e)||backlink!=check.subscriber)return fail(e,e.empty()?"Quest apply reciprocal Subscriber/Toon ownership differs":e);
    bool found=link.portable_key.empty()&&!link.object;
    if(!found){
        std::vector<QuestOwner> owners;
        if(!npc_probe::action_runtime::state_owners(p.context,p.binding,owners,e))return false;
        for(const auto& owner:owners){
            if(owner.key==link.portable_key&&(owner.npc==link.object||(check.clear_quest&&!link.object)))found=true;
            for(const auto& [sequence,quest]:owner.quests)
                if(owner.key+"/quest/"+std::to_string(sequence)==link.portable_key&&quest==link.object)found=true;
        }
    }
    if(!found&&!(check.absent_npc&&!link.object&&link.portable_key=="retired-progress"))
        return fail(e,"Quest apply canonical NPC/Quest target is no longer bound");
    if(check.instance_member){
        npc_quest_visitor_reader::ArrayHeader header;QuestMemory read;
        if(!npc_quest_visitor_reader::read_header(read,toon,header,e)||header.ap!=p.image+0x16ee0e0||
           header.count<0||header.capacity<header.count||header.count>100000||(header.count&&!header.data))
            return fail(e,e.empty()?"Quest apply Instance array differs":e);
        size_t instances=0;
        for(int32_t i=0;i<header.count;++i){uintptr_t pointer=0;if(!at(header.data,size_t(i)*8,pointer,e))return false;if(pointer==check.progress)++instances;}
        if(instances!=1||!ap(check.progress,p.image,0x172d130,e))return fail(e,"Quest apply Instance owner is not a current Progress member");
    }
    e.clear();return true;
}
}
bool NativePort::store_rewards(const Owners& before,size_t index,size_t field,uint32_t old,uint32_t target,std::string& e){
    constexpr size_t offsets[]={0x54,0x5c,0x60};
    if(index>=before.subscribers.size()||field>=3)return fail(e,"Subscriber reward CAS slot invalid");
    if(!current_subscriber(before,index,e))return false;
    return compare_fixed_i32(before.subscribers[index]+offsets[field],std::bit_cast<int32_t>(old),std::bit_cast<int32_t>(target),e);
}
bool NativePort::store_quest_state(const Owners& before,size_t index,
    const std::optional<quest_state::Toon>& old,const std::optional<quest_state::Toon>& target,std::string& e){
    if(index>=before.subscribers.size()||scalar_before_)return fail(e,"Quest apply owner slot invalid or scalar phase active");
    if(!current_subscriber(before,index,e))return false;
    const auto& p=before.history.pin;const auto subscriber=before.subscribers[index];
    std::vector<QuestOwner> owners;std::optional<quest_state::Toon> actual;
    if(!npc_probe::action_runtime::state_owners(p.context,p.binding,owners,e)||!quest_toon(p,owners,subscriber,actual,e))return false;
    const auto refresh_weapon=[&](){
        if(!target)return true;
        uintptr_t receiver=0;if(!at(subscriber,0x80,receiver,e))return false;
        toon_weapon_visual::Receipt receipt;
        if(!toon_weapon_visual::refresh(receiver,receipt,e))return false;
        return (receipt.owned&&receipt.confirmed)||fail(e,"Quest weapon visual did not confirm current Client");
    };
    if(actual==target)return refresh_weapon();
    if(!actual||!target)return fail(e,"Quest apply requires actual Toon lifecycle transition before state stores");
    if(old&&actual!=old)return fail(e,"Quest apply prior Toon state changed");
    uintptr_t toon=0;if(!at(subscriber,0x80,toon,e))return false;
    QuestFresh fresh{&p,subscriber,toon,0,before.state.subscribers[index].serialized_id,false,false,false};
    const auto weapon_fence=[&](std::string& error){return current_subscriber(before,index,error)&&quest_fresh_link(&fresh,{},error);};
    if(!store_toon_weapon(p,toon,*actual,*target,weapon_fence,e)||!refresh_weapon())return false;
    // A freshly captured equal Progress list needs no row stores or native callbacks.
    // Scalar ownership, CAS and the complete final read remain below this branch.
    if(actual->progress!=target->progress){
    // Identical links need no native reassignment. This path stays callback-free
    // until the final pause restoration, retaining the actual read and scalar CAS guards.
    const bool same_links=actual->progress.size()==target->progress.size()&&
        std::equal(actual->progress.begin(),actual->progress.end(),target->progress.begin(),
            [](const auto& a,const auto& b){return a.npc==b.npc&&a.quest==b.quest;});
    building_mixed_identity::opened_file::CurrentWorld world;
    if(!native_open_runtime::read_current_world(world,e))return false;
    const subscriber_lifecycle_append::FrameBinding frame{p.context,p.binding,p.common_epoch,world.sha256};
    const auto npc_owner=[&](const std::string& key)->const QuestOwner*{
        const auto found=std::find_if(owners.begin(),owners.end(),[&](const auto& owner){return owner.key==key;});
        return found==owners.end()?nullptr:&*found;
    };
    const auto pointers=[&](std::vector<uintptr_t>& out){
        QuestMemory read;npc_quest_visitor_reader::ArrayHeader header;
        if(!npc_quest_visitor_reader::read_header(read,toon,header,e)||header.ap!=p.image+0x16ee0e0||
           header.count<0||header.count>100000||header.capacity<header.count||(header.count&&!header.data))
            return fail(e,e.empty()?"Quest apply Progress array header differs":e);
        out.resize(size_t(header.count));
        for(size_t i=0;i<out.size();++i)if(!at(header.data,i*8,out[i],e)||!ap(out[i],p.image,0x172d130,e))return false;
        return true;
    };
    std::vector<uintptr_t> rows;if(!pointers(rows)||rows.size()!=actual->progress.size())return fail(e,e.empty()?"Quest apply Progress row shape changed":e);
    // Preserve retained rows and remove only rows absent from the authoritative state.
    for(size_t i=actual->progress.size();i-->0;){
        const auto& current=actual->progress[i];bool keep=false;
        for(const auto& wanted:target->progress)
            if(current.npc.state==quest_state::LinkState::Live&&wanted.npc==current.npc)keep=true;
        if(current.npc.state!=quest_state::LinkState::Live&&i<target->progress.size()&&
           current.npc==target->progress[i].npc&&current.quest==target->progress[i].quest)keep=true;
        if(keep)continue;
        const auto* owner=npc_owner(current.npc.npc_key);
        fresh.absent_npc=!owner;fresh.instance_member=false;
        subscriber_lifecycle_append::ProgressRequest request{toon,owner?owner->npc:0,
            owner?owner->key:"retired-progress",quest_fresh_link,&fresh};
        subscriber_lifecycle_append::ProgressReceipt receipt;
        if(!subscriber_lifecycle_append::retire_progress(request,rows[i],world.uuid,frame,receipt,e))return false;
    }
    for(size_t i=0;i<target->progress.size();++i){
        const auto& wanted=target->progress[i];uintptr_t progress=0;
        const auto* owner=npc_owner(wanted.npc.npc_key);
        fresh.absent_npc=false;fresh.instance_member=false;fresh.clear_quest=false;
        if(wanted.npc.state==quest_state::LinkState::Live&&!same_links){
            if(!owner)return fail(e,"Quest apply requested NPC key is absent");
            const subscriber_lifecycle_append::ProgressRequest request{toon,owner->npc,owner->key,quest_fresh_link,&fresh};
            subscriber_lifecycle_append::ProgressReceipt receipt;
            if(!subscriber_lifecycle_append::ensure_progress(request,world.uuid,frame,receipt,e))return false;
            progress=receipt.progress;
        }
        if(!pointers(rows)||i>=rows.size()||(progress&&rows[i]!=progress))
            return fail(e,e.empty()?"Quest apply ordered Progress membership differs":e);
        progress=rows[i];fresh.progress=progress;fresh.instance_member=true;
        if(wanted.quest.state==quest_state::LinkState::Live){
            const auto* quest_owner=npc_owner(wanted.quest.npc_key);
            if(!quest_owner)return fail(e,"Quest apply requested Quest owner is absent");
            uintptr_t quest=0;
            for(const auto& [sequence,pointer]:quest_owner->quests)if(sequence==wanted.quest.sequence_id)quest=pointer;
            if(!quest)return fail(e,"Quest apply requested Quest sequence is absent");
            if(!same_links){
                const subscriber_lifecycle_append::QuestLinkRequest request{progress+0x58,quest,
                    quest_owner->key+"/quest/"+std::to_string(wanted.quest.sequence_id),quest_fresh_link,&fresh};
                subscriber_lifecycle_append::QuestLinkReceipt receipt;
                if(!subscriber_lifecycle_append::bind_quest_instance(request,world.uuid,frame,receipt,e))return false;
            }
        }else if(wanted.quest.state==quest_state::LinkState::Empty&&owner){
            if(!same_links){
                fresh.clear_quest=true;
                const subscriber_lifecycle_append::QuestLinkRequest request{progress+0x58,0,owner->key,quest_fresh_link,&fresh};
                subscriber_lifecycle_append::QuestLinkReceipt receipt;
                if(!subscriber_lifecycle_append::bind_quest_instance(request,world.uuid,frame,receipt,e))return false;
                fresh.clear_quest=false;
            }
        }else{
            // Expired links belong to the original lifetime, not a recreated pointer.
            if(!quest_toon(p,owners,subscriber,actual,e)||!actual||i>=actual->progress.size()||actual->progress[i].quest!=wanted.quest)
                return fail(e,e.empty()?"Quest apply empty/expired link requires matching native lifetime":e);
        }
        const auto store=[&](size_t offset,int32_t value){int32_t prior=0;return at(progress,offset,prior,e)&&
            (prior==value||compare_fixed_i32(progress+offset,prior,value,e));};
        fresh.instance_member=true;
        if(!quest_fresh_link(&fresh,{},e))return false;
        if(!store(0x50,wanted.completed)||!store(0xa8,wanted.monsters_defeated))return false;
        uint32_t flags=0;if(!at(progress,0xac,flags,e))return false;
        const uint32_t required=(flags&0xff000000u)|(wanted.visited?1u:0u)|(wanted.action_performed?0x100u:0u)|(wanted.complete?0x10000u:0u);
        if(flags!=required&&!compare_fixed_i32(progress+0xac,std::bit_cast<int32_t>(flags),std::bit_cast<int32_t>(required),e))return false;
    }
    }
    constexpr size_t offsets[]={0x850,0x820,0x6f8,0x874,0x750,0x778,0x7a0,0x878};
    const std::array<uint32_t,8> values{std::bit_cast<uint32_t>(target->gold),target->xp_bits,
        std::bit_cast<uint32_t>(target->level),std::bit_cast<uint32_t>(target->quests_completed),
        target->attributes[0],target->attributes[1],target->attributes[2],target->seconds_to_level_bits};
    fresh.instance_member=false;if(!quest_fresh_link(&fresh,{},e))return false;
    for(size_t i=0;i<values.size();++i){uint32_t prior=0;if(!at(toon,offsets[i],prior,e))return false;
        if(prior!=values[i]&&!compare_fixed_i32(toon+offsets[i],std::bit_cast<int32_t>(prior),std::bit_cast<int32_t>(values[i]),e))return false;
    }
    if(!quest_restore_pause(p,e))return false;
    if(!quest_toon(p,owners,subscriber,actual,e)||actual!=target)return fail(e,e.empty()?"Quest apply complete Toon state readback differs":e);
    return true;
}
namespace {
void npc_transform(uintptr_t image,uintptr_t npc,const subscriber_motion::Pose& target){
    alignas(4) std::array<std::byte,0x6c> transform{};
    const auto construct=reinterpret_cast<void(*)(void*,const float*,const float*,const float*)>(image+0x9ff370);
    const auto set=reinterpret_cast<void(*)(uintptr_t,const void*)>(image+0xc67c80);
    construct(transform.data(),target.orientation.data(),target.position.data(),target.scale.data());
    set(npc+0x30,transform.data());
}
}
bool NativePort::store_npc_state(const Owners& before,size_t index,const quest_state::Npc& old,
                                 const quest_state::Npc& target,std::string& e){
    if(scalar_before_)return fail(e,"NPC state owner/key/type differs or scalar phase active: scalar phase active");
    if(index>=before.state.npcs.size())return fail(e,"NPC state owner/key/type differs or scalar phase active: index="+std::to_string(index)+" before.count="+std::to_string(before.state.npcs.size()));
    if(old.key!=target.key)return fail(e,"NPC state owner/key/type differs or scalar phase active: "+difference("npc.key",old.key,target.key));
    if(old.type!=target.type)return fail(e,"NPC state owner/key/type differs or scalar phase active: "+difference("npc["+target.key+"].type",old.type,target.type));
    if(!current(before,0,true,e))return false;
    const auto& p=before.history.pin;std::vector<QuestOwner> owners;std::vector<quest_state::Npc> actual;
    if(!npc_probe::action_runtime::state_owners(p.context,p.binding,owners,e)||!quest_npcs(p,owners,actual,e))return false;
    if(index>=actual.size()||index>=owners.size())return fail(e,"NPC state prior canonical owner/value changed: index="+std::to_string(index)+" actual.count="+std::to_string(actual.size())+" owners.count="+std::to_string(owners.size()));
    if(owners[index].key!=old.key)return fail(e,"NPC state prior canonical owner/value changed: "+difference("npc.owner.key",owners[index].key,old.key));
    if(!same_npc_non_pose(actual[index],old))return fail(e,"NPC state prior canonical owner/value changed: "+npc_non_pose_difference(actual[index],old));
    const auto npc=owners[index].npc;
    if(!store_npc_counters(p,owners[index],old,target,[&](std::string& error){return fence_.read_current_fence(p,error);},e))return false;
    if(old.level!=target.level&&!compare_fixed_i32(npc+0x6f8,old.level,target.level,e))return false;
    if(!npc_probe::action_runtime::state_owners(p.context,p.binding,owners,e)||!quest_npcs(p,owners,actual,e))return false;
    if(index>=actual.size())return fail(e,"NPC pose/counters full readback differs; no replay: index="+std::to_string(index)+" actual.count="+std::to_string(actual.size()));
    if(!same_npc_non_pose(actual[index],target))return fail(e,"NPC pose/counters full readback differs; no replay: "+npc_non_pose_difference(actual[index],target));
    return true;
}
bool NativePort::store_npc_pose(const Owners& before,size_t index,const quest_state::Npc& old,
                                 const quest_state::Npc& target,std::string& e){
    if(scalar_before_)return fail(e,"NPC state owner/key/type differs or scalar phase active: scalar phase active");
    if(index>=before.state.npcs.size())return fail(e,"NPC state owner/key/type differs or scalar phase active: index="+std::to_string(index)+" before.count="+std::to_string(before.state.npcs.size()));
    if(old.key!=target.key)return fail(e,"NPC state owner/key/type differs or scalar phase active: "+difference("npc.key",old.key,target.key));
    if(old.type!=target.type)return fail(e,"NPC state owner/key/type differs or scalar phase active: "+difference("npc["+target.key+"].type",old.type,target.type));
    if(!current(before,0,true,e))return false;
    const auto& p=before.history.pin;std::vector<QuestOwner> owners;std::vector<quest_state::Npc> actual;
    if(!npc_probe::action_runtime::state_owners(p.context,p.binding,owners,e)||!quest_npcs(p,owners,actual,e))return false;
    if(index>=actual.size()||index>=owners.size())return fail(e,"NPC state prior canonical owner/value changed: index="+std::to_string(index)+" actual.count="+std::to_string(actual.size())+" owners.count="+std::to_string(owners.size()));
    if(owners[index].key!=old.key)return fail(e,"NPC state prior canonical owner/value changed: "+difference("npc.owner.key",owners[index].key,old.key));
    if(actual[index]!=old)return fail(e,"NPC state prior canonical owner/value changed: "+npc_difference({actual[index]},{old}));
    const auto npc=owners[index].npc;
    if(!same_npc_non_pose(old,target))return fail(e,"NPC final pose requires settled exact non-pose state: "+npc_non_pose_difference(old,target));
    if(old.pose!=target.pose){
        uintptr_t render=0,callback=0,region=0;uint8_t follows=1;
        std::array<uint32_t,3> spawn{},spawn_after{};
        if(old.pose.region!=target.pose.region)return fail(e,"NPC simulation transform fixed Region/callback differs: "+difference("npc["+old.key+"].pose.region",old.pose.region,target.pose.region));
        if(!at(npc,0x30,render,e))return false;
        if(render!=p.image+0x1742e58)return fail(e,"NPC simulation transform fixed Region/callback differs: render class differs");
        if(!at(render,0x38,callback,e))return false;
        if(callback!=p.image+0x3c2de0)return fail(e,"NPC simulation transform fixed Region/callback differs: callback differs");
        if(!at(npc,0x178,region,e)||!at(npc,0x490,follows,e))return false;
        if(follows)return fail(e,"NPC simulation transform fixed Region/callback differs: "+difference("npc["+old.key+"].follows_region",follows,uint8_t(0)));
        if(!memory.copy(npc+0x818,spawn.data(),sizeof spawn,e))return false;
        npc_transform(p.image,npc,target.pose);
        uintptr_t after_region=0;uint8_t after_follows=1;
        if(!at(npc,0x178,after_region,e))return false;
        if(after_region!=region)return fail(e,"NPC simulation transform changed spawn point or owner Region; no replay: Region owner changed");
        if(!at(npc,0x490,after_follows,e))return false;
        if(after_follows!=follows)return fail(e,"NPC simulation transform changed spawn point or owner Region; no replay: "+difference("npc["+old.key+"].follows_region",after_follows,follows));
        if(!memory.copy(npc+0x818,spawn_after.data(),sizeof spawn_after,e))return false;
        if(spawn!=spawn_after)return fail(e,"NPC simulation transform changed spawn point or owner Region; no replay: "+difference("npc["+old.key+"].spawn_bits",spawn_after,spawn));
    }
    if(!npc_probe::action_runtime::state_owners(p.context,p.binding,owners,e)||!quest_npcs(p,owners,actual,e))return false;
    if(index>=actual.size())return fail(e,"NPC pose/counters full readback differs; no replay: index="+std::to_string(index)+" actual.count="+std::to_string(actual.size()));
    if(actual[index]!=target)return fail(e,"NPC pose/counters full readback differs; no replay: "+npc_difference({actual[index]},{target}));
    return true;
}
