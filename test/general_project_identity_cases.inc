#include "general_project_identity_reader.h"

namespace project_identity_fixture {
namespace p=mt2mp::game::general_project;
namespace live {namespace apply_timing=mt2mp::game::live_host_state::apply_timing;struct Pin {uintptr_t world=0;};}
namespace a {struct Context {};struct Binding {uint64_t native_epoch=7,native_generation=8;};}
struct World {uint64_t world_epoch=7,generation=8;void* world=reinterpret_cast<void*>(0x9000);void* state=reinterpret_cast<void*>(0xa000);};
namespace building_mixed_identity::opened_file {using CurrentWorld=World;}
struct Runtime {
    uintptr_t image=0x10000000;
    std::map<uintptr_t,std::vector<unsigned char>> fields;
    std::map<uintptr_t,std::string> texts;
    size_t reads=0,text_reads=0,passes=0,contexts=0;
    uintptr_t fail_address=0;
    std::function<void(Runtime&)> on_second;
    template<class T>void put(uintptr_t address,const T& value){const auto* bytes=reinterpret_cast<const unsigned char*>(&value);fields[address]={bytes,bytes+sizeof value};}
    void array(uintptr_t owner,size_t offset,uintptr_t data,std::initializer_list<uintptr_t> values){
        put(owner+offset,data);put(owner+offset+8,int32_t(values.size()));put(owner+offset+12,int32_t(values.size()));
        size_t n=0;for(const auto value:values)put(data+8*n++,value);
    }
    Runtime(){
        put(image+0x13df500,uintptr_t(0x1000));put(0x1000,image+0x1715450);
        put(image+0x13df250,uintptr_t(0x2000));put(0x2000,image+0x16dc400);
        put(image+0x13df2d0,uintptr_t(0x3000));
        array(0x2000,0x10,0xb000,{0x4000,0x5000});array(0x1000,0x280,0xc000,{0x6000,0x7000});array(0x3000,8,0xd000,{0x8000});
        put(0x4000,image+0x1744c40);put(0x5000,image+0x16dc330);texts[0x4020]="tech-a";texts[0x5020]="tech-b";
        put(0x6000,image+0x16e79d0);put(0x7000,image+0x16e79d0);texts[0x6038]="tech-a";texts[0x7038]="tech-b";
        put(0x6060,uintptr_t(0x4000));put(0x7060,uintptr_t(0x5000));put(0x41e8,uintptr_t(0x6000));put(0x51e8,uintptr_t(0x7000));
        put(0x8000,image+0x16c0140);texts[0x8028]="art-a";put(0xa1c8,uintptr_t(0x1000));
    }
};
inline World observed_world;
inline bool reject_context=false,reject_world=false;
inline bool fail(std::string& error,const std::string& cause){error=cause;return false;}
template<class T>bool at(Runtime& runtime,uintptr_t owner,size_t offset,T& out,std::string& error){
    ++runtime.reads;const auto address=owner+offset;
    if(address==runtime.image+0x13df500&&++runtime.passes==2&&runtime.on_second)runtime.on_second(runtime);
    const auto found=runtime.fields.find(address);
    if(address==runtime.fail_address||found==runtime.fields.end()||found->second.size()!=sizeof out)return fail(error,"original identity field unreadable");
    std::memcpy(&out,found->second.data(),sizeof out);return true;
}
inline bool string(Runtime& runtime,uintptr_t address,std::string& out,std::string& error){
    ++runtime.text_reads;const auto found=runtime.texts.find(address);
    if(found==runtime.texts.end())return fail(error,"original identity text unreadable");out=found->second;return true;
}
inline bool context(Runtime& runtime,const a::Context&,const a::Binding&,live::Pin& pin,std::string& error){
    ++runtime.contexts;pin.world=0x9000;return !reject_context||fail(error,"original session admission refused");
}
namespace native_open_runtime {inline bool read_current_world(World& out,std::string& error){out=observed_world;return !reject_world||fail(error,"original current world refused");}}
#include "project_reference_snapshot.inc"
inline bool owners(Runtime& runtime,p::ReferenceOwners& out,std::string& error){
    auto read=[&](uintptr_t address,size_t offset,auto& value,std::string& cause){return at(runtime,address,offset,value,cause);};
    auto text=[&](uintptr_t address,std::string& value,std::string& cause){return string(runtime,address,value,cause);};
    return p::reference_owners(read,text,runtime.image,out,error);
}
void checks(){
    p::ReferenceOwners out;std::string error;Runtime good;
    need(owners(good,out,error)&&out.tech_keys.at("tech-a")==0x4000&&out.tech_keys.at("tech-b")==0x5000&&out.pack_keys.at("art-a")==0x8000&&out.software==0x1000,"actual pure identity reader returns canonical owners without project/effect/inventory fields");
    const auto fresh_reads=good.reads;need(fresh_reads==28&&good.text_reads==5,"complete identity scan reads the exact owner arrays/classes/reciprocity once");
    for(uintptr_t klass:{uintptr_t(0x1744c40),uintptr_t(0x16dc330),uintptr_t(0x16e0ac0),uintptr_t(0x16fdb10)}){
        Runtime input;input.put(0x4000,input.image+klass);need(owners(input,out,error),"all admitted Tech subclasses retain canonical identity");
    }
    for(int fault=0;fault<17;++fault){Runtime input;error.clear();
        if(fault==0)input.put(0x1000,uintptr_t(1));
        if(fault==1)input.put(0x2000,uintptr_t(1));
        if(fault==2)input.put(input.image+0x13df2d0,uintptr_t(0));
        if(fault==3)input.put(0x2018,int32_t(-1));
        if(fault==4)input.put(0x201c,int32_t(1));
        if(fault==5)input.put(0xb008,uintptr_t(0x4000));
        if(fault==6)input.put(0xb008,uintptr_t(0));
        if(fault==7)input.put(0x4000,uintptr_t(1));
        if(fault==8)input.texts[0x5020]="tech-a";
        if(fault==9)input.texts[0x4020].clear();
        if(fault==10)input.put(0x6000,uintptr_t(1));
        if(fault==11)input.put(0x6060,uintptr_t(0xdead));
        if(fault==12)input.texts[0x6038]="wrong-tech";
        if(fault==13)input.put(0x41e8,uintptr_t(0x7000));
        if(fault==14)input.put(0x1288,int32_t(1));
        if(fault==15)input.put(0x8000,uintptr_t(1));
        if(fault==16)input.texts[0x8028].clear();
        need(!owners(input,out,error)&&!error.empty()&&out==p::ReferenceOwners{},"invalid canonical owner/extent/key/reciprocity must fail without partial publication");
    }
    Runtime unreadable;unreadable.fail_address=0x7060;error.clear();
    need(!owners(unreadable,out,error)&&error=="original identity field unreadable","identity reader preserves original failed read");
    observed_world={};reject_context=reject_world=false;Runtime snapshot;
    need(reference_snapshot(snapshot,{}, {},out,error)&&snapshot.passes==2&&snapshot.contexts==1&&snapshot.reads==fresh_reads*2+1,"actual runtime reads both complete identities and final State Software owner");
    for(int fault=0;fault<8;++fault){Runtime input;observed_world={};error.clear();reject_context=reject_world=false;
        if(fault==0)input.on_second=[](Runtime& value){value.texts[0x4020]="changed";value.texts[0x6038]="changed";};
        if(fault==1)input.on_second=[](Runtime& value){value.fail_address=0x7060;};
        if(fault==2)++observed_world.world_epoch;
        if(fault==3)++observed_world.generation;
        if(fault==4)observed_world.world=reinterpret_cast<void*>(0x9999);
        if(fault==5)input.put(0xa1c8,uintptr_t(0x2000));
        if(fault==6)reject_context=true;
        if(fault==7)reject_world=true;
        need(!reference_snapshot(input,{}, {},out,error)&&!error.empty()&&out==p::ReferenceOwners{},"double read/current world/session drift remains rejected atomically");
        if(fault==0)need(error=="General project canonical reference owners changed during double read","double read drift keeps distinct cause");
        if(fault==1)need(error=="original identity field unreadable","second read failure keeps upstream cause");
        if(fault==6)need(input.reads==0,"failed session admission reads no identity memory");
    }
    observed_world={};reject_context=reject_world=false;Runtime repeat;
    need(reference_snapshot(repeat,{}, {},out,error),"initial independent identity snapshot");repeat.put(0x41e8,uintptr_t(0));
    need(!reference_snapshot(repeat,{}, {},out,error),"later call cannot reuse prior identity bytes");
}
}
