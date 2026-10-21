#include "object_identity/remove_runtime/copy/saved_scenery_reader.h"
#include <functional>
namespace saved_root_reader_fixture {
namespace place=mt2mp::build_actions::private_native::place;
namespace reader=mt2mp::build_actions::private_native::host;
struct Memory final:mt2mp::build_actions::private_native::Memory {
    std::map<uintptr_t,unsigned char> bytes;uintptr_t fail_address=0;
    template<class T>void put(uintptr_t pointer,const T& value){const auto* raw=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[pointer+i]=raw[i];}
    bool copy(uintptr_t pointer,void* out,size_t count,std::string& error)override{
        if(pointer==fail_address){error="original saved Scenery memory failure";return false;}
        auto* raw=static_cast<unsigned char*>(out);for(size_t i=0;i<count;++i){const auto at=bytes.find(pointer+i);if(at==bytes.end()){error="fixture saved Scenery memory absent";return false;}raw[i]=at->second;}return true;
    }
};
struct Fixture final:place::SceneryHost {
    static constexpr uintptr_t image=0x140000000,map=0x100000,root=map+0x1c0,first=0x200000,region=0x210000,manager=0x300000,object=0x400000,saved=0x500000,subset=0x600000,slots=0x700000;
    static constexpr uint64_t uid=0x100000005;
    Memory memory;uintptr_t lookup=object;unsigned typed_calls=0;std::string fail;std::function<void()> after_typed;
    Fixture(){
        memory.put(map,image+0x1742860);memory.put(root,image+0x174ad20);memory.put(root+0x48,int32_t(0));
        memory.put(map+0x230,image+0x16f04e0);memory.put(map+0x238,saved);memory.put(map+0x240,int32_t(2));memory.put(map+0x244,int32_t(2));
        memory.put(saved,first);memory.put(saved+8,region);memory.put(map+0x358,subset);memory.put(map+0x360,int32_t(1));memory.put(map+0x364,int32_t(1));memory.put(subset,first);
        for(const auto [pointer,id]:{std::pair{first,0},std::pair{region,59}}){memory.put(pointer,image+0x1787930);memory.put(pointer+0x70,root);memory.put(pointer+0x834,int32_t(id));}
        memory.put(region+0x960,manager);memory.put(manager,image+0x17152d0);memory.put(manager+0x900,region);memory.put(manager+0x908,root);
        for(size_t category=0;category<47;++category){const auto off=0x18+category*0x30;memory.put(manager+off,image+0x172d6a0);memory.put(manager+off+8,category==8?slots:uintptr_t(0));memory.put(manager+off+0x10,int32_t(category==8));memory.put(manager+off+0x14,int32_t(category==8));}
        memory.put(slots,object);memory.put(object,image+0x16c1390);memory.put(object+0x18,uid);memory.put(object+0x170,root);memory.put(object+0x178,region);memory.put(object+0x268,int32_t(8));
        place::Box bounds;bounds.valid=1;bounds.max={1,1,1};memory.put(object+0x90,bounds);
        place::Transform pose;pose.rotation={0,0,0,1};pose.scale={1,1,1};memory.put(object+0x100,pose);
    }
    bool current(place::SceneryContext& out,std::string& error)override{
        if(fail=="world"){error="original saved Scenery world changed";return false;}
        out={7,region,root,manager,true,true,true};return true;
    }
    bool registry_object(uint64_t full,uintptr_t& out,std::string& error)override{
        if(fail=="lookup"){error="original saved Scenery UID lookup failure";return false;}
        if(full!=uid){error="fixture unexpected full UID";return false;}out=lookup;return true;
    }
    bool typed(uintptr_t pointer,uint64_t full,std::string& error){
        ++typed_calls;uint64_t actual_uid=0;if(!memory.copy(pointer+0x18,&actual_uid,sizeof actual_uid,error))return false;
        if(pointer!=object||full!=uid||actual_uid!=uid||lookup!=object){error="original saved Scenery typed/full UID failure";return false;}
        if(after_typed)after_typed();return true;
    }
    bool read(mt2mp::prop_lease::Prop& out,std::string& error,int32_t id=59,uintptr_t expected=region){
        auto exact=[&](uintptr_t pointer,uint64_t full,std::string& cause){return typed(pointer,full,cause);};
        return reader::read_saved_root_scenery(memory,*this,image,map,root,id,expected,object,uid,exact,out,error);
    }
};
template<class Check>void run(Check&& check){
    std::string error;mt2mp::prop_lease::Prop value;
    Fixture first;check(first.read(value,error)&&value.uid==Fixture::uid&&value.registry_object==reinterpret_cast<void*>(Fixture::object)&&value.complete_ownership,"actual saved Scenery accepts exact Region omitted from current Root subset");
    uintptr_t subset=0;check(first.memory.copy(Fixture::subset,&subset,sizeof subset,error)&&subset!=Fixture::region,"fixture reproduces saved/current Region collection difference");
    for(int kind=0;kind<10;++kind){Fixture f;
        if(kind==0)f.memory.put(Fixture::saved+8,Fixture::first);
        if(kind==1)f.memory.put(Fixture::region+0x70,Fixture::root+8);
        if(kind==2)f.memory.put(Fixture::manager+0x900,Fixture::first);
        if(kind==3)f.memory.put(Fixture::object+0x268,int32_t(19));
        if(kind==4)f.memory.put(Fixture::object+0x18,Fixture::uid+1);
        if(kind==5)f.memory.put(Fixture::object,Fixture::image+0x16d93c0);
        if(kind==6)f.lookup=Fixture::object+8;
        if(kind==7)f.memory.put(Fixture::region+0x834,int32_t(60));
        if(kind==8)f.fail="world";
        if(kind==9)f.memory.fail_address=Fixture::region+0x960;
        value={};error.clear();check(!f.read(value,error)&&!error.empty()&&!value.complete_ownership,"saved Scenery rejects foreign/replaced class, full UID, Region, manager, category or current world");
        if(kind==8)check(error=="original saved Scenery world changed","original world failure retained");
        if(kind==9)check(error=="original saved Scenery memory failure","original member read failure retained");
    }
    Fixture changed;changed.after_typed=[&]{if(changed.typed_calls==1)changed.memory.put(Fixture::region+0x834,int32_t(60));};value={};error.clear();
    check(!changed.read(value,error)&&error=="saved_scenery_complete_region_index_changed"&&!value.complete_ownership,"saved owner replacement during exact read cannot publish a confirmed Prop");
    Fixture wrong;value={};error.clear();check(!wrong.read(value,error,59,Fixture::first),"saved Region ID cannot bind a different receiver");
    Fixture lookup;lookup.fail="lookup";value={};error.clear();check(!lookup.read(value,error)&&error=="original saved Scenery UID lookup failure","original registry read failure retained");
}
}
