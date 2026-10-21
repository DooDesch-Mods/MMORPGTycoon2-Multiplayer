#include "live_host_state/network/node_state.h"
namespace gizmo_network_fixture {
namespace network=mt2mp::game::live_host_state::network;
struct Port final:network::Port {
    network::State actual;bool admitted=true;int fail_field=-1;unsigned reads=0,writes=0,preflights=0;
    bool read(network::State& out,std::string&)override{++reads;out=actual;return true;}
    bool preflight(const network::State& before,const network::State&,std::string& error)override{++preflights;if(!admitted||before!=actual){error="original Manager current owner changed";return false;}return true;}
    bool store(size_t,size_t,uint32_t,uint32_t,std::string& error)override{error="unexpected Node write";return false;}
    bool notify_supply(size_t,std::string& error)override{error="unexpected supply notification";return false;}
    bool store_manager(size_t index,size_t field,uint32_t before,uint32_t target,std::string& error)override{
        ++writes;if(int(field)==fail_field){error="original Manager CAS rejected";return false;}
        if(!actual.managers||index>=actual.managers->size()||(*actual.managers)[index].values[field]!=before){error="original Manager expected float bits changed";return false;}
        (*actual.managers)[index].values[field]=target;return true;
    }
};
inline void run(){
    std::string error;Port port;port.actual.other_graph_sha256=std::string(64,'a');
    port.actual.managers=std::vector<network::ManagerRow>{{7,{0x3f800000,0x40000000}}};
    auto target=port.actual;(*target.managers)[0].values={0x40400000,0x40800000};
    auto receipt=network::apply(port,target);need(receipt.confirmed&&!receipt.unknown&&receipt.completed_stores==2&&port.writes==2&&port.actual==target,"complete actual Manager desired/used adoption/readback");
    auto encoded=network::encode(target);network::State decoded;need(network::decode(encoded,decoded,error)&&decoded==target,"Manager float32 bits survive current wire");
    encoded.erase("managers");need(network::decode(encoded,decoded,error)&&!decoded.managers,"old exact Network wire remains supported");
    encoded["extra"]=1;need(!network::decode(encoded,decoded,error),"old Network exact fields reject extras");
    port={};port.actual=target;port.admitted=false;auto changed=target;(*changed.managers)[0].values[0]=0;
    receipt=network::apply(port,changed);need(!receipt.confirmed&&!receipt.attempted&&port.writes==0&&receipt.cause=="original Manager current owner changed","foreign current owner rejects before first Manager store");
    port={};port.actual=target;port.fail_field=1;changed=target;(*changed.managers)[0].values={0,0};
    receipt=network::apply(port,changed);need(receipt.unknown&&!receipt.confirmed&&receipt.completed_stores==1&&receipt.cause.find("original Manager CAS rejected")!=std::string::npos,"partial Manager writes stay Unknown with original cause");
    port={};port.actual=target;changed=target;(*changed.managers)[0].region=8;
    receipt=network::apply(port,changed);need(!receipt.attempted&&port.writes==0,"foreign Manager RegionID cannot be adopted");
    network::State pending=target;pending.nodes.push_back({7,0,{2,1,0,0},{0,0,0}});
    need(!network::valid(pending,error)&&network::valid_action105_effect(pending,error),"only scoped Action105 effect accepts unfinished current, strict Frame guard preserved");
    need(!network::decode(network::encode(pending),decoded,error)&&network::decode(network::encode(pending),decoded,error,true)&&decoded==pending,"pending-effect decode is explicit and unavailable to Frame default");
    (*pending.managers)[0].values[1]=0x7fc00000;
    need(!network::valid_action105_effect(pending,error),"pending105 never bypasses finite Manager guards");
}
}
