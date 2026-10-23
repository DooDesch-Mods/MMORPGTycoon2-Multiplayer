#include "dungeon_zone_roster.h"
#include "world_zone_runtime.h"
namespace world_zone_scope_fixture {
namespace dungeon_zone_roster=mt2mp::game::dungeon_zone_roster;
namespace dungeon_scene_roster=mt2mp::game::dungeon_scene_roster;
namespace world_zone_runtime=mt2mp::game::world_zone_runtime;
namespace q {
struct Zone {uintptr_t pointer=0;uint64_t uid=0;};
struct SavedPool {struct Array {std::vector<Zone> zones;}array;};
bool fail(std::string& e,const std::string& reason){e=reason;return false;}
template<class T>bool at(Memory& m,uintptr_t p,size_t offset,T& value,std::string& e){return m.read(p+offset,&value,sizeof value,e);}
std::string zone_key(const SavedPool&,const Zone& row){return "zone:"+std::to_string(row.uid);}
}
namespace opened {struct CurrentWorld {void* map=nullptr;uint64_t revision=0;};}
namespace native_open_runtime {
inline opened::CurrentWorld current;inline bool accept=true;
bool read_current_world(opened::CurrentWorld& out,std::string& e){
    need(!t::PageReadScope::reader_active(),"World Zone final World read crossed owner scope");
    if(!accept){e="fixture final World read rejected";return false;}out=current;return true;
}
}
bool same_world_zone_origin(const opened::CurrentWorld& a,const opened::CurrentWorld& b){return a.map==b.map&&a.revision==b.revision;}
struct Fixture {
    RegionFixture f;Memory& memory=f.memory;uintptr_t image=f.image;opened::CurrentWorld world{reinterpret_cast<void*>(f.map),1};
    std::vector<dungeon_scene_roster::Scene> scenes;
    std::map<int32_t,q::SavedPool> pools;
    struct ScenePool {dungeon_scene_roster::Scene scene;q::SavedPool saved;};std::map<uintptr_t,ScenePool> scene_pools;
    struct Row {uintptr_t object=0;int32_t type=0;std::string key;};
    struct ExtraPool {uintptr_t root=0,region=0;std::string scene;std::map<uint64_t,Row> rows;};std::map<uintptr_t,ExtraPool> world_zone_pools;
    std::function<void()> native_visit;uint32_t native_error=0;size_t native_calls=0;
    uintptr_t zone=f.base+0x70000;
    uintptr_t manager(size_t n)const{return f.base+0x50000+n*0x200;}
    Fixture(){reset();}
    void reset(){f.reset();pools.clear();scene_pools.clear();world_zone_pools.clear();scenes.clear();native_visit={};native_error=0;native_calls=0;
        native_open_runtime::current=world;native_open_runtime::accept=true;
        for(size_t n=0;n<121;++n){const auto m=manager(n),region=f.region(n);pools.emplace(int32_t(n),q::SavedPool{});
            f.put(region+0x968,m);f.put(m,image+0x16fe170);f.put(m+0xf0,region);
            for(size_t type=0;type<4;++type)f.put(m+0x18+type*0x30,image+0x172e1e0);}
    }
    void member(){const auto array=manager(0)+0x48,data=f.base+0x72000;
        f.put(array+8,data);f.put(array+0x10,int32_t(1));f.put(array+0x14,int32_t(1));f.put(data,zone);
        f.put(zone,image+0x1745ce0);f.put(zone+0x18,uint64_t(7));f.put(zone+0x170,f.root);f.put(zone+0x178,f.region(0));f.put(zone+0x24c,int32_t(1));
        pools.at(0).array.zones.push_back({zone,7});
    }
    void scene(){const auto region=f.base+0x68000,shard=f.base+0x69000,m=f.base+0x6a000;
        dungeon_scene_roster::Scene row;row.owner.key="scene:fixture";row.owner.region=region;row.owner.shard=shard;
        row.owner.dungeon=f.base+0x6b000;row.dungeon_uid=19;scenes.push_back(row);scene_pools.emplace(m,ScenePool{row,{}});
        f.put(region+0x968,m);f.put(region+0x70,shard);f.put(m,image+0x16fe170);f.put(m+0xf0,region);
        for(size_t type=0;type<4;++type)f.put(m+0x18+type*0x30,image+0x172e1e0);
    }
    uint32_t exact_uid(uint64_t uid,uintptr_t& value){need(!t::PageReadScope::reader_active(),"World Zone FindUID did not invalidate reader metadata");++native_calls;
        if(native_visit)native_visit();value=uid==7?zone:0;return native_error;}
    bool baseline(std::vector<world_zone_runtime::Current>& out,std::string& error){
#include "world_zone_scope_baseline.inc"
    }
    bool actual(std::vector<world_zone_runtime::Current>& out,std::string& error){
#include "world_zone_scope_actual.inc"
    }
};
void run(){
    Fixture f;t::Scope measured(t::Part::HostCapture);t::PageReadScope original(t::PageBoundary::OriginalCall);auto* parent=t::page_read_scope;
    std::string e;std::vector<world_zone_runtime::Current> got,expected;
    const auto check=[&](bool success,const std::function<void()>& setup){
        f.reset();setup();auto start=queries();e.clear();expected.clear();const bool old=f.baseline(expected,e);
        const auto unscoped=queries()-start,old_calls=f.native_calls;const auto reads=f.memory.reads;const auto cause=e;
        f.reset();setup();start=queries();e.clear();got.clear();const bool result=f.actual(got,e);const auto scoped=queries()-start;
        need(old==success&&result==old&&got==expected&&e==cause&&f.memory.reads==reads&&f.native_calls==old_calls,"World Zone scope changed census/reads/error/native calls");
        need(t::page_read_scope==parent&&!t::PageReadScope::reader_active(),"World Zone owner scope escaped its traversal");return std::pair{unscoped,scoped};
    };
    const auto counts=check(true,[]{});need(counts.first>240&&counts.second<=5,"World Zone per-Region query reset remains");
    check(true,[&]{f.member();});need(got.size()==1&&got[0].uid==7,"World Zone member identity differs");
    check(true,[&]{f.scene();});
    check(false,[&]{f.scene();f.scene_pools.begin()->second.scene.dungeon_uid=20;});
    check(false,[&]{Fixture::ExtraPool extra;extra.rows.emplace(7,Fixture::Row{f.zone,0,"orphan"});f.world_zone_pools.emplace(f.zone,std::move(extra));});
    check(false,[&]{f.pools.erase(120);});
    check(false,[&]{f.f.put(f.f.region(120)+0x968,f.manager(0));});
    check(false,[&]{f.f.put(f.manager(120)+0xf0,f.f.region(0));});
    check(false,[&]{f.member();f.pools.at(0).array.zones.clear();});
    check(false,[&]{f.member();f.native_error=1234;});
    check(false,[&]{f.member();f.native_visit=[&]{f.f.put(f.f.map+0x244,int32_t(122));};});
    check(false,[&]{native_open_runtime::current.revision=2;});
    check(false,[&]{native_open_runtime::accept=false;});
    // A native UID lookup can change protection after a previous Region read.
    f.reset();f.member();DWORD old_protection=0;bool changed=false;
    f.native_visit=[&]{if(!changed){need(VirtualProtect(reinterpret_cast<void*>(f.manager(0)),0x1000,PAGE_NOACCESS,&old_protection)!=0,"World Zone native protection change failed");changed=true;}};
    const bool accepted=f.actual(got,e);DWORD ignored=0;
    need(VirtualProtect(reinterpret_cast<void*>(f.manager(0)),0x1000,old_protection,&ignored)!=0,"World Zone native protection restore failed");
    need(changed&&!accepted&&e=="record fixture unreadable","World Zone reused cached page permissions across FindUID");
    f.native_visit={};e.clear();need(f.actual(got,e),"World Zone next read failed after restored native page");
    std::cout<<"World Zone owner traversal: identical121 Region censuses/errors, OS queries "<<counts.first<<" -> "<<counts.second<<"; native invalidation retained\n";
}
}
