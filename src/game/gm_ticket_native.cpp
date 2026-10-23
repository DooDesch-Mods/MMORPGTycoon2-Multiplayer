#include "gm_ticket_native.h"
#include "live_host_state/memory_query_timing.h"
#include <Windows.h>
#include <atomic>
#include <memory>
#include <intrin.h>

namespace mt2mp::game::gm_ticket::native {
namespace {
namespace c=subscriber_lifecycle;
struct Borrow {c::Memory& memory;const Key& key;release_graph::Value& value;};
bool inspect(void* user,const void* record,const void* context,std::string& error){auto& scope=*static_cast<Borrow*>(user);c::Graph graph;
    {
        live_host_state::apply_timing::PageReadScope record_pages;
        if(!c::read_graph(scope.memory,uintptr_t(record),uintptr_t(context),{release_graph::maximum_records,release_graph::maximum_tokens,release_graph::maximum_text,release_graph::maximum_links},graph,error))return false;
    }
    return release_graph::capture_value(graph,scope.key,scope.value,error,release_graph::Kind::GameMasterTickets);
}
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
struct Scope {
    DWORD thread=0;uintptr_t context=0;const Resolve* resolve=nullptr;const Guard* guard=nullptr;
    c::LookupScope links;bool failed=false;std::string cause;
};
std::atomic<Scope*> active{nullptr};
bool fresh(void* user,const c::ExternalLink& link,std::string& error){auto& scope=*static_cast<Scope*>(user);uintptr_t object=0;
    if(!(*scope.guard)(error)||!(*scope.resolve)(link.portable_key,object,error)||!object||object!=link.object){scope.failed=true;scope.cause=error.empty()?"GM external current canonical owner changed during original Resolve":error;error=scope.cause;return false;}return true;
}
uint32_t invoke(uintptr_t image,unsigned step,uintptr_t object,uintptr_t record,uintptr_t context)noexcept{
    __try{switch(step){
    case 0:reinterpret_cast<void(*)(void*)>(image+0x89e940)(reinterpret_cast<void*>(context));break;
    case 1:reinterpret_cast<void(*)(void*,void*,void*)>(image+0xbbcc00)(reinterpret_cast<void*>(object),reinterpret_cast<void*>(record),reinterpret_cast<void*>(context));break;
    case 2:reinterpret_cast<void(*)(void*)>(image+0x89e480)(reinterpret_cast<void*>(context));break;
    case 3:reinterpret_cast<void(*)(void*)>(image+0xbbd590)(reinterpret_cast<void*>(object));break;
    }return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t create(uintptr_t image,uintptr_t& object)noexcept{
    __try{const auto secondary=uintptr_t(reinterpret_cast<void*(*)(const void*)>(image+0xd86680)(nullptr));object=secondary>=0x18?secondary-0x18:0;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t exchange(uintptr_t address,uint64_t expected,uint64_t desired,bool& stored)noexcept{
    __try{stored=uint64_t(InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(address),LONG64(desired),LONG64(expected)))==expected;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool pointers(c::Memory& memory,const storage::Value& storage,std::vector<uintptr_t>& out,std::string& error){out.clear();const auto count=int32_t(uint32_t(storage.extent));
    for(int32_t index=0;index<count;++index){uintptr_t object=0;if(!memory.read(storage.data+size_t(index)*8,&object,8,error))return false;
        if(!object)return fail(error,"GM actual owning Ticket array contains a null item");out.push_back(object);}return true;
}
bool apply_owned(uintptr_t image,c::Memory& memory,uintptr_t queue,const release_graph::Value& desired,const Key& key,const Resolve& resolve,const Guard& guard,const Adopt& adopt,ApplyReceipt& receipt,std::string& error){
    receipt={};if(desired.kind!=release_graph::Kind::GameMasterTickets||!key||!resolve||!guard||!adopt)return fail(error,"GM original Ticket apply requires its exact typed value and native owner callbacks");
    if(!release_graph::valid(desired,error)||!guard(error))return false;
    if(active.load(std::memory_order_acquire))return fail(error,"GM original Ticket Context is already active");
    auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){if(!object||object>UINTPTR_MAX-offset)return fail(cause,"GM original owner address overflow");return memory.read(object+offset,&value,sizeof value,cause);};
    uintptr_t singleton=0,ap=0;if(!read(image,0x13df360,singleton,error)||!read(queue,0,ap,error))return false;
    if(queue!=singleton||ap!=image+0x16fdc60)return fail(error,"GM original Ticket apply current Queue singleton/class differs");
    storage::Value previous;if(!storage::read(read,image,queue+0x18,previous,error))return false;receipt.previous_storage=previous;
    std::vector<uintptr_t> old_objects;if(!pointers(memory,previous,old_objects,error))return false;
    c::first_append::DecodeApi api;if(!c::first_append::resolve_record_api(memory,image,api,error))return false;
    c::first_append::NativeTree tree;
    // Preserve the Ticket ObjectArray RTTI root and every original typed token.
    if(!c::first_append::decode_release_record(api,memory,desired.records,tree,receipt.records,error,release_graph::Kind::GameMasterTickets)){
        const auto first=error;std::string cleanup;if(!receipt.records.unknown&&!c::first_append::destroy_records(api,tree,receipt.records,cleanup))error=first+"; "+cleanup;return false;}
    auto context=std::make_unique<uintptr_t>(0);Scope scope;scope.thread=GetCurrentThreadId();scope.context=uintptr_t(context.get());scope.resolve=&resolve;scope.guard=&guard;
    struct Lifetime {Scope& scope;std::unique_ptr<uintptr_t>& context;c::first_append::NativeTree& tree;ApplyReceipt& receipt;
        ~Lifetime(){auto expected=&scope;active.compare_exchange_strong(expected,nullptr);scope.links.clear();
            if(!receipt.confirmed&&(receipt.load_attempted||(receipt.context_constructed&&!receipt.context_closed)||receipt.retained_array)){
                receipt.unknown=true;if(!receipt.context_closed&&context)receipt.retained_context=uintptr_t(context.release());receipt.retained_record=tree.root;}}
    } lifetime{scope,context,tree,receipt};
    std::vector<c::ExternalLink> external;for(const auto& link:desired.external){uintptr_t object=0;
        if(!resolve(link.key,object,error)||!object){if(error.empty())error="GM Ticket external canonical owner is absent";const auto first=error;std::string cleanup;if(!c::first_append::destroy_records(api,tree,receipt.records,cleanup))error=first+"; "+cleanup;return false;}external.push_back({link.id,link.key,object});}
    receipt.exception=invoke(image,0,0,0,scope.context);
    if(receipt.exception){receipt.unknown=true;receipt.retained_context=uintptr_t(context.release());receipt.retained_record=tree.root;return fail(error,"GM original Ticket Context constructor SEH="+std::to_string(receipt.exception));}receipt.context_constructed=true;
    if(!scope.links.arm(scope.thread,context.get(),release_graph::inline_ids(desired),std::move(external),&fresh,&scope,error)){
        const auto first=error;receipt.exception=invoke(image,2,0,0,scope.context);receipt.context_closed=receipt.exception==0;
        if(receipt.exception)return fail(error,first+"; GM empty Context cleanup SEH="+std::to_string(receipt.exception));std::string cleanup;if(!c::first_append::destroy_records(api,tree,receipt.records,cleanup))error=first+"; "+cleanup;return false;}
    uintptr_t temporary=0;receipt.exception=create(image,temporary);receipt.retained_array=temporary;storage::Value loaded;
    if(receipt.exception||!temporary)return fail(error,"GM original Ticket ObjectArray factory SEH="+std::to_string(receipt.exception));
    if(!storage::read(read,image,temporary,loaded,error))return false;
    active.store(&scope,std::memory_order_release);receipt.load_attempted=true;
    receipt.exception=invoke(image,1,temporary,tree.root,scope.context);receipt.load_returned=receipt.exception==0;
    if(receipt.exception)return fail(error,"GM original Ticket ObjectArray Load SEH="+std::to_string(receipt.exception));
    receipt.exception=invoke(image,2,0,0,scope.context);receipt.context_closed=receipt.exception==0;
    if(receipt.exception)return fail(error,"GM original Ticket Context Resolve/destructor SEH="+std::to_string(receipt.exception));
    receipt.lookup=scope.links.receipt();active.store(nullptr,std::memory_order_release);scope.links.clear();
    if(scope.failed||receipt.lookup.failed)return fail(error,!scope.cause.empty()?scope.cause:receipt.lookup.cause);
    if(!guard(error)||!storage::read(read,image,temporary,loaded,error))return false;receipt.fresh_storage=loaded;
    release_graph::Value inspected;CaptureReceipt original;
    // Capture the detached original array before it can replace the current one.
    Borrow borrow{memory,key,inspected};const c::CaptureApi capture_api{
        reinterpret_cast<c::Construct>(image+0x9667c0),reinterpret_cast<c::Construct>(image+0x9676a0),reinterpret_cast<c::Construct>(image+0x89e580),reinterpret_cast<c::Construct>(image+0x89e5f0),
        reinterpret_cast<c::PrepareSave>(image+0xd7cdd0),reinterpret_cast<c::SaveRecord>(image+0xd7ce70)};
    if(!c::capture_original(capture_api,reinterpret_cast<const void*>(temporary),&inspect,&borrow,original.original,error))return false;
    if(inspected!=desired)return fail(error,"GM detached original Ticket ObjectArray differs from Host typed graph");
    std::vector<uintptr_t> fresh_objects;if(!pointers(memory,loaded,fresh_objects,error))return false;
    auto swap=[&](uintptr_t address,uint64_t expected,uint64_t desired,bool& stored,std::string& cause){receipt.exception=exchange(address,expected,desired,stored);if(receipt.exception)return fail(cause,"GM Ticket storage CAS SEH="+std::to_string(receipt.exception));return true;};
    if(!storage::transfer(read,swap,guard,image,queue+0x18,temporary,previous,loaded,receipt.storage_transfer,error))return false;
    if(!adopt(fresh_objects,old_objects,error))return false;receipt.adopted=true;
    release_graph::Value actual;CaptureReceipt actual_receipt;if(!capture(image,memory,queue,key,actual,actual_receipt,error))return false;
    if(actual!=desired||!guard(error))return fail(error,error.empty()?"GM installed original Ticket graph differs from Host":error);receipt.readback_confirmed=true;
    receipt.exception=invoke(image,3,temporary,0,0);if(receipt.exception)return fail(error,"GM previous owning Ticket array destructor SEH="+std::to_string(receipt.exception));
    receipt.previous_retired=true;receipt.retained_array=0;
    if(!c::first_append::destroy_records(api,tree,receipt.records,error))return false;
    receipt.confirmed=true;error.clear();return true;
}
}
bool lookup(void* context,int32_t id,c::Lookup original,void*& result){auto* scope=active.load(std::memory_order_acquire);
    if(!scope||scope->thread!=GetCurrentThreadId()||scope->context!=uintptr_t(context))return false;
    result=scope->links.lookup(GetCurrentThreadId(),context,id,original);return true;
}
bool capture(uintptr_t image,c::Memory& memory,uintptr_t queue,const Key& key,release_graph::Value& out,CaptureReceipt& receipt,std::string& error){
    out={};receipt={};if(!key)return fail(error,"GM Ticket capture requires its canonical owner callback");
    uintptr_t singleton=0,ap=0;if(!memory.read(image+0x13df360,&singleton,8,error)||!memory.read(queue,&ap,8,error))return false;
    if(!queue||queue!=singleton||ap!=image+0x16fdc60)return fail(error,"GM Ticket capture current Queue singleton/class differs");
    const c::CaptureApi api{reinterpret_cast<c::Construct>(image+0x9667c0),reinterpret_cast<c::Construct>(image+0x9676a0),reinterpret_cast<c::Construct>(image+0x89e580),reinterpret_cast<c::Construct>(image+0x89e5f0),
        reinterpret_cast<c::PrepareSave>(image+0xd7cdd0),reinterpret_cast<c::SaveRecord>(image+0xd7ce70)};
    Borrow borrow{memory,key,out};if(!c::capture_original(api,reinterpret_cast<const void*>(queue+0x18),&inspect,&borrow,receipt.original,error))return false;
    receipt.confirmed=true;error.clear();return true;
}
bool apply(uintptr_t image,c::Memory& memory,uintptr_t queue,const release_graph::Value& value,const Key& key,const Resolve& resolve,const Guard& guard,const Adopt& adopt,ApplyReceipt& receipt,std::string& error){
    try{return apply_owned(image,memory,queue,value,key,resolve,guard,adopt,receipt,error);}
    catch(const std::exception& exception){receipt.unknown|=receipt.load_attempted||receipt.retained_array||(receipt.context_constructed&&!receipt.context_closed);return fail(error,"GM owning Ticket apply C++: "+std::string(exception.what()));}
    catch(...){receipt.unknown|=receipt.load_attempted||receipt.retained_array||(receipt.context_constructed&&!receipt.context_closed);return fail(error,"GM owning Ticket apply unknown C++ exception");}
}
uint32_t transform(uintptr_t image,uintptr_t object,const MasterValue& value)noexcept{
    alignas(4) std::array<unsigned char,0x6c> transform_bytes{};
    __try{reinterpret_cast<void(*)(void*,const float*,const float*,const float*)>(image+0x9ff370)(transform_bytes.data(),value.orientation.data(),value.position.data(),value.scale.data());
        reinterpret_cast<void(*)(uintptr_t,const void*)>(image+0xc67c80)(object+0x30,transform_bytes.data());return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t on_map(uintptr_t image,uintptr_t object,bool value)noexcept{
    __try{reinterpret_cast<void(*)(void*,bool)>(image+0x3b1c00)(reinterpret_cast<void*>(object),value);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t shard(uintptr_t image,uintptr_t object,uintptr_t root)noexcept{
    __try{reinterpret_cast<void(*)(void*,void*)>(image+0x3b1cb0)(reinterpret_cast<void*>(object),reinterpret_cast<void*>(root));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t spatial(uintptr_t image,uintptr_t object)noexcept{
    __try{reinterpret_cast<void(*)(void*)>(image+0x3b1b50)(reinterpret_cast<void*>(object));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t presentation(uintptr_t image,uintptr_t object,float delta)noexcept{
    __try{if(!*reinterpret_cast<const uint8_t*>(object+0x4c5)||!*reinterpret_cast<const uintptr_t*>(object+0x178))return 0;
        const auto arrow=*reinterpret_cast<const uintptr_t*>(object+0x498);
        if(!arrow||*reinterpret_cast<const uintptr_t*>(arrow)!=image+0x16fbbf0||*reinterpret_cast<const uintptr_t*>(image+0x16fbc00)!=image+0x1b6380)return ERROR_INVALID_DATA;
        reinterpret_cast<void(*)(uintptr_t,const float*)>(image+0x1b4910)(arrow,reinterpret_cast<const float*>(object+0x110));
        reinterpret_cast<void(*)(uintptr_t,float)>(image+0x1b6380)(arrow,delta);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t pool(uintptr_t image,uintptr_t lounge,int32_t count)noexcept{
    __try{reinterpret_cast<void(*)(void*,int32_t)>(image+0x2749d0)(reinterpret_cast<void*>(lounge),count);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t compare(uintptr_t address,unsigned bytes,uint64_t expected,uint64_t desired,bool& stored)noexcept{
    __try{if(bytes==1)stored=uint8_t(_InterlockedCompareExchange8(reinterpret_cast<volatile char*>(address),char(desired),char(expected)))==uint8_t(expected);
        else if(bytes==4)stored=uint32_t(InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address),LONG(desired),LONG(expected)))==uint32_t(expected);
        else if(bytes==8)stored=uint64_t(InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(address),LONG64(desired),LONG64(expected)))==expected;
        else return ERROR_INVALID_PARAMETER;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
