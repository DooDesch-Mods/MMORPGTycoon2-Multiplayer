#include "toon_weapon_visual/visual.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace mt2mp::game::toon_weapon_visual;
static int checks=0;
static void require(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
struct Fake final:Calls {
    Lease lease;Plan p;bool owned=true,same=false,fail_remove=false,fail_attach=false,throw_remove=false,owner_changed=false;
    int pins=0,removes=0,attaches=0,caches=0;bool matrix_ok=true;
    Fake(){lease.toon=1;lease.subscriber=2;p.toon=1;p.actor=3;p.skeleton=4;p.ilvl=12;p.category="swords";p.variant=-123;p.prior_attack=6;p.attack=13;p.model_id=8;
        Part part;part.bone=2;p.parts.push_back(part);}
    bool pin(uintptr_t,Lease& out,bool& active,std::string&)override{out=lease;active=owned;if(++pins==3&&owner_changed)++out.subscriber;return true;}
    bool plan(uintptr_t,Plan& out,std::string&)override{out=p;return true;}
    bool scene(Plan& out,Call& c,std::string&)override{out.scene=99;c.attempted=c.returned=true;return true;}
    bool matches(const Plan&,bool& result,std::string&)override{result=same;return true;}
    bool remove(const Plan&,Call& c,std::string& e)override{++removes;c.attempted=true;if(throw_remove)throw std::runtime_error("remove threw after attempt");
        if(fail_remove){e="original remove SEH";c.exception=5;return false;}c.returned=true;return true;}
    bool cache(const Plan& plan,Call& c,std::string&)override{++caches;c.attempted=c.returned=true;p.prior_attack=plan.attack;return true;}
    bool attach(const Plan&,const Part&,Call& c,std::string& e)override{++attaches;c.attempted=true;if(fail_attach){e="original attach SEH";return false;}c.returned=true;same=true;return true;}
    bool matrix(const Plan&,const Part&,Call& c,std::string& e)override{c.attempted=c.returned=true;if(!matrix_ok)e="matrix differs";return matrix_ok;}
};
struct Memory final:placement_ui_probe::Memory {
    std::map<uintptr_t,std::vector<unsigned char>> blocks;uintptr_t next=0x100000;
    uintptr_t alloc(size_t size){const auto at=next;next+=((size+0xfff)&~size_t(0xfff))+0x1000;blocks[at].resize(size);return at;}
    bool copy(uintptr_t at,void* out,size_t n,std::string& e)override{auto it=blocks.upper_bound(at);if(it==blocks.begin()){e="fixture read missing";return false;}--it;
        const auto delta=at-it->first;if(delta>it->second.size()||n>it->second.size()-delta){e="fixture read bounds";return false;}std::memcpy(out,it->second.data()+delta,n);return true;}
    template<class T>void put(uintptr_t at,size_t offset,const T& x){auto it=blocks.upper_bound(at+offset);--it;const auto delta=at+offset-it->first;
        if(delta+sizeof x>it->second.size())throw std::runtime_error("fixture store bounds");std::memcpy(it->second.data()+delta,&x,sizeof x);}
    void string(uintptr_t at,const std::string& value){const auto data=alloc(value.size()+1);std::memcpy(blocks[data].data(),value.c_str(),value.size()+1);put(at,0,data);put(at,8,uint64_t(value.size()));}
    void array(uintptr_t at,size_t offset,uintptr_t data,int32_t count){put(at,offset,data);put(at,offset+8,count);put(at,offset+12,count);}
};
struct Fixture {
    Memory m;uintptr_t image=0x140000000,toon,type,costume,actor,object,skeleton,definition,bones,descriptor,attachment,manager,rez,category,tier,weapon,model,groups,instance,group,record;
    Fixture(){
        toon=m.alloc(0xa00);type=m.alloc(0x600);costume=m.alloc(0x80);actor=m.alloc(0xe0);object=m.alloc(0x80);skeleton=m.alloc(0x100);
        definition=m.alloc(0x80);bones=m.alloc(8);descriptor=m.alloc(0xe0);attachment=m.alloc(0xb0);manager=m.alloc(0x80);rez=m.alloc(0x100);
        category=m.alloc(0x78);tier=m.alloc(0x20);weapon=m.alloc(8);model=m.alloc(0x100);groups=m.alloc(8);instance=m.alloc(0x78);group=m.alloc(0x80);record=m.alloc(0x78);
        m.blocks[image+0x13df6e0].resize(8);m.put(image+0x13df6e0,0,manager);m.blocks[image+0x13df4d0].resize(8);m.put(image+0x13df4d0,0,rez);
        m.put(toon,0x498,type);m.put(toon,0x6f0,actor);m.put(toon,0x170,uintptr_t(90));m.put(toon,0x4b0,int32_t(12));m.put(toon,0x4d8,int32_t(-123));m.put(toon,0x4dc,int32_t(13));m.string(toon+0x4b8,"swords");
        m.put(type,0xf0,costume);m.string(costume+0x38,"humanoid");m.put(actor,0,object);m.put(object,0x40,skeleton);
        const auto buckets=m.alloc(0x30),node=m.alloc(0x30);m.put(type,0xf8,buckets);m.put(type,0x100,int32_t(1));m.put(type,0x104,int32_t(32));m.put(buckets,0x28,node);m.string(node,"swords");
        m.array(manager,0x60,category,1);m.string(category,"swords");m.array(category,0x68,tier,1);m.put(tier,0,int32_t(10));m.array(tier,0x10,weapon,1);m.put(weapon,0,int32_t(0));m.put(weapon,4,int32_t(-123));
        const auto models=m.alloc(0x10);m.put(models,0,model);m.array(rez,0x68,models,1);
        const auto descriptors=m.alloc(8),attachments=m.alloc(8),bone_name=m.alloc(0x20);m.put(descriptors,0,descriptor);m.array(costume,0x60,descriptors,1);
        m.put(attachments,0,attachment);m.array(descriptor,0xd0,attachments,1);m.string(descriptor+0x18,"right_hand");m.string(attachment+0x18,"hand");m.string(bone_name,"right_hand");
        m.put(skeleton,0,definition);m.put(definition,0x68,bones);m.put(bones,0,bone_name);m.put(skeleton,0xb0,int32_t(1));
        const std::array<float,10> trs={0,0,0,1,0,0,0,1,1,1};m.put(attachment,0x38,trs);
        m.put(skeleton,0xe8,instance);const auto transform=m.alloc(0x6c);m.put(skeleton,0xf0,transform);m.put(transform,0,trs);m.put(skeleton,0xf8,int32_t(0));
        m.put(instance,0x50,group);m.put(group,0x58,model);m.array(rez,0x98,groups,1);m.put(groups,0,record);m.put(record,8,group);m.put(record,0x70,uintptr_t(99));m.put(record,0,int32_t(0));
    }
};
int main(){try{
    {Fake f;Receipt r;std::string e;require(reconcile(f,1,r,e),"normal chosen variant failed");require(r.confirmed&&r.attempted&&!r.unknown&&f.removes==1&&f.attaches==1&&f.caches==1,"normal exact calls");}
    {Fake f;f.owned=false;Receipt r;std::string e;require(reconcile(f,1,r,e)&&!r.owned&&!f.removes&&!f.attaches,"unowned forwarded without visual calls");}
    {Fake f;f.p.actor=0;f.p.parts.clear();f.same=true;Receipt r;std::string e;require(reconcile(f,1,r,e)&&r.deferred&&r.confirmed&&!r.attempted,"absent actor deferred");}
    {Fake f;f.same=true;Receipt r;std::string e;require(reconcile(f,1,r,e)&&!r.attempted&&!f.removes&&r.matrix.returned,"matching model no repeated mutation");}
    {Fake f;f.fail_remove=true;Receipt r;std::string e;require(!reconcile(f,1,r,e)&&r.unknown&&!f.attaches&&e=="original remove SEH","remove Unknown retains cause");}
    {Fake f;f.throw_remove=true;Receipt r;std::string e;require(!reconcile(f,1,r,e)&&r.unknown&&!f.attaches&&e.find("remove threw")!=std::string::npos,"throw after attempt Unknown");}
    {Fake f;f.fail_attach=true;Receipt r;std::string e;require(!reconcile(f,1,r,e)&&r.unknown&&f.attaches==1,"attach failure no replay");}
    {Fake f;f.owner_changed=true;Receipt r;std::string e;require(!reconcile(f,1,r,e)&&r.unknown&&e.find("owner changed after")!=std::string::npos,"post owner guard");}
    {Fake f;f.matrix_ok=false;Receipt r;std::string e;require(!reconcile(f,1,r,e)&&r.unknown&&e=="matrix differs","actual matrix mismatch Unknown");}
    {Fixture f;Plan p;std::string e;require(read_plan(f.m,f.image,f.toon,p,e),"actual plan reader");require(p.variant==-123&&p.model_id==0&&p.parts.size()==1&&p.parts[0].bone==0,"exact signed variant/model/bone");
        p.scene=99;bool match=false;require(read_attachment(f.m,p,match,e)&&match,"actual owned render readback");
        f.m.put(f.instance,0x70,uint8_t(1));require(read_attachment(f.m,p,match,e)&&!match,"actual visibility mismatch");
        f.m.put(f.record,0x70,uintptr_t(100));require(!read_attachment(f.m,p,match,e)&&e.find("scene/model owner")!=std::string::npos,"foreign scene rejected");}
    {Fixture f;f.m.put(f.weapon,4,int32_t(1));Plan p;std::string e;require(!read_plan(f.m,f.image,f.toon,p,e)&&e.find("variant")!=std::string::npos,"invalid variant rejected before effect");}
    {Fixture f;f.m.string(f.descriptor+0x18,"missing");Plan p;std::string e;require(!read_plan(f.m,f.image,f.toon,p,e)&&e.find("bone missing")!=std::string::npos,"missing bone rejected");}
    {Fixture f;f.m.put(f.toon,0x6f0,uintptr_t(0));Plan p;std::string e;require(read_plan(f.m,f.image,f.toon,p,e)&&p.parts.empty()&&p.attack==13,"absent native actor no invented model");}
    std::cout<<checks<<" actual weapon visual checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
