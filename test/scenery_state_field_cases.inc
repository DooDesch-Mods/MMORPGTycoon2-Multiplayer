#include <array>
#include <cmath>
#include <limits>
namespace scenery_state_field_fixture {
namespace native {struct Memory:world_prop_attribution_fixture::Memory{};}
#include "scenery_state_field_types.inc"
#include "scenery_state_field_bodies.inc"
struct Owner final:Root {
    Binding actual;
    unsigned calls=0;
    bool reject=false,change_second=false;
    t::PageReadScope* parent=nullptr;
    bool bind(uint64_t uid,Binding& out,std::string& error)override{
        ++calls;need(t::page_read_scope==parent&&!t::PageReadScope::reader_active(),"actual Scenery Root callback stays outside both pure field scopes");
        if(reject){error="original Scenery owner failure";return false;}
        if(uid!=actual.uid){error="original Scenery owner UID differs";return false;}
        if(change_second&&calls==2){const float value=19;std::memcpy(reinterpret_cast<void*>(actual.object+0x110),&value,sizeof value);}
        out=actual;return true;
    }
};
struct Catalogue final:Catalog {
    unsigned calls=0;
    bool reject=false,protect=false;
    uintptr_t object=0;
    t::PageReadScope* parent=nullptr;
    bool identify(int32_t category,int32_t variant,std::string& directory,std::string& filename,std::string& error)override{
        ++calls;need(t::page_read_scope==parent&&!t::PageReadScope::reader_active(),"actual Scenery catalogue/allocator callback stays outside both pure field scopes");
        if(reject){error="original Scenery catalogue failure";return false;}
        if(category!=8||variant!=2){error="original Scenery catalogue key differs";return false;}
        if(protect){DWORD previous=0;need(VirtualProtect(reinterpret_cast<void*>(object),0x1000,PAGE_NOACCESS,&previous)!=0,"protect only the owned Scenery fixture inside original catalogue callback");}
        directory="cactus";filename="p_ext_treecactus_01.vmb";return true;
    }
};
}
void scenery_state_field_cases(){
    namespace f=scenery_state_field_fixture;
    using Memory=f::native::Memory;
    auto* owned=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x2000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    need(owned!=nullptr,"allocate only the owned Scenery native field fixture");
    struct Release {void* allocation;~Release(){if(allocation&&!VirtualFree(allocation,0,MEM_RELEASE))std::terminate();}} release{owned};
    const auto object=reinterpret_cast<uintptr_t>(owned)+0x1000,image=object-0x1740000;
    auto put=[&](uintptr_t offset,const auto& value){std::memcpy(reinterpret_cast<void*>(object+offset),&value,sizeof value);};
    auto initial=[&]{
        std::memset(owned,0,0x2000);put(0,uintptr_t(image+0x16c1390));put(0x18,uint64_t(77));put(0x170,object+0x800);put(0x178,object+0x900);
        put(0x268,int32_t(8));put(0x26c,int32_t(2));put(0x100,std::array<float,4>{0,0,0,1});put(0x110,float(11));put(0x114,float(12));put(0x118,float(13));put(0x11c,std::array<float,3>{1,2,3});
    };
    initial();Memory memory;memory.field_mode=true;f::Owner owner;owner.actual={object,77,41,7,object+0x700,object+0x800,object+0x900};f::Catalogue catalogue;catalogue.object=object;
    std::string error;f::State before,after;
    auto reset=[&]{initial();memory.field_reads.clear();memory.failed_field=memory.throw_field=0;owner.calls=catalogue.calls=0;owner.reject=owner.change_second=false;catalogue.reject=catalogue.protect=false;owner.parent=catalogue.parent=nullptr;error.clear();};
    t::totals={};t::Scope measured(t::Part::HostCapture);
    auto start=count(t::Part::HostMemoryVirtualQuery);
    need(f::baseline_read_state(memory,catalogue,owner,image,77,before,error),"original double Scenery state read failed");
    const auto reads=memory.field_reads;const auto original_queries=count(t::Part::HostMemoryVirtualQuery)-start;
    need(reads.size()==22&&original_queries==22&&owner.calls==2&&catalogue.calls==2,"original Scenery state reads22 fields with exactly two Root and catalogue calls");
    reset();start=count(t::Part::HostMemoryVirtualQuery);
    need(f::read_state(memory,catalogue,owner,image,77,after,error)&&after==before&&memory.field_reads==reads,"actual Scenery scopes preserve full exact state and every field read in order");
    need(count(t::Part::HostMemoryVirtualQuery)-start==4&&owner.calls==2&&catalogue.calls==2&&!t::page_read_scope,"actual identity/pose extents reduce22 OS queries to4 without reducing fields or native callbacks");
    for(size_t field=0;field<11;++field){
        reset();memory.failed_field=reads[field].first;need(!f::baseline_read_state(memory,catalogue,owner,image,77,before,error),"original field failure did not reject");
        const auto cause=error;const auto trace=memory.field_reads;const auto root_calls=owner.calls,catalogue_calls=catalogue.calls;
        reset();memory.failed_field=reads[field].first;
        need(!f::read_state(memory,catalogue,owner,image,77,after,error)&&error==cause&&memory.field_reads==trace&&owner.calls==root_calls&&catalogue.calls==catalogue_calls&&!after.alive&&!t::page_read_scope,"actual Scenery scope keeps first failing native field/cause/callback order and cannot publish partial state");
    }
    reset();owner.reject=true;need(!f::read_state(memory,catalogue,owner,image,77,after,error)&&error=="original Scenery owner failure"&&owner.calls==1&&catalogue.calls==0&&memory.field_reads.empty()&&!t::page_read_scope,"owner failure stops before native field or catalogue work");
    reset();catalogue.reject=true;need(!f::read_state(memory,catalogue,owner,image,77,after,error)&&error=="original Scenery catalogue failure"&&owner.calls==1&&catalogue.calls==1&&memory.field_reads.size()==6&&!t::page_read_scope,"catalogue failure keeps original cause and stops before pose");
    reset();owner.change_second=true;need(!f::read_state(memory,catalogue,owner,image,77,after,error)&&error=="scenery_state_changed_during_read"&&!after.alive&&!t::page_read_scope,"second pass freshly observes native pose changes and rejects mismatched double read");
    reset();memory.throw_field=object+0x114;need(!f::read_state(memory,catalogue,owner,image,77,after,error)&&error=="scenery_state_exception: original field read exception"&&!t::page_read_scope,"original field exception keeps actual conversion cause and restores pure scope");
    reset();{t::PageReadScope original(t::PageBoundary::OriginalCall);owner.parent=catalogue.parent=t::page_read_scope;start=count(t::Part::HostMemoryVirtualQuery);
        need(f::read_state(memory,catalogue,owner,image,77,after,error)&&count(t::Part::HostMemoryVirtualQuery)-start==4&&t::page_read_scope==owner.parent&&!t::PageReadScope::reader_active(),"actual field scopes reenter beneath OriginalCall and restore it before both native callbacks");
    }
    reset();catalogue.protect=true;const bool accepted=f::read_state(memory,catalogue,owner,image,77,after,error);const auto protected_cause=error;DWORD previous=0;
    const bool restored=VirtualProtect(reinterpret_cast<void*>(object),0x1000,PAGE_READWRITE,&previous)!=0;
    need(restored,"restore only the Scenery fixture page after original callback boundary");
    need(!accepted&&protected_cause=="original native field page unreadable"&&owner.calls==1&&catalogue.calls==1&&!after.alive&&!t::page_read_scope,"fresh post-catalogue pose scope detects changed page mapping instead of borrowing pre-call metadata");
    t::totals={};
}
