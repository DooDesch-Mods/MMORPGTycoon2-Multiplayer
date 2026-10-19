bool ownership_read(Runtime& r,int32_t id,uintptr_t expected,const std::array<int32_t,2>& range,
                    action::RegionPropertyState& out,LocalView& view,uintptr_t& global,std::string& e){
    action::RegionLevelState actors;action::RegionAppearance appearance;
    if(!property_read(r,id,expected,out,view,e,true)||!level_state(r,view,range,actors,global,e)||
       !appearance_read_native(r,view.region,appearance,e))return false;
    out.actors=std::move(actors);out.appearance=std::move(appearance);return true;
}

bool ownership_inverse(Runtime& r,uintptr_t pointer,uintptr_t expected_region,
                       const action::RegionPropertyState& destination,int32_t cash,
                       const std::array<float,3>& hit,std::string& e){
    sale_native::GraphWitness graph;uintptr_t data=0,op=0,ap=0,region=0;
    int32_t type=-1;std::array<int32_t,2> range{};uint8_t owned=0;std::array<float,4> tint{};
    if(!destination.appearance||!sale_native::read_graph(r.memory,r.image,pointer,graph,e))return false;
    if(!graph.locked||graph.pending_delete||graph.operations!=(cash?2u:1u)||
       !at(r,pointer,0x20,data,e)||!at(r,data,0,op,e)||!at(r,op,0,ap,e)||
       !at(r,op,0xb8,region,e)||!at(r,op,0xc0,type,e)||!at(r,op,0xc4,range,e)||
       !at(r,op,0x160,owned,e)||!at(r,op,0x150,tint,e))
        return fail(e,e.empty()?"Region ownership inverse graph shape differs":e);
    if(ap!=r.image+0x1723230||region!=expected_region||type!=destination.type||
       range!=std::array<int32_t,2>{destination.level,destination.max_level}||
       owned!=uint8_t(destination.owned)||tint!=destination.appearance->tint)
        return fail(e,"Region ownership inverse ConfigureRegion owner/type/range/owned/tint differs");
    for(size_t i=0;i<4;++i){std::string style;if(!text(r,op+0xd0+i*0x20,style,e))return false;
        if(style!=destination.content.styles[i])return fail(e,"Region ownership inverse style differs index="+std::to_string(i));}
    if(cash){int32_t amount=0;std::array<float,3> position{};
        if(!at(r,data,8,op,e)||!at(r,op,0,ap,e)||!at(r,op,0xb8,amount,e)||!at(r,op,0xbc,position,e))return false;
        if(ap!=r.image+0x1786fb0||amount!=cash||position!=hit)
            return fail(e,"Region ownership inverse signed Cash amount/position differs");}
    return true;
}
