#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../src/game/management_service_graph.h"
#include "../src/game/management_service_graph_bounds.h"
#include "../src/game/management_service_shape_read.h"
#include "../src/game/management_service_graph_read.h"
#include "../src/game/management_service_graph_transition.h"
#include "../src/game/management_service_lock.h"
#include "../src/game/management_service_sources.h"
#include "../src/game/management_service_price_witness.h"
#include <cassert>
#include <map>
#include <tuple>
#include <utility>

using namespace mt2mp::game::management_service_graph;
// RIP-relative LEAs from the pinned manager and spatial-hash constructors.
static_assert(0x27ad8a+7+0x1489e5f==manager_islands_ap);
static_assert(0x27adcc+7+0x14ce26d==manager_member_ap);
static_assert(0x27f29a+7+0x14c7f3f==spatial_hash_ap);
static_assert(0x27f2a6+7+0x14c9d33==spatial_cell_ap);
int main(){
    size_t count=0;assert(array_span(0x1000,2,3,8,10));assert(!array_span(0x1000,3,2,8,10));
    assert(vector_span(0x1000,0x1030,0x1048,24,10,count)&&count==2);
    assert(!vector_span(0x1000,0x1031,0x1048,24,10,count));
    assert(bit_span(0x1000,0,0x1004,1,0x1008,33));assert(!bit_span(0x1000,0,0x1004,1,0x1004,33));
    Graph graph;graph.owner="world";graph.selected="building:1";graph.spatial_float_bits[0]=0x3f800000;graph.dimensions={1,1};graph.cells={{0,1}};
    Island island;island.region="region:1";island.shape.internal_present=true;island.shape.rings.push_back({{{1,2},{3,4}},false});
    island.shape.sections.push_back({{0,0,0}});Link link;link.building="building:1";link.destinations.push_back({1,10,20});island.links.push_back(link);
    graph.islands={island,island};graph.source_buildings.push_back({"building:1",{}});graph.source_buildings[0].services.spawn_type="spawn";std::string original,changed,error;
    assert(projection(graph,original,error));
    auto other=graph;other.islands[0].shape.rings[0].points[0][0]++;assert(projection(other,changed,error)&&original!=changed);
    other=graph;other.islands[0].shape.rings[0].hole=true;assert(projection(other,changed,error)&&original!=changed);
    other=graph;other.islands[0].links[0].destinations[0].currency0++;assert(projection(other,changed,error)&&original!=changed);
    other.islands[0].links[0].state=-1;other.islands[1].links[0].state=-1;assert(topology_transition(graph,other,true,error));assert(!topology_transition(graph,other,false,error));
    assert(changed_selected_destination_prices(graph,other)==1);
    other.cells[0]={1,0};assert(!topology_transition(graph,other,true,error));
    other=graph;other.islands[0].links[0].native_flag=1;assert(projection(other,changed,error)&&original!=changed);
    other=graph;other.source_buildings[0].services.rows.push_back({});assert(projection(other,changed,error)&&original!=changed);
    other.source_buildings[0].services.rows[0].counts[0].today=1;std::string count_changed;assert(projection(other,count_changed,error)&&changed!=count_changed);
    other.source_buildings[0].services.config_present=true;other.source_buildings[0].services.configs.push_back({"key",{0,0}});other.source_buildings[0].services.rows[0].config_key="key";assert(projection(other,count_changed,error));
    other.source_buildings[0].services.rows[0].config=true;std::string binding_changed;assert(projection(other,binding_changed,error)&&count_changed!=binding_changed);
    other=graph;other.source_buildings.clear();assert(!projection(other,changed,error));
    other=graph;other.cells[0]={1,0};assert(projection(other,changed,error)&&original!=changed);
    other=graph;other.islands[0].links[0].destinations[0].island=0;assert(projection(other,changed,error)&&original!=changed);
    other=graph;other.islands[0].links.push_back(link);other.islands[0].links[1].path="path:2";
    std::string ordered;assert(projection(other,ordered,error));std::swap(other.islands[0].links[0],other.islands[0].links[1]);
    assert(projection(other,changed,error)&&ordered!=changed);
    other=graph;other.islands[0].links[0].destinations.push_back({0,30,40});assert(projection(other,ordered,error));
    std::swap(other.islands[0].links[0].destinations[0],other.islands[0].links[0].destinations[1]);assert(projection(other,changed,error)&&ordered!=changed);
    other=graph;other.cells[0]={2};assert(!projection(other,changed,error));
    other=graph;other.islands[0].links[0].destinations[0].island=2;assert(!projection(other,changed,error));
    other=graph;other.islands[0].shape.bounds[0]=0x7f800000;assert(!projection(other,changed,error));
    std::map<uintptr_t,uint8_t> memory;
    const auto put=[&]<class T>(uintptr_t address,const T& value){const auto* p=reinterpret_cast<const uint8_t*>(&value);for(size_t i=0;i<sizeof(T);++i)memory[address+i]=p[i];};
    const auto read=[&]<class T>(uintptr_t address,size_t offset,T& value,std::string& why){auto* p=reinterpret_cast<uint8_t*>(&value);for(size_t i=0;i<sizeof(T);++i){auto found=memory.find(address+offset+i);if(found==memory.end()){why="fixture memory absent";return false;}p[i]=found->second;}return true;};
    const uintptr_t image=0x10000000,shape=0x10000,internal=0x20000,rings=0x21000,points=0x22000,bits=0x23000,sections=0x24000,section=0x25000,vertices=0x26000;
    put(shape,image+0x16e0140);put(shape+0x18,internal);put(shape+0x20,uint8_t(1));put(shape+0x24,std::array<uint32_t,6>{});
    put(shape+0x40,image+0x16eb080);put(shape+0x48,sections);put(shape+0x50,int32_t(1));put(shape+0x54,int32_t(1));put(sections,section);
    put(internal,rings);put(internal+8,rings+24);put(internal+16,rings+24);put(internal+0x18,bits);put(internal+0x20,uint32_t(0));put(internal+0x28,bits);put(internal+0x30,uint32_t(1));put(internal+0x38,bits+4);put(bits,uint32_t(1));
    put(rings,points);put(rings+8,points+16);put(rings+16,points+16);put(points,std::array<int64_t,2>{11,-7});
    put(section,image+0x16fef20);put(section+0x40,image+0x1746160);put(section+0x48,vertices);put(section+0x50,int32_t(1));put(section+0x54,int32_t(1));put(vertices,std::array<uint32_t,3>{0,0,0});
    Shape sampled;size_t units=0;assert(read_shape(read,image,shape,sampled,units,error));assert(sampled.rings.size()==1&&sampled.rings[0].hole&&sampled.rings[0].points[0][1]==-7&&sampled.sections.size()==1);
    put(internal+0x30,uint32_t(2));units=0;assert(!read_shape(read,image,shape,sampled,units,error));
    put(internal+0x30,uint32_t(1));int visits=0;
    const auto changing=[&]<class T>(uintptr_t address,size_t offset,T& value,std::string& why){if(address==section&&offset==0x48&&++visits==2)put(section+0x48,vertices+12);return read(address,offset,value,why);};
    units=0;assert(!read_shape(changing,image,shape,sampled,units,error));
    put(section+0x48,vertices);
    const uintptr_t manager=0x30000,islands=0x31000,cells=0x32000,cell_members=0x33000,links=0x34000,destinations=0x35000,region=0x36000,building=0x37000;
    put(manager,uintptr_t(0x5000));put(manager+0x10,image+0x1704bf0);put(manager+0x18,islands);put(manager+0x20,int32_t(1));put(manager+0x24,int32_t(1));put(islands,shape);
    put(manager+0x28,image+0x17471e0);put(manager+0x30,cells);put(manager+0x38,int32_t(1));put(manager+0x3c,int32_t(1));put(manager+0x40,uint32_t(0x3f800000));put(manager+0x44,uint32_t(0));put(manager+0x4c,uint32_t(0));put(manager+0x50,int32_t(1));put(manager+0x54,int32_t(1));put(manager+0x68,image+0x1749040);
    put(cells,image+0x1748fe0);put(cells+8,cell_members);put(cells+0x10,int32_t(1));put(cells+0x14,int32_t(1));put(cell_members,shape);
    const uintptr_t second_shape=shape+0x70;put(second_shape,image+0x16e0140);put(second_shape+0x18,internal);put(second_shape+0x20,uint8_t(1));put(second_shape+0x24,std::array<uint32_t,6>{});put(second_shape+0x40,image+0x16eb080);put(second_shape+0x48,sections);put(second_shape+0x50,int32_t(1));put(second_shape+0x54,int32_t(1));
    put(shape+0xe0,image+0x17477e0);put(shape+0xe8,links);put(shape+0xf0,int32_t(1));put(shape+0xf4,int32_t(1));put(shape+0xf8,region);
    put(links,image+0x17477b0);put(links+8,destinations);put(links+0x10,int32_t(1));put(links+0x14,int32_t(1));put(links+0x18,building);put(links+0x20,uintptr_t(0));put(links+0x28,uintptr_t(0));put(links+0x30,int32_t(-1));put(links+0x34,uint8_t(1));
    put(destinations,shape);put(destinations+8,int32_t(2));put(destinations+12,int32_t(8));
    OwnerKeys region_keys{{region,"region:1"}},building_keys{{building,"building:1"}},empty;std::map<std::string,SourceBuilding> source_samples{{"building:1",{"building:1",{}}}};source_samples["building:1"].services.spawn_type="spawn";Graph sampled_graph;
    assert(read_graph(read,image,manager,"world","building:1",region_keys,building_keys,empty,empty,source_samples,sampled_graph,error));assert(sampled_graph.islands.size()==1&&sampled_graph.islands[0].links[0].native_flag==1);assert(projection(sampled_graph,changed,error));
    std::vector<std::string> discovered;assert(discover_sources(read,image,manager,building_keys,discovered,error)&&discovered==std::vector<std::string>{"building:1"});
    for(const auto [address,base,actual]:std::vector<std::tuple<uintptr_t,uintptr_t,uintptr_t>>{{manager+0x10,0x1704be0,0x1704bf0},{manager+0x28,0x17471d0,0x17471e0},{manager+0x68,0x1749030,0x1749040},{cells,0x1748fd0,0x1748fe0}}){put(address,image+base);assert(!read_graph(read,image,manager,"world","building:1",region_keys,building_keys,empty,empty,source_samples,sampled_graph,error));if(address==manager+0x10)assert(!discover_sources(read,image,manager,building_keys,discovered,error));put(address,image+actual);}
    put(destinations,uintptr_t(0x99999));assert(!read_graph(read,image,manager,"world","building:1",region_keys,building_keys,empty,empty,source_samples,sampled_graph,error));
    struct Calls{int locks=0,unlocks=0;bool ok_lock=true,ok_unlock=true;bool lock(uintptr_t,std::string&){++locks;return ok_lock;}bool unlock(uintptr_t,std::string&){++unlocks;return ok_unlock;}} calls;
    bool unknown=false;assert(!with_read_lock(calls,manager,unknown,[](std::string& why){why="fixture read failed";return false;},error));assert(!unknown&&calls.locks==1&&calls.unlocks==1&&error=="fixture read failed");
    calls.ok_unlock=false;assert(!with_read_lock(calls,manager,unknown,[](std::string&){return true;},error)&&unknown);const int count_before=calls.locks;assert(!with_read_lock(calls,manager,unknown,[](std::string&){return true;},error)&&calls.locks==count_before);
    const uintptr_t source=0x78000,template_shard=0x71000,instance_shard=0x72000,instance_region=0x75000;
    const auto text=[](uintptr_t,std::string&,std::string&){return true;};const auto destination=[](uintptr_t,std::string&,std::string&){return true;};const auto find=[](uint64_t,uintptr_t&,std::string&){return true;};
    std::map<std::string,SourceBuilding> sampled_services;std::vector<ServiceSourcePin> pins;std::vector<BuildingSource> foreign_sources{{source,instance_shard,instance_region,42,"building:1"}};
    assert(!source_services(read,text,destination,find,image,template_shard,foreign_sources,sampled_services,pins,error));
    const uintptr_t rng=0x7a000;put(image+0x164b250,rng);put(rng,std::array<uint64_t,4>{1,2,3,4});put(rng+0x20,int32_t(7));
    const auto hash=[](const std::string&){return std::string(64,'a');};mt2mp::action_session::ManagementPriceWitness witness;
    assert(price_witness(read,hash,image,graph,witness,error)&&witness.random[3]==4&&witness.random_extra==7&&witness.graph_sha256==std::string(64,'a'));
    int rng_reads=0;const auto moving_rng=[&]<class T>(uintptr_t address,size_t offset,T& value,std::string& why){if(address==rng&&++rng_reads==2)put(rng,std::array<uint64_t,4>{1,2,3,5});return read(address,offset,value,why);};
    assert(!price_witness(moving_rng,hash,image,graph,witness,error));
}
