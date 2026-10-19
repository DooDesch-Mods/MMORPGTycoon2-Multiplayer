#define main group_protocol_fixture_main
#include "test_group_protocol.cpp"
#undef main
#include "group_native.h"
namespace gn=mt2mp::game::group_native;
namespace gr=mt2mp::game::group_reader;
struct NativePort {
    GroupIntent intent;gr::Local current;std::string fault;unsigned creates=0,selections=0,commands=0,cleans=0,restores=0,inverses=0;
    bool fail(const char* stage,std::string& e){if(fault==stage){e=std::string("first ")+stage+" cause";return true;}return false;}
    bool ready(const gr::Local&,std::string& e){return !fail("ready",e);}
    bool create(const gr::Local&,const gt::Plan&,gr::Handle& out,std::string& e){++creates;if(fail("create-before",e))return false;out={900,90};return !fail("create-after",e);}
    bool select(const std::vector<uintptr_t>& targets,std::string& e){++selections;
        if(creates)need(targets.front()==900,"new explicitly rooted Group not first for original existing branch");return !fail("select",e);}
    bool original(GroupKind kind,std::string& e){++commands;need(kind==intent.kind,"original kind differs");if(fail("original",e))return false;
        current.state=apply(current.state,intent);for(const auto& g:current.state.groups)if(!current.handles.contains(g.key))current.handles[g.key]={900,90};return true;}
    bool read_after(const gr::Local&,gr::Local& out,std::string& e){if(fail("read",e))return false;out=current;return true;}
    bool inverse(const gr::Local&,const gr::Local&,const gt::Plan& plan,const gr::Handle& provisional,uintptr_t& graph,std::string& e){++inverses;
        need(plan.inverse_count==(intent.kind==GroupKind::Group?1:plan.removed.size()),"whole original inverse count lost");
        if(creates)need(provisional==gr::Handle{900,90},"provisional full UID not inverse-owned");graph=1234;return !fail("inverse",e);}
    bool cleanup_empty(const gr::Handle& h,std::string& e){++cleans;need(h==gr::Handle{900,90},"foreign empty Group cleanup");return !fail("cleanup",e);}
    bool restore(const gr::Local&,const gr::Local&,const gt::Plan&,bool,std::string& e){++restores;return !fail("restore",e);}
};
gr::Local local(const GroupState& state){gr::Local out;out.manager=77;out.selection=88;out.state=state;uintptr_t object=100;
    for(const auto& p:state.props){out.handles[p.key]={object,uint64_t(object)};object+=10;}
    for(const auto& g:state.groups){out.handles[g.key]={object,uint64_t(object)};object+=10;}return out;}
int main()try{
    unsigned n=0;std::string e;const GroupIntent intent{GroupKind::Group,{key(1),key(2)}};const auto prior=local(before());
    NativePort p{intent,prior};gr::Local after;gn::Receipt receipt;
    need(gn::execute(p,intent,prior,after,receipt,e),e);need(p.creates==1&&p.commands==1&&p.inverses==1&&p.restores==1&&!p.cleans,"normal whole Group native order/count");n+=2;
    need(receipt.provisional_created&&receipt.original_returned&&receipt.selection_restored&&receipt.original_calls==1&&receipt.graph==1234,"whole native receipt missing");++n;
    need(!gn::execute(p,intent,prior,after,receipt,e)&&p.commands==1,"native mutation replay accepted");++n;
    for(const std::string stage:{"ready","create-before","create-after","select","original","read","inverse","restore"}){
        NativePort port{intent,prior,stage};gn::Receipt r;e.clear();need(!gn::execute(port,intent,prior,after,r,e),"failed native stage confirmed");
        need(e.starts_with("first "+stage+" cause"),"original stage cause lost");
        need(port.commands<=1&&port.restores==(stage=="ready"?0u:1u),"cleanup/replay count differs");
        need(port.cleans==((stage=="create-after"||stage=="select")?1u:0u),"provisional cleanup occurred after original or was skipped");n+=4;
    }
    auto first=apply(before(),intent);GroupIntent append{GroupKind::Group,{"group:"+key(1),key(3)}};auto prior_append=local(first);NativePort existing{append,prior_append};gn::Receipt r;
    need(gn::execute(existing,append,prior_append,after,r,e)&&!existing.creates&&existing.commands==1,e);++n;
    auto two=apply(first,{GroupKind::Group,{key(3),key(4)}});GroupIntent split{GroupKind::Ungroup,{"group:"+key(1),"group:"+key(3)}};auto prior_split=local(two);NativePort ungroup{split,prior_split};r={};
    need(gn::execute(ungroup,split,prior_split,after,r,e)&&!ungroup.creates&&ungroup.commands==1&&after.state.groups.empty(),e);++n;
    std::cout<<n<<" actual whole Group transaction/cause/provisional inverse/cleanup/no-replay cases passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}
