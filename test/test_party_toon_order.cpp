#include "subscriber_lifecycle/party_toon_order.h"
#include "subscriber_lifecycle/reference_map.h"
#include "subscriber_lifecycle/cohort.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>

namespace local=mt2mp::subscriber_lifecycle;
namespace stage=local::first_append;
size_t checks=0;
void require(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
struct Memory final:local::Memory {
    std::map<uintptr_t,unsigned char> bytes;
    template<class T>void put(uintptr_t p,T value){const auto* source=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[p+i]=source[i];}
    bool read(uintptr_t p,void* out,size_t count,std::string& error)override{
        auto* target=static_cast<unsigned char*>(out);
        for(size_t i=0;i<count;++i){if(!bytes.contains(p+i)){error="native fixture read failure";return false;}target[i]=bytes.at(p+i);}return true;
    }
};
struct Parties final:stage::PartyReferences {
    std::string key="action:11111111-1111-4111-8111-111111111111:party:0";
    uintptr_t pointer=0x400000;
    bool valid=true;
    bool identify(uintptr_t p,std::string& out,std::string& error)override{
        if(!valid||p!=pointer){error="original Party full UID changed";return false;}out=key;return true;
    }
    bool resolve(const std::string& value,uintptr_t& out,std::string& error)override{
        if(!valid||value!=key){error="original Party full UID changed";return false;}out=pointer;return true;
    }
};
void references(){
    Memory memory;Parties parties;const uintptr_t image=0x140000000,state=0x200000,map=0x300000;
    memory.put(state,image+0x16de920);memory.put(map,image+0x1742860);memory.put(map+0x1c0,image+0x174ad20);memory.put(map+0x208,int32_t(0));
    memory.put(parties.pointer,image+0x174a370);
    stage::ReferenceWorld world{image,state,map,nullptr,nullptr,&parties};stage::ReferenceKey key;std::string error;uintptr_t pointer=0;
    require(stage::identify_reference(memory,world,parties.pointer,-1,-1,key,error)&&key.kind==stage::ReferenceKind::SharedParty&&key.party_key==parties.key,"identify actual Party AP through canonical provider");
    require(stage::resolve_reference(memory,world,key,pointer,error)&&pointer==parties.pointer,"resolve Party canonical key");
    parties.pointer+=0x10000;memory.put(parties.pointer,image+0x174a370);
    require(stage::resolve_reference(memory,world,key,pointer,error)&&pointer==parties.pointer,"same portable key resolves different native Client object");
    parties.valid=false;
    require(!stage::resolve_reference(memory,world,key,pointer,error)&&error=="original Party full UID changed","preserve original provider cause");
    parties.valid=true;world.parties=nullptr;
    require(!stage::resolve_reference(memory,world,key,pointer,error)&&error=="Party reference has no current native bindings","missing Party provider fails without alias fallback");
    world.parties=&parties;
    for(int bad=0;bad<7;++bad){auto invalid=key;
        switch(bad){case 0:invalid.party_key="party:0";break;case 1:invalid.region_id=0;break;case 2:invalid.subscriber_id=1;break;case 3:invalid.npc_key="foreign";break;case 4:invalid.building_key="foreign";break;case 5:invalid.sequence_id=0;break;case 6:invalid.kind=stage::ReferenceKind::RootShard;break;}
        require(!stage::resolve_reference(memory,world,invalid,pointer,error)&&!error.empty(),"reject malformed Party/foreign key fields");
    }
    memory.put(parties.pointer,image+0x1744e90);
    require(!stage::resolve_reference(memory,world,key,pointer,error),"provider cannot substitute a Toon for actual Party");
}
void ordering(){
    for(int failure=-1;failure<6;++failure){
        stage::PartyToonOrderReceipt receipt;std::string error;std::vector<int> calls;
        bool loaded[2]{},party_ready=false;uintptr_t backlink[2]{};
        const uintptr_t party=0xabcdef0012345678;
        const auto load=[&](size_t i,std::string& cause){calls.push_back(int(i));if(failure==int(i)){cause="original ArrayLoad rejected";return false;}
            loaded[i]=true;backlink[i]=1;return true;};
        const auto finish=[&](std::string& cause){calls.push_back(2);
            require(loaded[0]&&loaded[1],"all native constructor results exist before Party completion");
            // The temporary integer is cleared through the injected original
            // PropertyLoad operation before Party code can use it as a pointer.
            backlink[0]=backlink[1]=0;
            if(failure==2){cause="original Party Load rejected";return false;}
            backlink[0]=backlink[1]=party;party_ready=true;return true;};
        const auto resolve=[&](size_t i,std::string& cause){calls.push_back(3+int(i));
            require(party_ready&&backlink[i]==party,"Party complete reciprocal precedes Subscriber PostResolve");
            if(failure==3+int(i)){cause="original context D1 rejected";return false;}
            backlink[i]=1;
            require(backlink[i]==1,"re-arm own saved link ID before original Subscriber Resolve");
            backlink[i]=party;return true;};
        const bool ok=stage::party_toon_order(2,load,finish,resolve,receipt,error);
        if(failure<0||failure==5){require(ok&&calls==std::vector<int>({0,1,2,3,4}),"complete deferred native order");require(receipt.loads_returned==2&&receipt.parties_returned&&receipt.resolves_returned==2,"complete attempted/returned receipt");}
        else{require(!ok&&!error.empty(),"native failure retains original cause");require(calls.back()==failure,"no operation after native failure");
            require(calls.size()==size_t(failure+1),"no failed operation replay");
            if(failure<2)require(!receipt.parties_attempted&&!receipt.resolves_attempted,"failed ArrayLoad cannot enter Party or context Resolve");
            if(failure==2)require(receipt.parties_attempted&&!receipt.parties_returned&&!receipt.resolves_attempted,"failed Party cannot enter Subscriber Resolve");}
    }
    stage::PartyToonOrderReceipt receipt;std::string error;
    require(!stage::party_toon_order(0,{},{},{},receipt,error)&&!receipt.loads_attempted,"empty batch fails before native work");
    try{
        stage::party_toon_order(2,[](size_t,std::string&)->bool{throw std::runtime_error("original native C++ exception");},
            [](std::string&){return true;},[](size_t,std::string&){return true;},receipt,error);
        require(false,"native C++ exception must propagate");
    }catch(const std::runtime_error& ex){require(std::string(ex.what())=="original native C++ exception"&&receipt.loads_attempted==1&&!receipt.loads_returned&&!receipt.parties_attempted,"native C++ cause and attempted receipt retained");}
}
void independent_namespaces(){
    struct Context {uintptr_t object;};Context subscriber{0x10000},party{0x20000};
    const auto original=+[](void* context,int32_t id)->void*{return id==1?reinterpret_cast<void*>(static_cast<Context*>(context)->object):nullptr;};
    const auto fresh=+[](void*,const local::ExternalLink&,std::string&){return true;};
    local::LookupScope scope;std::string error;
    require(scope.arm(1,&subscriber,{1},{},fresh,nullptr,error),"arm Subscriber own ID space");
    require(scope.lookup(1,&subscriber,1,original)==reinterpret_cast<void*>(subscriber.object),"Subscriber saved ID 1 resolves its original object");
    require(scope.lookup(1,&party,1,original)==reinterpret_cast<void*>(party.object),"another native context retains its original same ID");
    scope.clear();require(scope.arm(1,&party,{1},{},fresh,nullptr,error),"Party independent ID space remains valid");
    require(scope.lookup(1,&party,1,original)==reinterpret_cast<void*>(party.object),"no scalar ID remapping");scope.clear();
}
int main(){try{references();ordering();independent_namespaces();std::cout<<checks<<" Party/Toon seam checks passed\n";return 0;}
catch(const std::exception& ex){std::cerr<<"failed after "<<checks<<": "<<ex.what()<<'\n';return 1;}}
