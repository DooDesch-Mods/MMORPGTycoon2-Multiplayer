#include "building_clearance_graph.h"
namespace building_clearance_cases {
bool run(){
    namespace graph=mt2mp::game::building_clearance_graph;
    constexpr uintptr_t owner=0x160000,primary=0x170000,cash=0x180000;
    history::Saved saved;auto memory=setup(saved);std::string error;
    auto mixed=[&](){
        memory=setup(saved);memory.put(primary,f::image+0x16de860);memory.put(primary+0xb8,uint64_t(1001));
        memory.put(cash,f::image+0x1786fb0);
        f::array(memory,owner,0x16e32d0,{0x310000,0x320000,0x330000});
        f::array(memory,owner+0x18,0x16e2dc0,{primary,inverse_node,cash});
        f::array(memory,owner+0x30,0x1748aa0,{cash,inverse_node,primary});
    };
    mixed();
    auto read=[&](bool initial=true){return graph::read(memory,f::image,owner,{{saved.uid,saved}},{},{saved.uid},initial,error);};
    f::need(read(),"Actual mixed Building/Scenery inverse rejected: "+error);
    const auto lazy=[&](){mixed();f::array(memory,owner,0x16e32d0,{});f::array(memory,owner+0x30,0x1748aa0,{});};
    lazy();f::need(read(),"Original freshly locked inverse must retain complete Make payload before lazy Sort: "+error);
    f::need(read(false),"History preflight must accept the original graph before its first Do/Sort: "+error);
    lazy();memory.put(inverse_node+0xb8,saved.uid+1);
    f::need(!read()&&error.find("foreign or repeated")!=std::string::npos,"Lazy cache bypassed exact original Make UID domain");
    mixed();f::array(memory,owner+0x30,0x1748aa0,{});
    f::need(!read()&&error.find("nodes=3 sorted=0 operations=3")!=std::string::npos,"Partial native Sort cache was accepted");
    mixed();f::array(memory,owner,0x16e32d0,{});
    f::need(!read()&&error.find("nodes=0 sorted=3 operations=3")!=std::string::npos,"Sorted operations without native dependency nodes were accepted");
    mixed();f::array(memory,owner+0x30,0x1748aa0,{cash});
    f::need(!read()&&error.find("nodes=3 sorted=1 operations=3")!=std::string::npos,"Incomplete original sorted domain was accepted");

    mixed();memory.put(inverse_node+0xb8,saved.uid+1);f::need(!read()&&error.find("foreign or repeated")!=std::string::npos,"Foreign native Make UID admitted");
    mixed();memory.put(inverse_node+0xc8,f::root+1);f::need(!read(),"Foreign Make owner admitted");
    mixed();memory.put(inverse_node+0x13c,uint32_t(1));f::need(!read(),"Lost original height admitted");
    mixed();memory.put(inverse_node+0xd0+48,uint8_t(1));f::need(!read(),"Initial full matrix payload was ignored");
    f::need(read(false),"History cannot retain a rebuilt native matrix cache: "+error);
    mixed();memory.put(inverse_node+0xd0+16,uint32_t(0x3f800000));f::need(!read(false),"History changed authored position");
    mixed();dependency(memory,inverse_node+0x30,saved.uid);f::need(!read(),"Foreign dependency admitted");
    mixed();f::array(memory,owner+0x18,0x16e2dc0,{primary,cash});f::need(!read(),"Missing exact Make effect admitted");
    mixed();f::array(memory,owner+0x18,0x16e2dc0,{primary,inverse_node,inverse_node});f::need(!read(),"Repeated original operation admitted");
    mixed();f::array(memory,owner+0x30,0x1748aa0,{cash,cash,primary});f::need(!read(),"Foreign sorted graph domain admitted");
    mixed();memory.put(inverse_node,f::image+0x16de860);memory.put(f::image+0x16de860+0x10,f::image+0x53e1c0);
    dependency(memory,inverse_node+8,0);dependency(memory,inverse_node+0x30,0);dependency(memory,inverse_node+0x58,saved.uid);
    f::need(graph::read(memory,f::image,owner,{}, {saved.uid},{saved.uid},false,error),"Actual opposite Destroy graph rejected: "+error);
    f::need(!graph::read(memory,f::image,owner,{}, {},{saved.uid},false,error),"Unmatched original Destroy admitted");
    return true;
}
}
