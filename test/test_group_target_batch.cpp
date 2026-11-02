#define main group_reader_fixture_main
#include "test_group_reader.cpp"
#undef main
#include <chrono>
#include <functional>
#include "vendor/nlohmann/json.hpp"
template<class... T> void fixture_log(T&&...) {}
#define LOG_INFO(...) fixture_log(__VA_ARGS__)
namespace batch_fixture {
using json=nlohmann::json;
using f::need;
struct LocalTarget {uintptr_t object=0;};
using LocalTargetResolver=std::function<bool(uintptr_t,LocalTarget&,std::string&)>;
int collections=0,validations=0,world=1,registry_generation=1;bool unknown_locked=false,collector_ok=true,root_ok=true,code_ok=true,target_ok=true;
std::string fixture_snapshot="current";
namespace net {std::string current_snapshot_id(){return fixture_snapshot;}}
namespace host {
namespace opened {struct CurrentWorld {int value=0,world_epoch=0,generation=0;void* world=nullptr;void* map=nullptr;};}
bool same_world(const opened::CurrentWorld& a,const opened::CurrentWorld& b){return a.value==b.value;}
struct RootSceneryReadBatch {bool finish(std::string& e){if(!root_ok)e="root finish";return root_ok;}};
}
namespace scenery_kiss {struct CatalogCodeReadBatch {bool finish(std::string& e){if(!code_ok)e="code finish";return code_ok;}};}
namespace prop_lease {struct World {int epoch=0,origin_generation=0;void* world=nullptr;void* map=nullptr;bool operator==(const World&)const=default;};}
struct Bound {int generation=0;prop_lease::World world;std::string snapshot,scene_owner;};
std::map<std::string,Bound> registry;prop_lease::World admitted_world;
namespace scene_scenery {bool saved_admission_pending(){return false;}}
bool current(host::opened::CurrentWorld& out,std::string&){out.value=world;out.world_epoch=world;return true;}
enum class BindingDetails {Identity};
bool collect_bindings_body(json&,std::string& e,BindingDetails){++collections;if(!collector_ok)e="collector";return collector_ok;}
bool resolve_local_target_impl(uintptr_t manager,uintptr_t object,LocalTarget& out,std::string& e,const host::opened::CurrentWorld* admitted,bool metadata_only,const void* known,const std::string* group_snapshot){
    ++validations;if(manager!=f::selection||!admitted||metadata_only||known||!group_snapshot||!target_ok){e="full target rejected";return false;}out.object=object;return true;}
#include "group_target_batch_actual.inc"
void run(){
    auto memory=f::setup();std::string e;f::gr::Local value,baseline;
    constexpr uintptr_t other_group=0xa0000,other_member=0xb0000,other_prop=0xc0000;
    constexpr uint64_t other_group_uid=0x400000007,other_prop_uid=0x500000007;
    f::array(memory,f::manager+0x18,0x16f0060,{f::group,other_group});
    memory.put(other_group,f::image+0x1749de0);memory.put(other_group+0x18,other_group_uid);memory.put(other_group+0x170,f::root);
    memory.put(other_group+0x178,f::region);memory.put(other_group+0x180,uintptr_t(0));memory.put(other_group+0x100,f::pose());
    f::array(memory,other_group+0x210,0x16ea660,{other_member});memory.put(other_member,f::image+0x16fbc40);
    memory.put(other_member+0x18,other_prop);memory.put(other_member+0x64,f::pose());memory.put(other_prop,f::image+0x16c1390);
    memory.put(other_prop+0x18,other_prop_uid);memory.put(other_prop+0x170,f::root);memory.put(other_prop+0x180,other_group);memory.put(other_prop+0x100,f::pose());
    auto bind=[&](uintptr_t p,uint64_t uid,std::string& key,int32_t& id,std::string& why){
        if(p==other_prop&&uid==other_prop_uid){key="action:other";id=67;return true;}return f::bind(p,uid,key,id,why);};
    auto find=[&](uint64_t uid,uintptr_t& p,std::string& why){if(uid==other_group_uid){p=other_group;return true;}return f::find(uid,p,why);};
    need(f::gr::read(memory,f::image,f::root,f::selection,{f::free_prop},bind,f::owner,find,baseline,e)&&baseline.state.groups.size()==2,e);
    auto read=[&](const LocalTargetResolver& resolve,std::string& why){
        auto binder=[&](uintptr_t p,uint64_t uid,std::string& key,int32_t& id,std::string& err){LocalTarget target;
            return resolve(p,target,err)&&target.object==p&&bind(p,uid,key,id,err);};
        return f::gr::read(memory,f::image,f::root,f::selection,{f::free_prop},binder,f::owner,find,value,why);};
    need(with_local_target_batch(f::selection,read,e),e);
    need(collections==1&&validations==3&&value.state==baseline.state,"one collection, full target validations and equal state");
    need(with_local_target_batch(f::selection,read,e)&&collections==2&&validations==6,"each sample is freshly collected");
    auto reject=[&](auto modify,const char* why){e.clear();need(!with_local_target_batch(f::selection,[&](const LocalTargetResolver& resolve,std::string& error){modify();return read(resolve,error);},e),why);};
    reject([]{++world;},"world drift");world=1;
    reject([]{++registry_generation;},"registry drift");registry_generation=1;
    reject([]{fixture_snapshot="other";},"snapshot drift");fixture_snapshot="current";
    reject([]{unknown_locked=true;},"unknown lock");unknown_locked=false;
    reject([]{target_ok=false;},"target rejection");target_ok=true;
    root_ok=false;need(!with_local_target_batch(f::selection,read,e)&&e=="root finish","root finish failure");root_ok=true;
    code_ok=false;need(!with_local_target_batch(f::selection,read,e)&&e=="code finish","code finish failure");code_ok=true;
    collector_ok=false;need(!with_local_target_batch(f::selection,read,e)&&e=="collector","collection failure");collector_ok=true;
    memory.put(f::scenery+0x180,uintptr_t(0));need(!with_local_target_batch(f::selection,read,e),"actual parent mismatch");
}
}
int main()try{batch_fixture::run();std::cout<<f::checks<<" Group batch and actual reader cases passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
