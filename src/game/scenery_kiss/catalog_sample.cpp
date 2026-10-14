#include "catalog_sample.h"
#include <exception>
#include <cmath>
namespace mt2mp::scenery_kiss {
namespace {
struct Raw {uintptr_t ap=0,shard=0;uint64_t uid=0;int32_t category=0,variant=0;std::array<float,4> rotation{};std::array<float,3> position{},scale{};bool operator==(const Raw&)const=default;};
bool fail(std::string& e,const char* why){e=why;return false;}
template<class T>bool at(native::Memory& m,uintptr_t p,size_t offset,T& out,std::string& e){if(!p||p>UINTPTR_MAX-offset||p+offset>UINTPTR_MAX-sizeof(T))return fail(e,"scenery_catalog_sample_address_overflow");return m.copy(p+offset,&out,sizeof out,e);}
bool read(native::Memory& m,uintptr_t p,Raw& r,std::string& e){return at(m,p,0,r.ap,e)&&at(m,p,0x18,r.uid,e)&&at(m,p,0x170,r.shard,e)&&at(m,p,0x268,r.category,e)&&at(m,p,0x26c,r.variant,e)&&at(m,p,0x100,r.rotation,e)&&at(m,p,0x110,r.position,e)&&at(m,p,0x11c,r.scale,e);}
}
bool read_catalog_sample(native::Memory& memory,Catalog& catalog,uintptr_t image,uintptr_t object,uint64_t uid,uintptr_t root,CatalogSample& out,std::string& e){
    out={};e.clear();try{
        if(!image||image>UINTPTR_MAX-0x16c1390||!object||!uid||!root)return fail(e,"scenery_catalog_sample_actual_identity_missing");
        Raw first,second;if(!read(memory,object,first,e))return false;
        if(first.ap!=image+0x16c1390||first.uid!=uid||first.shard!=root||first.category<0||first.category>=47)return fail(e,"scenery_catalog_sample_AP_UID_root_category_differs");
        std::string type,variant,again_type,again_variant;
        if(!catalog.identify(first.category,first.variant,type,variant,e)||!read(memory,object,second,e)||!catalog.identify(first.category,first.variant,again_type,again_variant,e))return false;
        if(first!=second||type!=again_type||variant!=again_variant)return fail(e,"scenery_catalog_sample_native_identity_or_pose_changed");
        State state{type,variant,first.position[0],first.position[1],first.position[2],first.rotation,first.scale,true};
        if(!valid_state(state,e))return false;
        out={uid,first.category,first.variant,std::move(state)};e.clear();return true;
    }catch(const std::exception& cause){e=std::string("scenery_catalog_sample_exception: ")+cause.what();return false;}
    catch(...){return fail(e,"scenery_catalog_sample_nonstandard_exception");}
}
}
