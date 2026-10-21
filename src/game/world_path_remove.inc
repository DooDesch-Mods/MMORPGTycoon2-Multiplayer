namespace world_path_runtime {
uint32_t remove_quote(uintptr_t image,uintptr_t object,int32_t& price,action::PathPoint& position){
    __try{price=reinterpret_cast<int32_t(*)(void*)>(image+0x43c680)(reinterpret_cast<void*>(object));
        reinterpret_cast<void*(*)(void*,void*)>(image+0x43c470)(position.data(),reinterpret_cast<void*>(object));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool remove_inverse(region::Runtime& r,const Sample& before,const std::set<std::string>& retired,
                    const std::string& target,uintptr_t graph,const undo::GraphWitness& witness,
                    int32_t price,const action::PathPoint& position,std::string& error){
    std::map<uint64_t,bool> expected;
    for(const auto& row:before.mixed.graph.paths)if(retired.contains(row.key)){
        if(row.key!=target)return fail(error,"mixed Path Remove inverse contains another Path");
        expected.emplace(row.local_uid,true);
    }
    for(const auto& row:before.mixed.graph.nodes)if(retired.contains(row.key))expected.emplace(row.local_uid,false);
    if(expected.size()!=retired.size()||witness.operations!=expected.size()+size_t(price!=0))
        return fail(error,"mixed Path Remove inverse complete operation count differs");
    uintptr_t data=0;if(!region::at(r,graph,0x20,data,error))return false;
    std::set<uint64_t> made;unsigned cash=0;
    for(size_t i=0;i<witness.operations;++i){uintptr_t operation=0,ap=0;
        if(!region::at(r,data,i*8,operation,error)||!region::at(r,operation,0,ap,error))return false;
        if(ap==r.image+0x16e49c0||ap==r.image+0x172bf40){uint64_t uid=0;
            if(!region::at(r,operation,0xb8,uid,error))return false;
            if(!expected.contains(uid)||!made.insert(uid).second||expected.at(uid)!=(ap==r.image+0x16e49c0))
                return fail(error,"mixed Path Remove inverse full UID or original Make type differs");
        }else if(ap==r.image+0x1786fb0){int32_t amount=0;action::PathPoint point{};
            if(!region::at(r,operation,0xb8,amount,error)||!region::at(r,operation,0xbc,point,error))return false;
            if(int64_t(amount)!=-int64_t(price)||point!=position)
                return fail(error,"mixed Path Remove inverse original cash amount/position differs");
            ++cash;
        }else return fail(error,"mixed Path Remove inverse contains foreign operation AP="+std::to_string(ap));
    }
    return made.size()==expected.size()&&cash==unsigned(price!=0)||
        fail(error,"mixed Path Remove inverse incomplete full UID or cash set");
}
bool capture_remove(region::Runtime& r,uintptr_t window,bool& handled,std::string& error){
    handled=false;uintptr_t object=0,proxy=0,live=0,region=0,second=0;int32_t type=-1,id=-1,end=-1;
    if(!region::at(r,window,0x4f0,object,error)||!object||
       !region::at(r,object,0x21c,type,error))return false;
    if(!idle(r,error)||!region::at(r,window,0x4f8,proxy,error)||!proxy||
       !region::at(r,proxy,0,live,error)||live!=object+0x20||
       !region::at(r,object,0x178,region,error)||!region||
       !region::at(r,object,0x2f8,second,error)||!region::at(r,region,0x834,id,error))
        return fail(error,error.empty()?"mixed Path Remove current InfoWindow/weak owner differs":error);
    end=id;if(second&&!region::at(r,second,0x834,end,error))return false;
    Sample before;undo::UndoWitness history;region::LocalView view;
    const auto snapshot=net::current_snapshot_id();
    if(!world_path_sample(r,snapshot,id,end,before,error)||
       !region::identity(r,id,region,view,error)||!undo::read_undo(r.memory,r.image,history,error))return false;
    if(history.active_graph)return fail(error,"mixed Path Remove capture requires idle original Undo");
    const auto found=std::find_if(before.mixed.native.paths.begin(),before.mixed.native.paths.end(),
        [&](const auto& row){return row.member.object==object;});
    if(found==before.mixed.native.paths.end()||found->member.ref.type!=type||
       found->member.ref.region!=id||found->owner!=region||found->second!=second||
       !owner.keys.keys().contains(found->member.uid))
        return fail(error,"mixed Path Remove selected full identity is outside current reciprocal World graph");
    if(type==path.road){
        const bool mixed=std::any_of(before.mixed.native.paths.begin(),before.mixed.native.paths.end(),
            [&](const auto& other){return other.member.ref.type!=path.road&&
                ((found->start&&(other.start==found->start||other.end==found->start))||
                 (found->end&&(other.start==found->end||other.end==found->end)));});
        if(!mixed)return true;
    }
    handled=true;
    action::WorldPathIntent intent;intent.kind=action::WorldPathKind::Remove;
    intent.start_region=id;intent.end_region=end;intent.type=type;
    intent.target_path=owner.keys.keys().at(found->member.uid);
    intent.graph_before_sha256=before.wire.complete_sha256;
    if(!action::world_path_valid(intent))return fail(error,"mixed Path Remove captured intent invalid");
    capture.pending=Pending{std::move(intent),{view.world.origin.world_epoch,view.world.origin.generation},snapshot};
    ++capture.accepted;error.clear();return true;
}
uint32_t replay_remove(uintptr_t image,uintptr_t object,bool& returned){
    const region::NativeString command{"Destroy",7,{}},edit{"Destroy path",12,{}};
    __try{
        reinterpret_cast<void(*)(const void*)>(image+0x265300)(&edit);
        returned=reinterpret_cast<bool(*)(void*,const void*)>(image+0x43a2e0)(reinterpret_cast<void*>(object),&command);
        reinterpret_cast<void(*)()>(image+0x265650)();return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool remove_target(const Sample& before,const action::WorldPathIntent& intent,uintptr_t& target,std::string& error){
    target=0;
    for(const auto& row:before.mixed.native.paths){
        const auto key=owner.keys.keys().find(row.member.uid);
        if(key==owner.keys.keys().end()||key->second!=intent.target_path)continue;
        int32_t second=row.member.ref.region;
        if(row.second){const auto region=std::find_if(before.mixed.native.regions.begin(),before.mixed.native.regions.end(),
            [&](const auto& value){return value.object==row.second;});
            if(region==before.mixed.native.regions.end())return fail(error,"mixed Path Remove second Region absent");
            second=region->id;
        }
        if(row.member.ref.region!=intent.start_region||second!=intent.end_region||row.member.ref.type!=intent.type)
            return fail(error,"mixed Path Remove target owner/type differs from captured intent");
        target=row.member.object;return true;
    }
    return fail(error,"mixed Path Remove canonical target absent");
}
bool remove_live(region::Runtime& r,const Sample& before,std::string& error){
    for(const auto& row:before.mixed.native.paths){uint8_t queued=0;
        if(!region::at(r,row.member.object,0x476,queued,error))return false;
        if(queued)return fail(error,"mixed Path Remove before graph contains an already retiring Path");
    }
    return true;
}
bool remove_selection(region::Runtime& r,uintptr_t object,std::string& error){
    uintptr_t selection=0,focused=0;
    if(!region::at(r,r.image,0x13df630,selection,error)||
       !native::ap(r.memory,selection,r.image,0x17205d0,error)||
       !region::at(r,selection,0x80,focused,error))return false;
    if(focused==object)return fail(error,"mixed Path Remove target remains focused");
    for(size_t offset:{size_t(8),size_t(0x20),size_t(0x38),size_t(0x50)}){
        std::vector<uintptr_t> members;
        if(!native::array(r.memory,selection,offset+8,offset+16,offset+20,1000000,members,error))return false;
        if(std::find(members.begin(),members.end(),object)!=members.end())return fail(error,"mixed Path Remove target remains selected");
    }
    return true;
}
}
