namespace zone_edit_graph_fixture {
namespace graph58=mt2mp::game::dungeon_cell_graph;
namespace stored58=mt2mp::game::zone_edit_stored;
namespace poly58=mt2mp::game::ui_terrain_runtime::poly;
inline Memory graph_memory58(const std::vector<size_t>& kinds){
    auto memory=fixture_kinds(kinds);
    for(size_t index=0;index<kinds.size();++index)if(graph58::descriptors[kinds[index]].kind==graph58::Kind::Cash){
        const auto node=uintptr_t(0x10000+index*0x1000);memory.put(node+0xb8,int32_t(-17));
        memory.put(node+0xbc,std::array<uint32_t,3>{0x80000000,0,0x3f800000});
        for(size_t relation=0;relation<3;++relation)uid_set(memory,node+8+relation*0x28,0x100000+0x5000*(index*3+relation),0);
    }
    return memory;
}
inline bool read_graph58(Memory& memory,graph58::Graph& result,std::string& cause,graph58::Policy policy){
    auto at=[&](uintptr_t pointer,size_t offset,auto& value,std::string& error){return memory.read(pointer+offset,&value,sizeof value,error);};
    auto copy=[&](uintptr_t pointer,void* destination,size_t count,std::string& error){return memory.read(pointer,destination,count,error);};
    auto values=[](graph58::Node& node,std::string&){node.values.push_back("complete-zone-owned-value");return true;};
    auto owner=[](const graph58::Node&,uintptr_t pointer,std::string& error){
        if(pointer!=fixture_region){error="Zone graph cached Region differs";return false;}return true;};
    return graph58::read(memory,at,copy,values,owner,image,graph,result,cause,policy);
}
inline void put_stored58(Memory& memory,bool edit){
    const auto& descriptor=graph58::descriptors[edit?5:4];
    memory.put(uintptr_t(0x10000),image+descriptor.ap);memory.put(image+descriptor.ap+0x10,image+descriptor.method);
    memory.put(uintptr_t(0x100b8),uint64_t(0x700000123));memory.put(uintptr_t(0x100c0),int32_t(1));
    memory.put(uintptr_t(0x100c4),int32_t(3));memory.put(uintptr_t(0x10138),uintptr_t(0x800000));
    memory.put(uintptr_t(0x10140),fixture_region);memory.put(uintptr_t(0x10148),int32_t(17));memory.put(uintptr_t(0x1014c),int32_t(9));
    memory.put(uintptr_t(0x100e0),uintptr_t(0x500000));
    memory.put(uintptr_t(0x500000),poly58::VectorView{0x501000,0x501030,0x501030});
    memory.put(uintptr_t(0x501000),poly58::VectorView{0x502000,0x502030,0x502030});
    memory.put(uintptr_t(0x501018),poly58::VectorView{0x503000,0x503030,0x503030});
    const std::array<mt2mp::action_session::TerrainPoint,3> outside{{{-100,20},{30,40},{50,-60}}},hole{{{1,2},{3,4},{5,6}}};
    memory.put(uintptr_t(0x502000),outside);memory.put(uintptr_t(0x503000),hole);
}
struct StoredMemory58 {
    Memory& source58;int mutation58=0;unsigned reads58=0;
    bool copy(uintptr_t pointer,void* destination,size_t count,std::string& error){
        if(pointer==0x10000&&count==sizeof(uintptr_t)){
            ++reads58;
            if(reads58==2){
                if(mutation58==1)source58.put(uintptr_t(0x100b8),uint64_t(0x800000123));
                if(mutation58==2)source58.put(uintptr_t(0x503000),mt2mp::action_session::TerrainPoint{7,8});
                if(mutation58==3){source58.put(uintptr_t(0x10000),image+graph58::descriptors[5].ap);
                    source58.put(image+graph58::descriptors[5].ap+0x10,image+graph58::descriptors[5].method);}
            }
        }
        return source58.read(pointer,destination,count,error);
    }
};
inline unsigned run(){
    unsigned comparisons58=0;std::string cause58;
    const auto require58=[&](bool okay,const char* cause){++comparisons58;if(!okay)throw std::runtime_error(std::string(cause)+": "+cause58);};
    const auto policy58=graph58::Policy{graph58::Shape::Zones,0};
    for(size_t kind58:{4,5,3,8,9}){auto memory=graph_memory58({kind58});graph58::Graph result;
        require58(read_graph58(memory,result,cause58,policy58),"Zone graph rejected an original allowed non-Cash family");
        require58(result.nodes.size()==1&&result.nodes[0].dependencies[1]==std::vector<uint64_t>{0x200000002},"Zone graph omitted full UID dependencies");
    }
    {auto memory=graph_memory58({4,5,3,8,9,10});graph58::Graph result;
        require58(read_graph58(memory,result,cause58,{graph58::Shape::Zones,1}),"Zone graph rejected complete mixed Zone inverse");
        require58(result.nodes.size()==6&&result.execution==std::vector<uintptr_t>{0x15000,0x14000,0x13000,0x12000,0x11000,0x10000},"Zone graph changed original node or sorted execution order");
        require58(result.nodes.back().values==std::vector<std::string>{"-17","2147483648","0","1065353216"},"Zone graph Cash original signed amount/float bits differ");
    }
    for(size_t forbidden58:{0,1,2,6,7}){auto memory=graph_memory58({4,forbidden58});graph58::Graph result;
        require58(!read_graph58(memory,result,cause58,policy58)&&!cause58.empty(),"Zone graph admitted unrelated original node family");
    }
    for(const auto& kinds58:std::vector<std::vector<size_t>>{{10},{4,10},{4,10,10}}){auto memory=graph_memory58(kinds58);graph58::Graph result;
        const uint32_t expected_cash58=kinds58.size()==2?0u:1u;
        require58(!read_graph58(memory,result,cause58,{graph58::Shape::Zones,expected_cash58}),"Zone graph admitted Cash-only or wrong Cash count");
    }
    {auto memory=graph_memory58({4});graph58::Graph result;memory.put(uintptr_t(0x3000),uintptr_t(0xdead));
        require58(!read_graph58(memory,result,cause58,policy58),"Zone graph ignored native execution membership");}
    for(bool edit58:{false,true}){Memory memory;put_stored58(memory,edit58);StoredMemory58 port{memory};stored58::Stored value;
        require58(stored58::read(port,image,0x10000,value,cause58),"Original Make/Edit stored values rejected");
        require58(value.uid==0x700000123&&value.root==0x800000&&value.region==fixture_region&&value.type==1&&
            value.density==3&&value.monster_type==17&&value.level==9,"Stored full UID and all scalar owner values differ");
        require58(value.shape==mt2mp::action_session::TerrainShape{{{-100,20},{30,40},{50,-60}},{{1,2},{3,4},{5,6}}}&&port.reads58==2,
            "Stored reader lost deep shape order/holes or complete repeat read");
    }
    for(int32_t type58=0;type58<4;++type58){Memory memory;put_stored58(memory,false);memory.put(uintptr_t(0x100c0),type58);StoredMemory58 port{memory};stored58::Stored value;
        require58(stored58::read(port,image,0x10000,value,cause58)&&value.type==type58,"Stored reader rejected original active Zone type");}
    {Memory memory;put_stored58(memory,false);memory.put(uintptr_t(0x500000),poly58::VectorView{});StoredMemory58 port{memory};stored58::Stored value;
        require58(stored58::read(port,image,0x10000,value,cause58)&&value.shape.empty(),"Stored reader invented a nonempty shape requirement");}
    for(int invalid58=0;invalid58<8;++invalid58){Memory memory;put_stored58(memory,false);
        if(invalid58==0)memory.put(uintptr_t(0x10000),image+0x16e4a08);
        if(invalid58==1)memory.put(image+0x16e4a00+0x10,image+0x55bd51);
        if(invalid58==2)memory.put(uintptr_t(0x100b8),uint64_t(0));
        if(invalid58==3)memory.put(uintptr_t(0x10138),uintptr_t(0));
        if(invalid58==4)memory.put(uintptr_t(0x10140),uintptr_t(0));
        if(invalid58==5)memory.put(uintptr_t(0x100c0),int32_t(4));
        if(invalid58==6)memory.put(uintptr_t(0x501000),poly58::VectorView{0x502000,0x501ff0,0x502030});
        if(invalid58==7)memory.bytes.erase(0x503000);
        StoredMemory58 port{memory};stored58::Stored value;value.uid=9;
        require58(!stored58::read(port,image,0x10000,value,cause58)&&!cause58.empty()&&value==stored58::Stored{},"Invalid stored inverse succeeded or retained stale output");
        if(invalid58==7)require58(cause58.find("fixture unmapped original graph read=")==0,"Stored reader hid the original memory failure cause");
    }
    for(int mutation58:{1,2,3}){Memory memory;put_stored58(memory,false);StoredMemory58 port{memory,mutation58};stored58::Stored value;
        require58(!stored58::read(port,image,0x10000,value,cause58)&&cause58.find("changed during read")!=std::string::npos,
            "Stored reader accepted changed high UID bits, deep shape or valid original class");
    }
    {Memory memory;StoredMemory58 port{memory};stored58::Stored value;
        require58(!stored58::read(port,image,UINTPTR_MAX-0x100,value,cause58)&&cause58.find("overflows")!=std::string::npos,"Stored node range wrapped before a read");}
    return comparisons58;
}
}
inline unsigned zone_edit_graph_cases(){return zone_edit_graph_fixture::run();}
