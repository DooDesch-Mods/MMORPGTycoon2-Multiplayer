namespace gizmo_move_read_fixture {
namespace r=mt2mp::game::dungeon_gizmo_move_read;namespace c=mt2mp::game::dungeon_gizmo_control;
struct Memory{
    std::map<uintptr_t,unsigned char> storage;
    template<class T>void put(uintptr_t address,const T& value){const auto* data=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)storage[address+i]=data[i];}
    bool read(uintptr_t address,void* output,size_t count,std::string& error){auto* data=static_cast<unsigned char*>(output);for(size_t i=0;i<count;++i){const auto found=storage.find(address+i);
        if(found==storage.end()){error="controlled native Gizmo cursor read failed";return false;}data[i]=found->second;}return true;}
};
template<class Check>void run(Check& check){
    constexpr uintptr_t base=0x140000000,behaviour=0x200000000,records=0x210000000,costs=0x220000000,cursor=0x230000000;
    for(bool scene_scenery:{false,true})for(bool duplicate:{false,true}){Memory memory;
        memory.put(behaviour,base+0x172f1f0);memory.put(behaviour+0x401,uint8_t(1));memory.put(behaviour+0x499,uint8_t(duplicate));
        memory.put(behaviour+0x458,base+0x1747300);memory.put(behaviour+0x460,records);memory.put(behaviour+0x468,int32_t(2));memory.put(behaviour+0x46c,int32_t(4));
        memory.put(behaviour+0x478,base+0x1748c50);memory.put(behaviour+0x480,costs);memory.put(behaviour+0x488,int32_t(2));memory.put(behaviour+0x48c,int32_t(4));
        for(unsigned i=0;i<2;++i){const uintptr_t object=0x300000000+i*0x1000,ghost=0x400000000+i*0x1000,blueprint=0x500000000+i*0x1000;
            memory.put(records+i*24,blueprint);memory.put(records+i*24+8,ghost);memory.put(records+i*24+16,object);
            memory.put(object,base+(scene_scenery&&i==1?0x16c1390:0x1749ab0));memory.put(object+0x18,uint64_t(0x100000001)+uint64_t(i)*0x100000000);
            memory.put(ghost,base+0x1705230);memory.put(ghost+0x210,blueprint);memory.put(costs+i*8,object);
            c::Row value;value.before.rotation={0,0,0,1};value.before.scale={1,1,1};value.before.position={1,2,3};auto pose=c::pose(value.before);
            pose[4]=std::bit_cast<uint32_t>(11.0f+float(i));memory.put(ghost+0x100,pose);}
        const auto owner=[&](uintptr_t object,uint64_t uid,c::Row& row,std::string& error){const auto index=uint32_t((object-0x300000000)/0x1000);
            if(index>1||uid!=uint64_t(0x100000001)+uint64_t(index)*0x100000000){error="full UID owner changed";return false;}
            row.key=mt2mp::game::dungeon_gizmo_identity::birth("11111111-1111-4111-8111-111111111111",index);
            row.before.rotation={0,0,0,1};row.before.scale={1,1,1};row.before.position={1,2,3};row.cost=duplicate?100:0;
            if(scene_scenery&&index==1){row.scenery=c::Scenery{"Trees","oak",c::pose(row.before)};row.before={};
                row.key=mt2mp::game::scenery_scene_key::record(std::string(64,'a'),67,3,-1,0);}return true;};
        const auto point=[](std::array<uint32_t,3>& out,std::string&){out={0,std::bit_cast<uint32_t>(20.0f),0};return true;};
        r::Capture first,second;std::string error;
        check(r::read(memory,base,behaviour,owner,point,first,error)&&first.rows.size()==2&&first.duplicate==duplicate,"actual Gizmo cursor body reads full ordered owner/ghost/cost input");
        check(r::read(memory,base,behaviour,owner,point,second,error)&&first==second,"complete repeated cursor capture is stable");
        for(unsigned failure=0;failure<6;++failure){auto bad=memory;
            if(failure==0)bad.put(records+24+16,uintptr_t(0x300000000));
            if(failure==1)bad.put(0x300000000+0x18,uint64_t(1));
            if(failure==2)bad.put(0x400000000+0x210,uintptr_t(0xdead));
            if(failure==3)bad.put(0x300001000,base+(scene_scenery?0x1749ab0:0x16c1390));
            if(failure==4)bad.put(behaviour+0x468,int32_t(5));
            if(failure==5)bad.storage.erase(0x400000000+0x100);
            check(!r::read(bad,base,behaviour,owner,point,second,error)&&!error.empty(),"Gizmo cursor rejects repeated/missing/full UID/foreign/incomplete original values");}
        auto changed=memory;changed.put(0x400000000+0x110,std::bit_cast<uint32_t>(50.0f));
        check(r::read(changed,base,behaviour,owner,point,second,error)&&first!=second,"second complete cursor read detects fresh Ghost transform change");
        memory.put(behaviour+0x468,int32_t(0));memory.put(behaviour+0x490,uintptr_t(0));memory.put(base+0x13df9d0,cursor);memory.put(cursor+0x190,uintptr_t(0));
        for(const auto& row:first.rows){memory.put(row.record,uintptr_t(0));memory.put(row.record+8,uintptr_t(0));}
        check(r::cleaned(memory,base,first,error),"original deactivation proof reads retained record slots without freed Ghost dereference");
        memory.put(first.rows[0].record+8,uintptr_t(1));check(!r::cleaned(memory,base,first,error),"uncleared owned Ghost slot is rejected before submit");
    }
}
}
