#define main retained_visual_fixture_main
#include "test_toon_weapon_visual.cpp"
#undef main
#include "toon_weapon_visual/probe.h"
#include "npc_census.h"

struct ProbeFixture:Fixture {
    uintptr_t shard,region,matrices;
    ProbeFixture(){
        shard=m.alloc(0x60);region=m.alloc(0x788);matrices=m.alloc(0x40);
        m.put(toon,0x170,shard);m.put(shard,0x48,int32_t(1));m.put(shard,0x18,region);m.put(region,0x780,uintptr_t(99));
        m.put(skeleton,0x18,matrices);
        std::array<float,16> identity{};for(size_t i=0;i<4;++i)identity[i*5]=1;
        m.put(matrices,0,identity);m.put(instance,0,identity);
    }
    bool observe(Observation& out,std::string& e){Plan p;return read_plan(m,image,toon,p,e)&&read_scene(m,p,e)&&read_observation(m,p,out,e);}
};
struct RandomCalls final:Calls {
    Fake f;int random_calls=0;bool change=false,missing=false,throws=false;
    bool pin(uintptr_t t,Lease& l,bool& o,std::string& e)override{return f.pin(t,l,o,e);}
    bool plan(uintptr_t t,Plan& p,std::string& e)override{return f.plan(t,p,e);}
    bool scene(Plan& p,Call& c,std::string& e)override{return f.scene(p,c,e);}
    bool matches(const Plan& p,bool& m,std::string& e)override{return f.matches(p,m,e);}
    bool remove(const Plan& p,Call& c,std::string& e)override{return f.remove(p,c,e);}
    bool cache(const Plan& p,Call& c,std::string& e)override{return f.cache(p,c,e);}
    bool attach(const Plan& p,const Part& a,Call& c,std::string& e)override{return f.attach(p,a,c,e);}
    bool matrix(const Plan& p,const Part& a,Call& c,std::string& e)override{return f.matrix(p,a,c,e);}
    bool random(std::array<uint64_t,4>& out,std::string& e)override{
        ++random_calls;if(throws)throw std::runtime_error("fixture RNG read exception");
        if(missing){e="fixture RNG pointer absent";return false;}out={1,2,3,4};if(change&&random_calls==2)++out[0];return true;
    }
};
struct RandomMemory final:mt2mp::npc_census::Memory {
    ::Memory& memory;explicit RandomMemory(::Memory& m):memory(m){}
    bool copy(uintptr_t p,void* out,size_t n,std::string& e)override{return memory.copy(p,out,n,e);}
};
int main(){try{
    {ProbeFixture f;const auto before=f.m.blocks;Observation o;std::string e;require(f.observe(o,e),"actual probe reader rejected owned model");
        require(o.actor_present&&o.attachment_present&&o.matching&&o.actual_model_id==0&&o.actual_bone==0,"probe lost actual selected model/bone");
        const auto j=encode_observation(o);require(j.at("attempted")==false&&j.at("read_only")==true&&j.at("actual_model_id")==0,"read-only receipt differs");
        require(f.m.blocks==before,"read-only probe changed source memory");}
    {ProbeFixture f;f.m.put(f.toon,0x6f0,uintptr_t(0));Observation o;std::string e;require(f.observe(o,e)&&!o.actor_present&&!o.attachment_present,"absent actor not represented");}
    {ProbeFixture f;f.m.put(f.skeleton,0xe8,uintptr_t(0));Observation o;std::string e;require(f.observe(o,e)&&o.actor_present&&!o.attachment_present&&!o.matching,"missing attachment invented render success");}
    {ProbeFixture f;f.m.put(f.record,0x70,uintptr_t(100));Observation o;std::string e;require(!f.observe(o,e)&&e.find("scene/model owner")!=std::string::npos,"foreign scene accepted");}
    {ProbeFixture f;f.m.put(f.skeleton,0xf8,int32_t(1));Observation o;std::string e;require(!f.observe(o,e)&&e.find("bone outside")!=std::string::npos,"invalid actual bone read");}
    {ProbeFixture f;f.m.put(f.shard,0x48,int32_t(3));Observation o;std::string e;require(!f.observe(o,e)&&e.find("Shard kind")!=std::string::npos,"unsupported scene kind accepted");}
    {ProbeFixture f;f.m.put(f.region,0x780,uintptr_t(0));Observation o;std::string e;require(!f.observe(o,e)&&e.find("scene absent")!=std::string::npos,"missing scene accepted");}
    {ProbeFixture f;f.m.put(f.shard,0x48,int32_t(2));const auto owner=f.m.alloc(0xc8);f.m.put(f.shard,0x20,owner);f.m.put(owner,0xc0,uintptr_t(99));Observation o;std::string e;require(f.observe(o,e),"kind2 existing scene not read");}
    {ProbeFixture f;f.m.put(f.shard,0x48,int32_t(0));const auto global=f.m.alloc(8),screen=f.m.alloc(0x38),array=f.m.alloc(8);
        f.m.blocks[f.image+0x164b420].resize(8);f.m.put(f.image+0x164b420,0,global);f.m.put(global,0,screen);f.m.put(screen,0x10,array);f.m.put(screen,0x30,int32_t(1));f.m.put(array,0,uintptr_t(99));
        Observation o;std::string e;require(f.observe(o,e),"kind0 existing Screen scene not read");f.m.put(screen,0x30,int32_t(0));require(!f.observe(o,e)&&e.find("scene count")!=std::string::npos,"empty Screen array accepted");}
    {RandomCalls f;Receipt r;std::string e;require(reconcile(f,1,r,e)&&r.confirmed&&r.random.observed&&r.random.unchanged&&f.random_calls==2,"RNG same before/after not retained");}
    {RandomCalls f;f.change=true;Receipt r;std::string e;require(reconcile(f,1,r,e)&&r.confirmed&&!r.unknown&&e.empty()&&r.random.observed&&!r.random.unchanged&&r.random.cause.find("changed")!=std::string::npos,"diagnostic RNG difference changed visual result");}
    {RandomCalls f;f.missing=true;Receipt r;std::string e;require(reconcile(f,1,r,e)&&r.confirmed&&!r.random.observed&&!r.random.unchanged&&r.random.before_cause=="fixture RNG pointer absent"&&r.random.after_cause==r.random.before_cause,"missing RNG concealed or blocked visual");}
    {RandomCalls f;f.throws=true;Receipt r;std::string e;require(reconcile(f,1,r,e)&&r.confirmed&&!r.random.observed&&r.random.cause.find("fixture RNG read exception")!=std::string::npos,"RNG reader exception changed visual result");}
    {RandomCalls f;f.f.fail_remove=true;Receipt r;std::string e;require(!reconcile(f,1,r,e)&&r.unknown&&e=="original remove SEH"&&r.random.observed&&f.random_calls==2,"partial visual failure lost original cause or RNG after");}
    {RandomCalls f;f.f.owned=false;Receipt r;std::string e;require(reconcile(f,1,r,e)&&!f.random_calls&&!r.random.observed,"unowned pass-through surveyed RNG");}
    {Memory m;RandomMemory reader(m);const uintptr_t image=0x140000000,random=m.alloc(32);m.blocks[image+0x164b250].resize(8);m.put(image+0x164b250,0,random);
        const std::array<uint64_t,4> expected={1,UINT64_MAX,3,4};m.put(random,0,expected);const auto before=m.blocks;std::array<uint64_t,4> actual{};std::string e;
        require(mt2mp::npc_census::read_global_random(reader,image,actual,e)&&actual==expected&&m.blocks==before,"original global RNG helper differs");
        m.put(image+0x164b250,0,uintptr_t(0));require(!mt2mp::npc_census::read_global_random(reader,image,actual,e)&&!e.empty(),"missing RNG pointer success");}
    std::cout<<checks<<" actual probe/RNG checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
