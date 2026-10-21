#define main duplicate_protocol_fixture_main
#include "test_duplicate_protocol.cpp"
#undef main
#include "duplicate_native.h"
namespace dn=mt2mp::game::duplicate_native;
struct TestPort {
    std::string failure;bool startup=true;unsigned charges=0,creates=0,duplicates=0,ends=0,restores=0,reads=0;
    std::vector<std::string> order;std::vector<std::array<uint32_t,3>> positions;
    bool step(const char* name,std::string& e){order.push_back(name);if(failure==name){e=std::string("first ")+name;return false;}return true;}
    bool ready(const DuplicateIntent&,const std::vector<dn::Handle>&,int32_t,std::string& e){return step("ready",e);}
    bool charge(int32_t,std::string& e){++charges;return step("charge",e);}
    bool begin_edit(bool& opened,std::string& e){opened=startup;return step("begin",e);}
    bool end_edit(std::string& e){++ends;return step("end",e);}
    bool create_graph(uintptr_t& out,std::string& e){++creates;out=900;return step("graph",e);}
    bool cash_inverse(uintptr_t graph,int32_t,const std::array<uint32_t,3>& point,std::string& e){need(graph==900,"foreign graph");positions.push_back(point);return step("cash",e);}
    bool duplicate(const dn::Handle&,const DuplicateTarget&,dn::Handle& out,std::string& e){++duplicates;out={1000+duplicates,2000+duplicates};return step("duplicate",e);}
    bool destroy_inverse(uintptr_t graph,const dn::Handle&,std::string& e){need(graph==900,"split inverse graph");return step("destroy",e);}
    bool select_created(const std::vector<dn::Handle>&,std::string& e){return step("select",e);}
    bool release_points(const DuplicateIntent&,const std::vector<dn::Handle>&,std::string& e){return step("release",e);}
    bool commit_graph(uintptr_t graph,const std::vector<dn::Handle>&,std::string& e){need(graph==900,"split commit graph");return step("commit",e);}
    bool readback(const DuplicateIntent&,const std::vector<dn::Handle>&,const dn::Receipt&,std::string& e){++reads;need(!startup||ends==1,"read under original open EditScope");return step("read",e);}
    bool restore_selection(std::string& e){++restores;return step("restore",e);}
};
#include "duplicate_selection_rng_cases.inc"
int main()try{
    unsigned count=duplicate_selection_rng_cases();std::string e;auto intent=message().request.duplicate;std::vector<dn::Handle> sources{{100,1}};
    TestPort p;dn::Receipt r;need(dn::execute(p,intent,sources,r,e),e);
    need(p.order==std::vector<std::string>{"ready","charge","begin","graph","cash","duplicate","destroy","select","release","commit","end","read","restore"},"whole original order differs");
    need(r.graph_committed&&r.charged&&r.duplicate_returns==1&&r.destroy_inverses==1&&r.edit_closed&&r.selection_restored,"complete original receipt absent");count+=3;
    need(!dn::execute(p,intent,sources,r,e)&&p.duplicates==1,"native Duplicate replay accepted");++count;
    for(const std::string stage:{"ready","charge","begin","graph","cash","duplicate","destroy","select","release","commit","end","read","restore"}){
        TestPort port;port.failure=stage;dn::Receipt receipt;e.clear();need(!dn::execute(port,intent,sources,receipt,e),"failed stage confirmed");
        need(e.starts_with("first "+stage),"original cause replaced");need(port.ends<=1&&port.duplicates<=1,"uncertain native call replayed");
        need(port.restores==(stage=="ready"?0u:1u),"local selection cleanup count differs");count+=4;}
    auto many=intent;many.rows.resize(10,intent.rows.front());many.cash_position={1,2,3};sources.resize(10,{100,1});TestPort multi;dn::Receipt all;
    need(dn::execute(multi,many,sources,all,e),e);need(all.cash_inverses==1&&all.destroy_inverses==10&&all.duplicate_returns==10&&multi.creates==1,"large selection not one original transaction");
    need(multi.positions==std::vector<std::array<uint32_t,3>>{many.cash_position},"original aggregated cash point lost");count+=3;
    TestPort normal;normal.startup=false;dn::Receipt no_edit;need(dn::execute(normal,intent,{sources.front()},no_edit,e)&&normal.ends==0&&!no_edit.edit_open,e);++count;
    std::cout<<count<<" actual Duplicate orchestration/failure/one-graph/no-replay cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
