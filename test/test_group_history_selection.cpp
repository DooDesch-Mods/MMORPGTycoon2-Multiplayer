#define main group_reader_fixture_main
#include "test_group_reader.cpp"
#undef main
#include "group_history_reader.h"
namespace selection_fixture {
namespace group_reader=mt2mp::game::group_reader;
namespace q=group_reader::q;
struct Runtime{uintptr_t image=f::image;f::Memory memory;};
struct Sample{struct World{uintptr_t selection=f::selection;}world;group_reader::Local local;};
Runtime* active=nullptr;std::map<uint64_t,uintptr_t> live;unsigned selections=0,filters=0;bool fail_select=false,fail_filter=false;
bool find(Runtime&,uint64_t uid,uintptr_t& object,std::string&){const auto found=live.find(uid);object=found==live.end()?0:found->second;return true;}
uint32_t filter_call(uintptr_t,uintptr_t receiver,const std::array<uint8_t,9>& filter){++filters;if(fail_filter)return 5;active->memory.put(receiver+0xc8,filter);return 0;}
struct Port{Runtime& r;std::array<uint8_t,9> old_filter{};std::vector<group_reader::Handle> old_selection;uintptr_t camera_shard=0;
    bool select(const std::vector<uintptr_t>& desired,std::string& e){++selections;std::array<uint8_t,9> all;all.fill(1);r.memory.put(f::selection+0xc8,all);
        if(fail_select){e="first original selection failure";return false;}
        f::array(r.memory,f::selection+0x20,0x1748c50,desired);f::array(r.memory,f::selection+0x38,0x1748c50,desired);return true;}};
#include "group_history_restore_actual.inc"
void run(){std::string e;Runtime r;r.memory=f::setup();active=&r;const uintptr_t shards=0xb0000,camera=0xc0000;
    r.memory.put(f::image+0x13df440,shards);r.memory.put(shards+0x60,camera);
    Sample before,after;before.local.handles={{"group:one",{f::group,77}},{"one",{f::scenery,11}},{"two",{f::free_prop,22}}};
    before.local.state.groups={{"group:one",67,f::pose(),{{"one",f::pose()},{"two",f::pose()}}}};
    before.local.state.props={{"one","group:one",67,f::pose()},{"two","group:one",67,f::pose()}};
    after=before;after.local.state.groups.clear();after.local.handles.erase("group:one");for(auto& p:after.local.state.props)p.parent.clear();
    live={{11,f::scenery},{22,f::free_prop},{77,f::group}};Port port{r};port.old_filter[1]=1;
    r.memory.put(f::selection+0xc8,port.old_filter);r.memory.put(f::group+0x18,uint64_t(77));before.local.raw={f::group};
    f::need(history_ready(port,before,e)&&port.old_selection==std::vector<group_reader::Handle>{{f::group,77}}&&port.camera_shard==camera,e);
    live.erase(77);Port wrong{r};f::need(!history_ready(wrong,before,e),"foreign selected full UID admitted");
    f::need(history_restore(port,before,after,e)&&selections==1&&filters==1,e);
    std::vector<uintptr_t> raw,filtered;f::need(group_reader::selection(r.memory,f::image,f::selection,raw,filtered,e)&&raw==std::vector<uintptr_t>{f::scenery,f::free_prop},"removed Group did not restore its exact ordered free members");
    live[77]=f::group;port.old_selection={{f::scenery,11},{f::free_prop,22}};
    f::need(history_restore(port,after,before,e),e);f::need(group_reader::selection(r.memory,f::image,f::selection,raw,filtered,e)&&raw==std::vector<uintptr_t>{f::group},"regrouped local members did not map to one reciprocal Group");
    port.old_selection={{0xd0000,99}};f::need(!history_restore(port,before,after,e)&&e.find("unrelated")!=std::string::npos,"lost unrelated local full UID ignored");
    port.old_selection={{f::scenery,11}};fail_select=true;f::need(!history_restore(port,after,before,e)&&e=="first original selection failure","original Selection cause lost");
    std::array<uint8_t,9> restored;f::need(q::at(r.memory,f::selection,0xc8,restored,e)&&restored==port.old_filter,"failed Selection left forced all-category filter");
    fail_filter=true;f::need(!history_restore(port,after,before,e)&&e.starts_with("first original selection failure; cleanup:"),"filter cleanup replaced primary cause");
    fail_select=fail_filter=false;r.memory.put(shards+0x60,camera+1);f::need(!history_restore(port,after,before,e)&&e.find("camera")!=std::string::npos,"camera switch ignored");
}
}
int main()try{selection_fixture::run();std::cout<<f::checks<<" actual Group history local Selection/filter/camera/cleanup cases passed\n";return 0;}
catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
