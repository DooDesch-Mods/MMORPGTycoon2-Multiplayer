#include "release_graph_native.h"
#include "subscriber_lifecycle/append_calls.h"
#include <Windows.h>
#include <atomic>
#include <memory>
#include <stdexcept>
namespace mt2mp::game::release_graph_native {
namespace {
namespace c=subscriber_lifecycle;
thread_local uintptr_t prepared_capture_image=0;
void save_prepared(const void* object,void* record,void* context){
    const auto image=prepared_capture_image;
    reinterpret_cast<void(*)(void*,const char*)>(image+0x964b40)(record,reinterpret_cast<const char*>(image+0x13e7438));
    reinterpret_cast<void(*)(const void*,void*,void*)>(image+0xd55e20)(object,record,context);
}
struct Borrow {c::Memory& memory;const Key& key;release_graph::Value& value;release_graph::Kind kind;};
bool inspect(void* user,const void* root,const void* context,std::string& e){auto& b=*static_cast<Borrow*>(user);c::Graph graph;
    if(!c::read_graph(b.memory,uintptr_t(root),uintptr_t(context),{release_graph::maximum_records,release_graph::maximum_tokens,release_graph::maximum_text,release_graph::maximum_links},graph,e))return false;
    return release_graph::capture_value(graph,b.key,b.value,e,b.kind);
}
}

namespace {
using Decode=c::first_append::DecodeApi;
struct LoadScope {
    DWORD thread=0;uintptr_t image=0,context=0,manager=0,fresh=0;c::Memory* memory=nullptr;
    const Resolve* resolve=nullptr;const Guard* guard=nullptr;
    c::LookupScope links;bool failed=false,suppressed=false;std::string cause;
};
std::atomic<PostResolve> original_postresolve{nullptr};
// The game thread owns this scope. Other threads retain the ordinary original path.
std::atomic<LoadScope*> active_load{nullptr};
bool fresh_link(void* user,const c::ExternalLink& link,std::string& e){auto& scope=*static_cast<LoadScope*>(user);uintptr_t current=0;
    if(!(*scope.guard)(e)||!(*scope.resolve)(link.portable_key,current,e)||!current||current!=link.object){if(e.empty())e="Release external canonical owner changed during original Resolve";scope.failed=true;scope.cause=e;return false;}return true;
}
uint32_t invoke(uintptr_t image,unsigned step,uintptr_t object,uintptr_t record,uintptr_t context)noexcept{
    __try{switch(step){
    case 0:reinterpret_cast<void(*)(void*)>(image+0x89e940)(reinterpret_cast<void*>(context));break;
    case 1:reinterpret_cast<void(*)(void*,void*,void*)>(image+0xcafb20)(reinterpret_cast<void*>(object),reinterpret_cast<void*>(record),reinterpret_cast<void*>(context));break;
    case 2:reinterpret_cast<void(*)(void*)>(image+0x89e480)(reinterpret_cast<void*>(context));break;
    case 3:reinterpret_cast<void(*)(void*)>(image+0x4e5c30)(reinterpret_cast<void*>(object));break;
    case 5:reinterpret_cast<void(*)(void*)>(image+0xb8b870)(reinterpret_cast<void*>(object));break;
    case 4:reinterpret_cast<void(*)(void*,void*,void*)>(image+0xb8ada0)(reinterpret_cast<void*>(object),reinterpret_cast<void*>(record),reinterpret_cast<void*>(context));break;
    }return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t create_array(uintptr_t image,uintptr_t& object)noexcept{
    __try{const auto base=uintptr_t(reinterpret_cast<void*(*)(const void*)>(image+0xd84130)(nullptr));object=base>=0x18?base-0x18:0;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool failure(std::string& e,const std::string& cause){e=cause;return false;}
uint32_t exchange_current(uintptr_t address,uintptr_t expected,uintptr_t desired,bool& stored)noexcept{
    __try{stored=uintptr_t(InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(address),reinterpret_cast<void*>(desired),reinterpret_cast<void*>(expected)))==expected;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool set_postresolve_original(PostResolve original,std::string& e){
    if(!original)return failure(e,"Release PostResolve original trampoline missing");
    auto expected=PostResolve{};if(!original_postresolve.compare_exchange_strong(expected,original)&&expected!=original)return failure(e,"Release PostResolve original owner changed");e.clear();return true;
}
void postresolve_entry(void* receiver,void* context){
    auto* scope=active_load.load(std::memory_order_acquire);const auto original=original_postresolve.load(std::memory_order_acquire);
    if(!original)throw std::runtime_error("Release PostResolve original owner is not installed");
    if(!scope||scope->thread!=GetCurrentThreadId()||scope->context!=uintptr_t(context)||scope->fresh!=uintptr_t(receiver)){original(receiver,context);return;}
    std::string e;bool guarded=false;
    try{auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){if(!object||object>UINTPTR_MAX-offset){cause="Release PostResolve owner address overflow";return false;}return scope->memory->read(object+offset,&value,sizeof value,cause);};
        guarded=!scope->failed&&!scope->suppressed&&(*scope->guard)(e)&&release_current_owner::read(read,scope->image,scope->manager,scope->fresh,e);}
    catch(const std::exception& ex){e="Release fresh PostResolve guard C++: "+std::string(ex.what());}
    catch(...){e="Release fresh PostResolve guard unknown C++ exception";}
    if(!guarded){scope->failed=true;if(scope->cause.empty())scope->cause=e.empty()?"Release owned fresh PostResolve duplicate/failed scope":e;return;}
    // Resolve's normal repair would regenerate the already complete Host-owned changes.
    scope->suppressed=true;
}
bool lookup(void* context,int32_t id,c::Lookup original,void*& result){
    auto* scope=active_load.load(std::memory_order_acquire);
    if(!scope||scope->thread!=GetCurrentThreadId()||scope->context!=uintptr_t(context))return false;
    result=scope->links.lookup(GetCurrentThreadId(),context,id,original);return true;
}
bool apply_owned(uintptr_t image,c::Memory& memory,uintptr_t manager,const release_graph::Value& desired,const Key& key,const Resolve& resolve,const Guard& guard,ApplyReceipt& receipt,std::string& e){
    receipt={};if(!key||!resolve||!guard)return failure(e,"Release whole graph Apply requires canonical key/resolve/current-owner callbacks");
    if(desired.kind!=release_graph::Kind::Manager&&desired.kind!=release_graph::Kind::PreparedDemands)return failure(e,"Release graph Apply received another typed value family");
    if(!release_graph::valid(desired,e))return false;
    if(!original_postresolve.load(std::memory_order_acquire))return failure(e,"Release Host graph adoption requires admitted original PostResolve route");
    if(active_load.load(std::memory_order_acquire))return failure(e,"Release private original graph load already active");
    uintptr_t ap=0,old=0;if(!guard(e)||!memory.read(manager,&ap,8,e)||ap!=image+0x1714ed0||!memory.read(manager+0x38,&old,8,e)||!old||!memory.read(old,&ap,8,e)||ap!=image+0x16c12b0)return failure(e,e.empty()?"Release graph Apply actual Manager/current Release ownership differs":e);
    auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){if(!object||object>UINTPTR_MAX-offset){cause="Release current ownership address overflow";return false;}return memory.read(object+offset,&value,sizeof value,cause);};
    if(!release_current_owner::read(read,image,manager,old,e))return false;
    receipt.old_release=old;c::first_append::DecodeApi api;if(!c::first_append::resolve_record_api(memory,image,api,e))return false;
    const bool prepared=desired.kind==release_graph::Kind::PreparedDemands;
    c::first_append::NativeTree tree;if(!c::first_append::decode_release_record(api,memory,desired.records,tree,receipt.records,e,desired.kind)){
        const auto first=e;std::string cleanup;if(!receipt.records.unknown&&!c::first_append::destroy_records(api,tree,receipt.records,cleanup))e=first+"; "+cleanup;return false;}
    auto context=std::make_unique<uintptr_t>(0);LoadScope scope;scope.thread=GetCurrentThreadId();scope.image=image;scope.memory=&memory;scope.context=uintptr_t(context.get());scope.manager=manager;scope.resolve=&resolve;scope.guard=&guard;
    struct Lifetime {
        LoadScope& scope;std::unique_ptr<uintptr_t>& context;c::first_append::NativeTree& tree;ApplyReceipt& receipt;
        ~Lifetime(){auto expected=&scope;active_load.compare_exchange_strong(expected,nullptr);scope.links.clear();
            if(!receipt.confirmed&&(receipt.load_attempted||(receipt.context_constructed&&!receipt.context_closed))){receipt.unknown=true;if(!receipt.context_closed&&context)receipt.retained_context=uintptr_t(context.release());receipt.retained_record=tree.root;}}
    } lifetime{scope,context,tree,receipt};
    std::vector<c::ExternalLink> external;for(const auto& link:desired.external){uintptr_t object=0;if(!resolve(link.key,object,e)||!object){if(e.empty())e="Release external link has no current canonical owner";const auto first=e;std::string cleanup;if(!c::first_append::destroy_records(api,tree,receipt.records,cleanup))e=first+"; "+cleanup;return false;}external.push_back({link.id,link.key,object});}
    receipt.exception=invoke(image,0,manager,tree.root,scope.context);if(receipt.exception){receipt.unknown=true;e="Release original Context constructor SEH="+std::to_string(receipt.exception);receipt.retained_context=uintptr_t(context.release());receipt.retained_record=tree.root;return false;}receipt.context_constructed=true;
    if(!scope.links.arm(scope.thread,context.get(),release_graph::inline_ids(desired),std::move(external),&fresh_link,&scope,e)){
        // The constructed Context has no loaded objects. Its original destructor is safe once.
        const auto first=e;receipt.exception=invoke(image,2,manager,0,scope.context);receipt.context_closed=receipt.exception==0;
        if(receipt.exception){receipt.unknown=true;receipt.retained_context=uintptr_t(context.release());receipt.retained_record=tree.root;e=first+"; Context cleanup SEH="+std::to_string(receipt.exception);return false;}
        std::string cleanup;if(!c::first_append::destroy_records(api,tree,receipt.records,cleanup))e=first+"; "+cleanup;return false;}
    uintptr_t temporary=0;release_demand_owner::Storage old_storage,fresh_storage;
    if(prepared){receipt.exception=create_array(image,temporary);receipt.retained_array=temporary;
        if(receipt.exception||!temporary||!release_demand_owner::read(read,image,manager+0x70,false,old_storage,e)||!release_demand_owner::read(read,image,temporary,true,fresh_storage,e)){
            receipt.unknown=true;if(e.empty())e="Release original prepared demand factory/owner failed SEH="+std::to_string(receipt.exception);return false;}}
    active_load.store(&scope,std::memory_order_release);receipt.load_attempted=true;
    receipt.exception=invoke(image,prepared?4:1,prepared?temporary:manager,tree.root,scope.context);receipt.load_returned=receipt.exception==0;
    bool ok=receipt.load_returned;
    if(ok&&!prepared)ok=memory.read(manager+0x38,&scope.fresh,8,e)&&scope.fresh&&scope.fresh!=old&&memory.read(scope.fresh,&ap,8,e)&&ap==image+0x16c12b0&&guard(e);
    if(ok&&prepared)ok=release_current_owner::read(read,image,manager,old,e)&&guard(e);
    receipt.new_release=scope.fresh;
    if(!ok&&e.empty())e=receipt.exception?"Release original Manager Load SEH="+std::to_string(receipt.exception):"Release original Load did not install a new owned typed Release";
    if(ok&&!prepared){auto exchange=[&](uintptr_t address,uintptr_t expected,uintptr_t desired,bool& stored,std::string& cause){receipt.exception=exchange_current(address,expected,desired,stored);if(receipt.exception){cause="Release static current pointer CAS SEH="+std::to_string(receipt.exception);return false;}return true;};
        ok=release_current_owner::adopt(read,exchange,image,manager,old,scope.fresh,receipt.current,e);}
    if(ok){receipt.exception=invoke(image,2,manager,0,scope.context);receipt.context_closed=receipt.exception==0;ok=receipt.context_closed;if(!ok)e="Release original Context Resolve/destructor SEH="+std::to_string(receipt.exception);}
    receipt.lookup=scope.links.receipt();receipt.postresolve_suppressed=scope.suppressed;
    if(scope.failed&&!scope.cause.empty()){ok=false;e=scope.cause;}
    if(ok&&(scope.failed||receipt.lookup.failed||(!prepared&&!scope.suppressed)||(prepared&&scope.suppressed))){ok=false;e=scope.cause.empty()?(receipt.lookup.cause.empty()?"Release exact private PostResolve expectation differs":receipt.lookup.cause):scope.cause;}
    active_load.store(nullptr,std::memory_order_release);scope.links.clear();
    if(ok&&prepared){
        ok=release_demand_owner::read(read,image,temporary,true,fresh_storage,e);
        if(ok){release_graph::Value loaded;Receipt inspected;
            // The original member serializer uses only the ArrayStore prefix.
            Borrow borrow{memory,key,loaded,desired.kind};
            const c::CaptureApi capture_api{reinterpret_cast<c::Construct>(image+0x9667c0),reinterpret_cast<c::Construct>(image+0x9676a0),reinterpret_cast<c::Construct>(image+0x89e580),reinterpret_cast<c::Construct>(image+0x89e5f0),reinterpret_cast<c::PrepareSave>(image+0xd55ac0),&save_prepared};
            struct ImageScope{uintptr_t old;ImageScope(uintptr_t value):old(prepared_capture_image){prepared_capture_image=value;}~ImageScope(){prepared_capture_image=old;}} image_scope(image);
            ok=c::capture_original(capture_api,reinterpret_cast<const void*>(temporary),&inspect,&borrow,inspected.original,e)&&release_graph::same(loaded,desired);
            if(!ok&&e.empty())e="Release original temporary prepared demand value differs from Host";
        }
        auto exchange=[&](uintptr_t address,uint64_t expected,uint64_t desired,bool& stored,std::string& cause){receipt.exception=exchange_current(address,expected,desired,stored);if(receipt.exception){cause="Release prepared storage CAS SEH="+std::to_string(receipt.exception);return false;}return true;};
        auto current=[&](std::string& cause){return guard(cause)&&release_current_owner::read(read,image,manager,old,cause);};
        if(ok)ok=release_demand_owner::transfer(read,exchange,current,image,manager+0x70,temporary,old_storage,fresh_storage,receipt.prepared_storage,e);
    }
    if(ok){release_graph::Value actual;Receipt readback;ok=capture(image,memory,manager,key,actual,readback,e,desired.kind)&&release_graph::same(actual,desired)&&guard(e);receipt.readback_confirmed=ok;if(!ok&&e.empty())e="Release whole original graph readback differs from Host";}
    if(ok&&!prepared){receipt.exception=invoke(image,3,old,0,0);ok=receipt.exception==0;receipt.old_release_retired=ok;if(!ok)e="Release original previous owner destructor SEH="+std::to_string(receipt.exception);else ok=release_current_owner::read(read,image,manager,scope.fresh,e);}
    if(ok&&prepared){receipt.exception=invoke(image,5,temporary,0,0);ok=receipt.exception==0;
        if(!ok)e="Release original previous prepared demand owner destructor SEH="+std::to_string(receipt.exception);else receipt.retained_array=0;}
    if(!ok){receipt.unknown=true;receipt.retained_context=receipt.context_closed?0:uintptr_t(context.release());receipt.retained_record=tree.root;return false;}
    if(!c::first_append::destroy_records(api,tree,receipt.records,e)){receipt.unknown=true;receipt.retained_record=tree.root;return false;}
    receipt.confirmed=true;e.clear();return true;
}
bool apply(uintptr_t image,c::Memory& memory,uintptr_t manager,const release_graph::Value& desired,const Key& key,const Resolve& resolve,const Guard& guard,ApplyReceipt& receipt,std::string& e){
    try{return apply_owned(image,memory,manager,desired,key,resolve,guard,receipt,e);}
    catch(const std::exception& ex){receipt.unknown|=receipt.load_attempted;e="Release owned graph Apply C++: "+std::string(ex.what());return false;}
    catch(...){receipt.unknown|=receipt.load_attempted;e="Release owned graph Apply unknown C++ exception";return false;}
}
bool capture(uintptr_t image,c::Memory& memory,uintptr_t manager,const Key& key,release_graph::Value& out,Receipt& receipt,std::string& e,release_graph::Kind kind){
    out={};receipt={};uintptr_t ap=0;if(!manager||!key||!memory.read(manager,&ap,sizeof ap,e)||ap!=image+0x1714ed0){if(e.empty())e="Release original Record capture current Manager/class differs";return false;}
    uintptr_t current=0;auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){if(!object||object>UINTPTR_MAX-offset){cause="Release capture current ownership address overflow";return false;}return memory.read(object+offset,&value,sizeof value,cause);};
    if(!read(manager,0x38,current,e)||!release_current_owner::read(read,image,manager,current,e))return false;
    if(kind!=release_graph::Kind::Manager&&kind!=release_graph::Kind::PreparedDemands)return failure(e,"Release capture typed native value kind invalid");
    const bool prepared=kind==release_graph::Kind::PreparedDemands;
    const c::CaptureApi api{reinterpret_cast<c::Construct>(image+0x9667c0),reinterpret_cast<c::Construct>(image+0x9676a0),reinterpret_cast<c::Construct>(image+0x89e580),reinterpret_cast<c::Construct>(image+0x89e5f0),reinterpret_cast<c::PrepareSave>(image+(prepared?0xd55ac0:0xfa1690)),prepared?&save_prepared:reinterpret_cast<c::SaveRecord>(image+0xfa16f0)};
    struct ImageScope{uintptr_t old;ImageScope(uintptr_t value):old(prepared_capture_image){prepared_capture_image=value;}~ImageScope(){prepared_capture_image=old;}} image_scope(image);
    Borrow borrow{memory,key,out,kind};if(!c::capture_original(api,reinterpret_cast<const void*>(prepared?manager+0x70:manager),&inspect,&borrow,receipt.original,e))return false;
    receipt.confirmed=true;e.clear();return true;
}
}
