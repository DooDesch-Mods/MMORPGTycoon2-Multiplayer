#include "release_graph.h"
#include "release_boundary_owner.h"
#include "release_boundary_frames.h"
#include "release_reference_reader.h"
#include "release_current_owner.h"
#include "release_demand_owner.h"
#include <iostream>
#include <stdexcept>
namespace graph=mt2mp::game::release_graph;
namespace boundary=mt2mp::game::release_boundary;
namespace {
unsigned checks=0;void check(bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(why);}
template<class Body>bool rejects(Body body){try{body();return false;}catch(const std::exception&){return true;}}
struct Framework {
    MT2FW_MT2_ReleaseDelegate callback=nullptr;void* user=nullptr;MT2FW_MT2_ReleaseReceipt receipt{};bool busy=false;
    Framework(){receipt.size=sizeof receipt;}
    static MT2FW_MT2_Result MT2FW_CALL bind(void* context,MT2FW_MT2_ReleaseDelegate fn,void* user,MT2FW_MT2_ReleaseReceipt* out){auto& f=*static_cast<Framework*>(context);
        f.callback=fn;f.user=user;f.receipt.bound=1;f.receipt.registration=71;std::strcpy(f.receipt.owner,"doodesch.multiplayer");*out=f.receipt;return MT2FW_MT2_OK;}
    static MT2FW_MT2_Result MT2FW_CALL read(void* context,MT2FW_MT2_ReleaseReceipt* out){*out=static_cast<Framework*>(context)->receipt;return MT2FW_MT2_OK;}
    static MT2FW_MT2_Result MT2FW_CALL unbind(void* context,uint64_t token,MT2FW_MT2_ReleaseReceipt* out){auto& f=*static_cast<Framework*>(context);
        if(token!=f.receipt.registration)return MT2FW_MT2_INVALID;if(f.busy){f.receipt.bound=0;f.receipt.inflight=1;*out=f.receipt;return MT2FW_MT2_BUSY;}
        f.callback=nullptr;f.user=nullptr;f.receipt={};f.receipt.size=sizeof f.receipt;*out=f.receipt;return MT2FW_MT2_OK;}
    MT2FW_MT2_ReleaseV1 api(){return {sizeof(MT2FW_MT2_ReleaseV1),1,this,bind,unbind,read};}
    MT2FW_MT2_ReleaseCall call(){return {sizeof(MT2FW_MT2_ReleaseCall),1,0,0,1,71,91,reinterpret_cast<void*>(0x100),reinterpret_cast<void*>(0x200),0x144faf6a,"LevelUp",7};}
    MT2FW_MT2_ReleaseDecision invoke(const MT2FW_MT2_ReleaseCall& call){receipt.last_invocation=call.invocation;receipt.inflight=1;MT2FW_MT2_ReleaseDecision out{};out.size=sizeof out;check(callback&&callback(user,&call,&out)==MT2FW_MT2_OK,"actual consumer delegate result");receipt.inflight=0;return out;}
};
}
int main(){try{
    graph::Value value;value.records={{SIZE_MAX,0,{1,0,"mmoReleaseManager"},{}},{0,0,{1,0,"linkId"},{{4,1,{}}}},{0,1,{1,0,"current"},{{4,2,{}}}},{2,0,{1,0,"mmoRelease"},{}},{3,0,{1,0,"linkId"},{{4,2,{}}}},{3,1,{1,0,"title"},{{2,0,"Native {title}"}}},{3,2,{1,0,"rating"},{{3,0x3f800000,{}}}}};value.external={{3,"release:tech:test"}};
    std::string e;check(graph::valid(value,e),"actual original scalar graph domain");check(graph::same(graph::decode(graph::encode(value)),value),"full graph codec roundtrip");
    check(graph::inline_ids(value)==std::vector<int32_t>({1,2}),"inline native Context IDs come from actual linkId definitions");
    auto changed=value;changed.external[0].id=2;check(!graph::valid(changed,e),"external link cannot alias inline native allocation");
    changed=value;changed.records[4].tokens[0].bits=1;check(!graph::valid(changed,e),"duplicate inline definitions reject");
    changed=value;changed.external.push_back({4,changed.external[0].key});check(!graph::valid(changed,e),"canonical external object cannot have two saved IDs");
    changed=value;changed.records[5].parent=5;check(!graph::valid(changed,e),"forward/cyclic Record parent rejects");
    changed=value;changed.records[6].tokens[0].bits=0x7fc00000;check(!graph::valid(changed,e),"nonfinite active Release float rejects");
    auto wire=graph::encode(value);wire["records"][6][3][0][1]=true;check(rejects([&]{graph::decode(wire);}),"wire bool cannot replace exact float bits");
    wire=graph::encode(value);wire["records"][0][0]=UINT64_MAX;check(rejects([&]{graph::decode(wire);}),"unsigned native parent overflow rejects");
    wire=graph::encode(value);wire["external"][0][0]=0;check(rejects([&]{graph::decode(wire);}),"null external object ID rejects");
    wire=graph::encode(value);wire["external"][0].push_back(0);check(rejects([&]{graph::decode(wire);}),"wire extra tuple field rejects");
    wire=graph::encode(value);wire["extra"]=1;check(rejects([&]{graph::decode(wire);}),"wire extra root field rejects");
    mt2mp::subscriber_lifecycle::Graph actual;actual.records=value.records;actual.private_links={{1,0x100,true},{2,0x200,true},{3,0x300,false}};
    graph::Value captured;unsigned resolved=0;
    check(graph::capture_value(actual,[&](uintptr_t object,std::string& key,std::string&){++resolved;check(object==0x300,"only external actual pointer reaches canonical resolver");key="release:tech:test";return true;},captured,e)&&resolved==1&&graph::same(captured,value),"capture retains complete graph and canonical external mapping");
    check(!graph::capture_value(actual,[](uintptr_t,std::string&,std::string& e){e="exact full UID no longer current";return false;},captured,e)&&e=="exact full UID no longer current","native canonical binding first cause retained");
    {
        namespace current=mt2mp::game::release_current_owner;
        const uintptr_t image=0x140000000,manager=0x20000,old=0x30000,fresh=0x40000,slot=image+current::current_rva;
        std::map<uintptr_t,uintptr_t> memory{{image+current::got_rva,slot},{slot,old},{manager,image+current::manager_ap},{manager+0x38,old},{old,image+current::release_ap},{fresh,image+current::release_ap}};
        auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){const auto found=memory.find(object+offset);if(found==memory.end()){cause="fixture exact pointer missing";return false;}value=found->second;return true;};
        unsigned exchanges=0;bool concurrent=false;
        auto exchange=[&](uintptr_t address,uintptr_t expected,uintptr_t desired,bool& stored,std::string&){++exchanges;if(concurrent)memory[address]=0x99999;stored=memory[address]==expected;if(stored)memory[address]=desired;return true;};
        current::Receipt receipt;
        check(current::read(read,image,manager,old,e),"actual initial static current/Manager reciprocity");
        memory[manager+0x38]=fresh;
        check(current::adopt(read,exchange,image,manager,old,fresh,receipt,e)&&receipt.attempted&&receipt.stored&&receipt.confirmed&&memory[slot]==fresh&&exchanges==1,"actual private load retains native Init static-current store through checked CAS");
        check(current::read(read,image,manager,fresh,e),"new current owner remains reciprocal before old destructor");
        memory[slot]=old;concurrent=true;check(!current::adopt(read,exchange,image,manager,old,fresh,receipt,e)&&receipt.attempted&&!receipt.stored&&!receipt.confirmed&&exchanges==2,"concurrent static-current replacement fails without success-shaped readback");concurrent=false;
        memory[slot]=old;memory[image+current::got_rva]=slot+8;check(!current::adopt(read,exchange,image,manager,old,fresh,receipt,e)&&!receipt.attempted&&exchanges==2,"wrong actual initialized GOT target rejects before mutation");memory[image+current::got_rva]=slot;
        memory[fresh]=image+0x1715450;check(!current::adopt(read,exchange,image,manager,old,fresh,receipt,e)&&!receipt.attempted&&exchanges==2,"foreign loaded object class rejects before mutation");memory[fresh]=image+current::release_ap;
        memory[manager+0x38]=old;check(!current::adopt(read,exchange,image,manager,old,fresh,receipt,e)&&!receipt.attempted&&exchanges==2,"loader must have installed the exact fresh object before static pointer adoption");
    }
    {
        namespace references=mt2mp::game::release_references;
        std::map<uintptr_t,std::string> fields;std::map<uintptr_t,std::string> texts;
        const auto put=[&](uintptr_t base,size_t offset,auto value){fields[base+offset]=std::string(reinterpret_cast<const char*>(&value),sizeof value);};
        const auto read=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){const auto found=fields.find(base+offset);if(found==fields.end()||found->second.size()!=sizeof value){cause="fixture exact field missing";return false;}std::memcpy(&value,found->second.data(),sizeof value);return true;};
        const auto text=[&](uintptr_t address,std::string& value,std::string& cause){const auto found=texts.find(address);if(found==texts.end()){cause="fixture exact text missing";return false;}value=found->second;return true;};
        const uintptr_t image=0x140000000,manager=0x20000,state=0x30000;
        put(image,0x13df7a0,manager);put(state,0x168,manager);put(manager,0,image+0x1732430);
        for(int32_t kind=0;kind<5;++kind){const auto offset=0x80+size_t(kind)*0x50;const uintptr_t data=0x40000+uintptr_t(kind)*0x1000,object=0x50000+uintptr_t(kind)*0x1000;
            put(manager,offset+8,data);put(manager,offset+0x10,int32_t(1));put(manager,offset+0x14,int32_t(1));put(data,0,object);
            put(object,0,image+0x1705990);put(object,0x18,uint64_t(100+kind));put(object,0x2c,kind);put(object,0x30,int32_t(0));put(object,0x4a0,uintptr_t(0));put(object,0x498,uintptr_t(0));put(object,0x2c8,uintptr_t(0));
            for(size_t flag:{size_t(0x490),size_t(0x491),size_t(0x492)})put(object,flag,uint8_t(0));texts[object+0xb0]="name";texts[object+0x248]="definition";
        }
        references::Types first,second;std::map<uintptr_t,std::string> keys;
        check(references::base_types(read,text,image,state,first,e)&&references::base_types(read,text,image,state,second,e)&&references::add_base_keys(first,second,keys,e)&&keys.size()==5,"actual five base CharacterType census yields canonical kind/id references");
        check(keys.at(0x50000)==references::type_key(0,0),"Character owner key is typed canonical kind/id, not mutable name");
        auto changed=second;changed.rows[0].uid++;keys.clear();check(!references::add_base_keys(first,changed,keys,e)&&keys.empty(),"changed local full UID rejects before portable mapping");
        put(0x50000,0x490,uint8_t(1));put(0x50000,0x18,uint64_t(0));keys.clear();
        check(references::base_types(read,text,image,state,first,e)&&references::base_types(read,text,image,state,second,e)&&references::add_base_keys(first,second,keys,e)&&keys.size()==4&&!keys.contains(0x50000),"deleted original UID0 slot remains a census tombstone without a live Release reference");
        put(0x50000,0x490,uint8_t(0));put(0x50000,0x18,uint64_t(100));
        put(0x50000,0x4a0,uintptr_t(0x999));check(!references::base_types(read,text,image,state,second,e)&&e.find("parent")!=std::string::npos,"unbound variant parent cannot masquerade as a current global definition");put(0x50000,0x4a0,uintptr_t(0));
        put(0x50000,0x30,int32_t(4));check(!references::base_types(read,text,image,state,second,e),"wrong current native pool id rejects");put(0x50000,0x30,int32_t(0));
        put(0x51000,0x18,uint64_t(100));check(!references::base_types(read,text,image,state,second,e),"duplicate current full UID rejects");put(0x51000,0x18,uint64_t(101));
        put(state,0x168,uintptr_t(0x20100));check(!references::base_types(read,text,image,state,second,e),"foreign native State manager rejects");
    }
    {
        namespace owner=mt2mp::game::release_demand_owner;
        const uintptr_t image=0x140000000,destination=0x4000,temporary=0x5000;
        const owner::Storage old{0x6000,uint64_t(3)|(uint64_t(4)<<32)},fresh{0x7000,uint64_t(5)|(uint64_t(8)<<32)};
        const auto initial=[&]{return std::map<uintptr_t,uint64_t>{{destination,image+0x16e2100},{destination+8,old.data},{destination+0x10,old.extent},{temporary,image+0x16ebf20},{temporary+0x18,image+0x16ebf88},{temporary+8,fresh.data},{temporary+0x10,fresh.extent}};};
        auto memory=initial();unsigned calls=0,fail_at=0;
        auto read=[&](uintptr_t base,size_t offset,auto& out,std::string& cause){const auto found=memory.find(base+offset);if(found==memory.end()){cause="fixture exact demand storage field missing";return false;}out=found->second;return true;};
        auto exchange=[&](uintptr_t address,uint64_t expected,uint64_t desired,bool& stored,std::string&){++calls;stored=calls!=fail_at&&memory.at(address)==expected;if(stored)memory[address]=desired;return true;};
        auto guard=[](std::string&){return true;};owner::Receipt receipt;
        check(owner::transfer(read,exchange,guard,image,destination,temporary,old,fresh,receipt,e)&&receipt.confirmed&&receipt.stored==4,"actual checked ArrayStore ownership transfer commits four fields");
        check(memory.at(destination)==image+0x16e2100&&memory.at(temporary)==image+0x16ebf20&&memory.at(temporary+0x18)==image+0x16ebf88,"native object/base classes stay in their actual owners");
        check(memory.at(temporary+8)==old.data&&memory.at(destination+8)==fresh.data,"old storage remains owned by original temporary D0, new storage by manager");
        for(unsigned failed=1;failed<=4;++failed){memory=initial();calls=0;fail_at=failed;check(!owner::transfer(read,exchange,guard,image,destination,temporary,old,fresh,receipt,e)&&!receipt.confirmed&&receipt.stored==failed-1&&calls==failed,"partial CAS stops at first failure without replay/cleanup claim");}
        memory=initial();calls=0;fail_at=0;memory[temporary+0x18]=0;check(!owner::transfer(read,exchange,guard,image,destination,temporary,old,fresh,receipt,e)&&!calls,"wrong original secondary owner rejects before transfer");
        memory=initial();memory[destination+0x10]=UINT64_MAX;check(!owner::transfer(read,exchange,guard,image,destination,temporary,old,fresh,receipt,e)&&!calls,"invalid native count/capacity rejects before transfer");
    }
    using O=boundary::Origin;
    check(boundary::origin(0x4faf6a,{0x4faf6a,0x4fc025,0x4e6d0c})==O::Finish,"real MessageTo tail chain Finish");
    check(boundary::origin(0x4faf6a,{0x4faf6a,0x4fc025,0x4e6dec})==O::Skip,"real MessageTo tail chain Skip");
    check(boundary::origin(0x4faf6a,{0x4faf6a,0x4fc025,0x4e6f53})==O::Begin,"real MessageTo tail chain Begin");
    check(boundary::origin(0x4faf6a,{0x4faf6a,0x4fc025,0x79be1f})==O::Immediate,"real HUD immediate chain");
    check(boundary::origin(0x4faf6a,{0x4e6d0c})==O::Other,"outer caller alone is not immediate provenance");
    check(boundary::origin(0x4faf6a,{0x4faf6a,0x4fc025,0x123,0x4e6d0c})==O::Other,"arbitrary later matching frame cannot prove Release origin");
    check(boundary::origin(0x4e6d0c,{0x4faf6a,0x4fc025,0x4e6d0c})==O::Other,"nested plugin return cannot replace physical Mode caller");
    Framework framework;boundary::Owner owner;unsigned captures=0;
    auto capture=[&](const auto&,uint32_t& disposition,std::string&){++captures;disposition=MT2FW_MT2_RELEASE_SUPPRESS;return true;};
    check(owner.bind(framework.api(),91,capture,e),"exact private V1 bind receipt");auto call=framework.call();check(framework.invoke(call).disposition==MT2FW_MT2_RELEASE_SUPPRESS&&captures==1,"actual consumer suppress decision without original call");
    check(framework.invoke(call).disposition==MT2FW_MT2_RELEASE_FAILED&&captures==1&&owner.failed(),"duplicate invocation fails closed without replay");
    call.invocation=2;check(framework.invoke(call).disposition==MT2FW_MT2_RELEASE_FAILED&&captures==1,"first uncertain cause remains latched");
    framework.busy=true;check(!owner.unbind(e)&&owner.registration()==71,"inflight SDK retirement retained for exact retry");framework.busy=false;check(owner.unbind(e)&&!owner.registration(),"actual SDK unbind returns cleared metadata/token");
    check(owner.bind(framework.api(),91,[](const auto&,uint32_t& out,std::string&){out=MT2FW_MT2_RELEASE_FORWARD;return true;},e),"explicit fresh metadata bind after completed retirement");
    call=framework.call();check(framework.invoke(call).disposition==MT2FW_MT2_RELEASE_FORWARD,"known unrelated command can forward exactly once");check(owner.unbind(e),"forward owner retires");
    check(owner.bind(framework.api(),91,capture,e),"owner rebound for actual stale native guard");call=framework.call();call.thread=92;check(framework.invoke(call).disposition==MT2FW_MT2_RELEASE_FAILED&&captures==1,"wrong actual native thread rejects before capture");check(owner.unbind(e),"failed registration explicitly unbound");
    check(owner.bind(framework.api(),91,capture,e),"owner rebound for real registration readback");framework.receipt.registration=72;call=framework.call();check(framework.invoke(call).disposition==MT2FW_MT2_RELEASE_FAILED&&captures==1,"changed actual Framework registration rejects before consumer");framework.receipt.registration=71;check(owner.unbind(e),"registration restored only in fixture for metadata cleanup");
    std::cout<<checks<<" actual Release graph/consumer/caller assertions passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
