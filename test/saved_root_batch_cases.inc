#include "object_identity/remove_runtime/copy/saved_scenery_batch.h"
#include <functional>
namespace saved_root_batch_fixture {
namespace host=mt2mp::build_actions::private_native::host;
namespace place=mt2mp::build_actions::private_native::place;
namespace saved=mt2mp::game::region_saved_identity;
struct Ports {
    saved_root_reader_fixture::Fixture native;
    host::SavedSceneryBatch batch;
    uint64_t boundary=0;
    unsigned indices=0,censuses=0,member_lookups=0;
    struct Owner final:place::SceneryHost {
        Ports& ports;explicit Owner(Ports& p):ports(p){}
        bool current(place::SceneryContext& out,std::string& error)override{return ports.native.current(out,error);}
        bool registry_object(uint64_t uid,uintptr_t& out,std::string& error)override{
            ++ports.member_lookups;return ports.native.registry_object(uid,out,error);
        }
    };
    bool published=false;std::function<void()> after_census;
    static constexpr auto image=saved_root_reader_fixture::Fixture::image;
    static constexpr auto map=saved_root_reader_fixture::Fixture::map;
    static constexpr auto root=saved_root_reader_fixture::Fixture::root;
    bool read(mt2mp::prop_lease::Prop& value,std::string& error){
        if(!batch.admit(image,map,root,boundary,error))return false;
        auto original_index=[&](uintptr_t i,uintptr_t m,uintptr_t r,saved::Index& out,auto& at,std::string& cause){
            ++indices;return saved::capture(i,m,r,out,at,cause);
        };
        auto index=[&](uintptr_t i,uintptr_t m,uintptr_t r,saved::Index& out,auto& at,std::string& cause){
            return batch.index(i,m,r,out,at,cause,original_index);
        };
        auto witness=[&](auto& memory,auto& owner,uintptr_t i,uintptr_t region,uintptr_t r,uintptr_t manager,uintptr_t object,uint64_t uid,std::string& cause){
            return batch.member(memory,owner,i,region,r,manager,object,uid,cause);
        };
        auto typed=[&](uintptr_t object,uint64_t uid,std::string& cause){return native.typed(object,uid,cause);};
        Owner owner(*this);
        return host::read_saved_root_scenery(native.memory,owner,image,map,root,59,
            native.region,native.object,native.uid,typed,value,error,index,witness);
    }
    bool finish(std::string& error){
        auto at=[&](uintptr_t pointer,size_t offset,auto& value,std::string& cause){return native.memory.copy(pointer+offset,&value,sizeof value,cause);};
        auto index=[&](uintptr_t i,uintptr_t m,uintptr_t r,saved::Index& out,auto& read,std::string& cause){
            ++indices;return saved::capture(i,m,r,out,read,cause);
        };
        auto region=[&](const place::SceneryContext&,place::SceneryWitness& out,std::string& cause){
            ++censuses;Owner owner(*this);if(!place::read_scenery(native.memory,owner,image,out,cause))return false;
            if(after_census)after_census();return true;
        };
        auto current_boundary=[&](){return boundary;};
        if(!batch.finish(current_boundary,index,at,region,error))return false;
        published=true;return true;
    }
};
template<class Check>void run(Check&& need){
    using F=saved_root_reader_fixture::Fixture;
    std::string error;mt2mp::prop_lease::Prop actual,expected;
    F original;need(original.read(expected,error),"unbatched original saved Root owner fixture");
    Ports p;
    for(unsigned n=0;n<20;++n){need(p.read(actual,error),"repeated exact Root metadata remains admitted inside batch");
        need(actual.uid==expected.uid&&actual.registry_object==expected.registry_object&&actual.complete_ownership==expected.complete_ownership&&
             actual.class_addresspoint==expected.class_addresspoint,"batch returns identical native owner proof");}
    need(p.indices==1&&p.member_lookups==2&&p.native.typed_calls==40&&!p.published,"batch shares complete index while retaining both per-call typed proofs");
    need(p.finish(error)&&p.indices==3&&p.censuses==1&&p.member_lookups==4&&p.published,"publication rereads complete Region census and both complete index boundaries");
    for(unsigned kind=0;kind<9;++kind){Ports changed;actual={};error.clear();need(changed.read(actual,error),"initial batch capture before mutation");
        if(kind==0)changed.native.memory.put(F::object+0x18,F::uid+1);
        if(kind==1)changed.native.memory.put(F::object+0x170,F::root+8);
        if(kind==2)changed.native.memory.put(F::object+0x268,int32_t(9));
        if(kind==3)changed.native.memory.put(F::slots,F::object+8);
        if(kind==4)changed.native.memory.put(F::region+0x960,F::manager+8);
        if(kind==5)changed.native.memory.put(F::region+0x834,int32_t(60));
        if(kind==6)changed.native.lookup=F::object+8;
        if(kind==7){place::Transform moved;moved.rotation={0,0,0,1};moved.position={2,3,4};moved.scale={1,1,1};changed.native.memory.put(F::object+0x100,moved);}
        if(kind==8){changed.native.memory.put(F::manager+0x18+8*0x30+0x10,int32_t(0));changed.native.memory.put(F::manager+0x18+8*0x30+0x14,int32_t(0));}
        need(!changed.finish(error)&&!changed.published&&!error.empty(),"removed, replaced, foreign owner, UID, category, manager, index or transform blocks publication");
    }
    Ports failed;error.clear();need(failed.read(actual,error),"capture before final current World failure");failed.native.fail="world";
    need(!failed.finish(error)&&error=="original saved Scenery world changed"&&!failed.published,"final current World failure preserves original cause");
    failed.native.fail.clear();error.clear();need(!failed.read(actual,error)&&error=="original saved Scenery world changed","failed batch cannot revive after a later recovered native read");
    Ports native;error.clear();need(native.read(actual,error),"capture before unexpected native effect boundary");++native.boundary;
    need(!native.finish(error)&&error=="saved Scenery batch native boundary or owner changed"&&!native.published&&native.censuses==0,
         "native boundary invalidation stops before cached values publish or another native census runs");
    Ports during;error.clear();need(during.read(actual,error),"capture before effect during final native census");during.after_census=[&]{++during.boundary;};
    need(!during.finish(error)&&error=="saved Scenery batch native boundary or owner changed"&&!during.published,
         "native effect during final census cannot publish an earlier witness");
    Ports memory;error.clear();need(memory.read(actual,error),"capture before final memory failure");memory.native.memory.fail_address=F::region+0x960;
    need(!memory.finish(error)&&error=="original saved Scenery memory failure"&&!memory.published,"final memory read failure is not replaced by a batch fallback");
    Ports untouched;error.clear();need(untouched.finish(error)&&untouched.indices==0&&untouched.censuses==0,"empty readonly traversal needs no invented Region or Building anchor");
}
}
