#include "dungeon_fill_native.h"
#include "dungeon_input_codes.h"
#include "subscriber_lifecycle/capture.h"
#include "subscriber_lifecycle/append_calls.h"
#include "mt2mp/core/hash.h"
#include <Windows.h>
namespace mt2mp::game::dungeon_fill {
namespace {
namespace capture=subscriber_lifecycle;namespace decode=capture::first_append;using json=nlohmann::json;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool digest(const json& value,std::string& out){const auto bytes=value.dump();out=mt2mp::sha256(std::vector<unsigned char>(bytes.begin(),bytes.end()));return true;}
json scalar(const capture::Token& token){return json::array({token.type,uint64_t(token.bits),token.text});}
struct Borrow {capture::Memory& memory;json records=json::array();};
bool inspect(void* user,const void* root,const void* context,std::string& e){
    auto& borrow=*static_cast<Borrow*>(user);capture::Graph graph;
    if(!capture::read_graph(borrow.memory,uintptr_t(root),uintptr_t(context),{2048,16384,65536,2048},graph,e))return false;
    if(!graph.private_links.empty())return fail(e,"Dungeon Fill detached graph contains a native object reference");
    for(const auto& row:graph.records){auto tokens=json::array();for(const auto& token:row.tokens)tokens.push_back(scalar(token));
        borrow.records.push_back(json::array({row.parent==SIZE_MAX?int64_t(-1):int64_t(row.parent),uint64_t(row.child_index),scalar(row.label),tokens}));}
    std::vector<capture::Record> validate;return records(borrow.records,validate,e);
}
struct Invoke {uintptr_t image=0;unsigned step=0;uintptr_t object=0,record=0,context=0;};
uint32_t invoke(Invoke* call){
    __try{switch(call->step){
        case 0:call->object=uintptr_t(reinterpret_cast<void*(*)()>(call->image+0xd96100)());break;
        case 1:break;
        case 2:reinterpret_cast<void(*)(void*)>(call->image+0x89e940)(reinterpret_cast<void*>(call->context));break;
        case 3:reinterpret_cast<void(*)(void*,void*,void*)>(call->image+0xce7af0)(reinterpret_cast<void*>(call->object),reinterpret_cast<void*>(call->record),reinterpret_cast<void*>(call->context));break;
        case 4:reinterpret_cast<void(*)(void*)>(call->image+0x89e480)(reinterpret_cast<void*>(call->context));break;
        case 5:reinterpret_cast<void(*)(void*)>(call->image+0x157f70)(reinterpret_cast<void*>(call->object));break;
    }return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool read(uintptr_t image,capture::Memory& memory,uintptr_t object,State& state,json& records,std::string& e){
    state={};records=json::array();if(!dungeon_input_codes::verify(image,memory,e))return false;if(!object){digest(nullptr,state.sha256);e.clear();return true;}
    uintptr_t ap=0;if(!memory.read(object,&ap,sizeof ap,e)||ap!=image+0x1743680)return fail(e,e.empty()?"Dungeon Fill actual native class differs":e);
    const capture::CaptureApi api{reinterpret_cast<capture::Construct>(image+0x9667c0),reinterpret_cast<capture::Construct>(image+0x9676a0),reinterpret_cast<capture::Construct>(image+0x89e580),reinterpret_cast<capture::Construct>(image+0x89e5f0),reinterpret_cast<capture::PrepareSave>(image+0x10ee6f0),reinterpret_cast<capture::SaveRecord>(image+0x10ee750)};
    Borrow borrow{memory};capture::CaptureReceipt receipt;
    if(!capture::capture_original(api,reinterpret_cast<void*>(object),&inspect,&borrow,receipt,e))return false;
    state.present=true;records=std::move(borrow.records);digest(records,state.sha256);e.clear();return true;
}
bool create(uintptr_t image,capture::Memory& memory,const Intent& intent,Owned& owned,std::string& e){
    if(!dungeon_input_codes::verify(image,memory,e))return false;
    if(owned.pointer||owned.unknown||!valid(intent))return fail(e,"Dungeon Fill detached allocation requires empty owner and valid intent");
    std::string expected;digest(intent.records,expected);if(expected!=intent.desired_sha256)return fail(e,"Dungeon Fill detached value SHA differs from captured intent");
    decode::DecodeApi api;if(!decode::resolve_record_api(memory,image,api,e))return false;
    std::vector<capture::Record> records;if(!dungeon_fill::records(intent.records,records,e))return false;
    decode::NativeTree tree;decode::DecodeReceipt receipt;
    if(!decode::decode_dungeon_fill_record(api,memory,records,tree,receipt,e)){
        const auto cause=e;std::string cleanup;if(!receipt.unknown&&!decode::destroy_records(api,tree,receipt,cleanup))e=cause+"; "+cleanup;return false;}
    Invoke call{image};auto step=[&](unsigned op){call.step=op;const auto seh=invoke(&call);if(seh){owned.unknown=true;e="Dungeon Fill original detached allocation/load/resolve SEH="+std::to_string(seh)+" step="+std::to_string(op);return false;}return true;};
    bool ok=step(0);owned.pointer=call.object;owned.constructed=ok&&owned.pointer!=0;
    if(ok&&!owned.pointer){e="Dungeon Fill original allocation returned null";ok=false;}
    alignas(8) uintptr_t context=0;call.context=uintptr_t(&context);call.record=tree.root;
    bool context_ready=false;if(ok){ok=step(2);context_ready=ok;}
    if(ok)ok=step(3);
    if(context_ready&&!owned.unknown){const auto cause=e;const bool cleaned=step(4);if(!cleaned&&!cause.empty())e=cause+"; "+e;ok=ok&&cleaned;}
    const auto cause=e;std::string cleanup;if(!decode::destroy_records(api,tree,receipt,cleanup)){e=cause+(cause.empty()?"":"; ")+cleanup;owned.unknown=true;return false;}
    if(!ok)return false;
    State after;json actual;
    if(!read(image,memory,owned.pointer,after,actual,e)||!after.present||after.sha256!=intent.desired_sha256||actual!=intent.records)return fail(e,e.empty()?"Dungeon Fill detached original load/Resolve readback differs":e);
    e.clear();return true;
}
bool discard(uintptr_t image,Owned& owned,std::string& e){
    if(!owned.pointer){e.clear();return true;}if(owned.unknown||owned.adopted||!owned.constructed)return fail(e,"Dungeon Fill cleanup cannot release adopted, partial or Unknown native owner");
    Invoke call{image,5,owned.pointer};const auto seh=invoke(&call);if(seh){owned.unknown=true;return fail(e,"Dungeon Fill original owning destructor SEH="+std::to_string(seh));}
    owned={};e.clear();return true;
}
}
