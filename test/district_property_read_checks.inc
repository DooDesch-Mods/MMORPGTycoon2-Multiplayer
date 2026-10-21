void property_read_checks(){
    namespace property=mt2mp::game::district_property;
    namespace controls=mt2mp::game::district_property_ui;
    namespace inverse=mt2mp::game::district_edit_history;
    constexpr uintptr_t widget=0x910000,value_object=0x920000;
    std::string error;
    for(const auto& field:property::fields){
        reset();auto& memory=fixture_runtime.memory;memory.zero(widget,0x500);memory.zero(value_object,0x60);
        const auto type=field.type==property::Type::Number?controls::Widget::Spinner:field.type==property::Type::Flag?controls::Widget::Checkbox:controls::Widget::Color;
        const auto value=type==controls::Widget::Color?widget+0x338:value_object;
        memory.put(widget,image+(type==controls::Widget::Spinner?0x16c1720:type==controls::Widget::Checkbox?0x16d98c0:0x17224a0));
        if(type!=controls::Widget::Color)memory.put(widget+(type==controls::Widget::Spinner?0x428:0x3c8),value);
        memory.put(value,image+(type==controls::Widget::Spinner?0x1749660:type==controls::Widget::Checkbox?0x1749610:0x1749520));
        memory.put(value+8,object_ptr);memory.text(value+0x18,0x930000,std::string(field.name));memory.put(value+0x38,object_ptr+field.offset);
        memory.put(image+field.property,image+(field.type==property::Type::Number?0x16d3050:field.type==property::Type::Flag?0x16cfb60:0x16c69b0));
        memory.put(image+field.property+0x38,field.offset);
        controls::Binding bound;bool ours=false;
        need(controls::bind(memory,image,widget,type,bound,ours,error)&&ours&&bound.object==object_ptr&&bound.uid==object_uid&&bound.kind==field.kind,error);
        need(controls::unchanged(memory,image,bound,type,error),error);
        memory.put(value+0x38,object_ptr+field.offset+1);
        need(!controls::bind(memory,image,widget,type,bound,ours,error),"wrong native property address admitted");memory.put(value+0x38,object_ptr+field.offset);
        memory.put(image+field.property+0x38,field.offset+1);
        need(!controls::bind(memory,image,widget,type,bound,ours,error),"changed original property descriptor admitted");memory.put(image+field.property+0x38,field.offset);
        memory.put(widget+(type==controls::Widget::Spinner?0x380:type==controls::Widget::Checkbox?0x3c0:0x2e8),uintptr_t(1));
        need(!controls::bind(memory,image,widget,type,bound,ours,error),"additional native world-write route admitted");
    }
    reset();auto& memory=fixture_runtime.memory;
    constexpr uintptr_t graph=0xa10000,node=0xa20000,items=0xa30000,polygons=0xa40000;
    memory.zero(graph,0x120);memory.zero(node,0x200);memory.zero(polygons,24);
    memory.put(graph,image+0x16e32d0);memory.put(graph+0x18,image+0x16e2dc0);memory.put(graph+0x20,items);
    memory.put(graph+0x28,int32_t(1));memory.put(graph+0x2c,int32_t(1));memory.put(items,node);
    memory.put(graph+0x48,image+0x1748c50);memory.put(graph+0x10c,uint8_t(1));
    memory.put(node,image+0x17132b0);memory.put(node+0xb8,object_uid);memory.put(node+0xc0,region_ptr);memory.put(node+0xc8,map_ptr+0x1c0);
    memory.put(node+0xf0,polygons);memory.text(node+0x160,0xa50000,"Old");memory.text(node+0x180,0xa60000,"Description");
    action::DistrictState before;
    need(district_reader::read(memory,image,map_ptr,fixture_target,contains,before,error),error);
    inverse::Stored proof;need(inverse::graph(memory,image,graph,proof,error),error);
    need(proof==inverse::stored(object_uid,region_ptr,map_ptr+0x1c0,before),"original Edit payload projection differs");
    memory.put(node+0xb8,uint64_t(0));need(!inverse::graph(memory,image,graph,proof,error),"zero inverse full UID admitted");memory.put(node+0xb8,object_uid);
    memory.put(node+0x1bc,uint8_t(2));need(!inverse::graph(memory,image,graph,proof,error),"nonboolean inverse option admitted");memory.put(node+0x1bc,uint8_t(0));
    memory.put(graph+0x10c,uint8_t(0));need(!inverse::graph(memory,image,graph,proof,error),"unlocked inverse graph admitted");memory.put(graph+0x10c,uint8_t(1));
    memory.put(graph+0x28,int32_t(2));need(!inverse::graph(memory,image,graph,proof,error),"extra inverse operation admitted");memory.put(graph+0x28,int32_t(1));
    need(inverse::graph(memory,image,graph,proof,error),error);
    auto current=before;current.flags[1]=current.flags[12]=1;current.values[4]=0x3f000000;current.values[15]=0x3f400000;
    const auto projected=inverse::project(current,proof);
    need(projected.flags[1]==1&&projected.flags[12]==1&&projected.values[4]==current.values[4]&&projected.values[15]==current.values[15],"original history overwrote properties absent from its record");
}
