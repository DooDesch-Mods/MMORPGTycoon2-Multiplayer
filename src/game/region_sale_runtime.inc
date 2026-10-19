namespace sale_native=build_actions::private_native;
uint32_t sale_price(uintptr_t image,int32_t& price){
    __try{price=reinterpret_cast<int32_t(__fastcall*)()>(image+0x5954f0)();return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool sale_inverse(Runtime& r,const LocalView& view,const action::RegionPropertyState& before,
                  const sale_native::UndoWitness& old,const sale_native::UndoWitness& current,int32_t refund,
                  const std::array<float,3>& hit,std::string& error){
    if(current.undo.empty())return fail(error,"Region sale original Undo graph absent");
    sale_native::GraphWitness graph;const auto pointer=current.undo.back();uintptr_t data=0,op=0,ap=0,region=0;
    int32_t type=-1;std::array<int32_t,2> levels{};uint8_t owned=0;std::array<float,4> tint{};
    if(!sale_native::read_graph(r.memory,r.image,pointer,graph,error)||!sale_native::validate_undo_commit(old,current,graph,error))return false;
    if(graph.operations!=(refund?2u:1u)||!at(r,pointer,0x20,data,error)||!at(r,data,0,op,error)||
       !at(r,op,0,ap,error)||!at(r,op,0xb8,region,error)||!at(r,op,0xc0,type,error)||
       !at(r,op,0xc4,levels,error)||!at(r,op,0x160,owned,error)||!at(r,op,0x150,tint,error))
        return fail(error,error.empty()?"Region sale Undo exact operation count/fields differ":error);
    if(ap!=r.image+0x1723230||region!=view.region||type!=before.type||
       levels!=std::array<int32_t,2>{before.level,before.max_level}||owned!=1||!before.appearance||tint!=before.appearance->tint)
        return fail(error,"Region sale inverse ConfigureRegion owner/type/levels/owned/tint differs");
    for(size_t i=0;i<4;++i){std::string style;if(!text(r,op+0xd0+i*0x20,style,error))return false;
        if(style!=before.content.styles[i])return fail(error,"Region sale inverse style differs index="+std::to_string(i));}
    if(refund){int32_t amount=0;std::array<float,3> position{};
        if(!at(r,data,8,op,error)||!at(r,op,0,ap,error)||!at(r,op,0xb8,amount,error)||!at(r,op,0xbc,position,error))return false;
        if(ap!=r.image+0x1786fb0||amount!=-refund||position!=hit)return fail(error,"Region sale inverse Cash amount/position differs");}
    return true;
}
bool sale_members(const LocalView& before,const LocalView& after,std::string& error){
    return (before.building_uids==after.building_uids&&before.scenery_uids==after.scenery_uids&&
        before.path_uids==after.path_uids&&before.path_node_uids==after.path_node_uids&&before.network_uids==after.network_uids)||
        fail(error,"Region sale changed existing Building/Scenery/Path/Network membership");
}
