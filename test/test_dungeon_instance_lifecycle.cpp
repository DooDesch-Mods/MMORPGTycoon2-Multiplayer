#include "dungeon_instance_lifecycle.h"
#include <iostream>
#include <stdexcept>

namespace live=mt2mp::game::dungeon_instance_lifecycle;
const std::string uuid="12345678-1234-1234-1234-123456789abc";
const std::string exterior="action:"+uuid;
unsigned checks=0;
void require(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
live::Event make(){return {live::Effect::Make,exterior,uuid,1,false,37,37,38,{2,9},{2,9,37}};}
struct Native final:live::Port {
    live::Pin state;
    unsigned calls=0,reads=0,witnesses=0;
    bool witnessed=false,witness_failure=false,invoke_failure=false,replace_survivor=false,wrong_return=false,change_owner=false;
    uintptr_t selected_target=0;
    Native(){
        state.origin={11,12,13,uuid};state.local_epoch=21;state.local_generation=22;
        state.exterior_uid=123;state.root=0x1000;state.exterior=0x2000;state.dungeon=0x3000;
        state.key=exterior;state.counter=7;
        state.members={{2,0x4000,0x5000,0x6000},{9,0x7000,0x8000,0x9000}};
    }
    bool current(const std::string& key,live::Pin& out,std::string& error)override{
        ++reads;if(key!=state.key){error="actual exterior missing";return false;}out=state;error.clear();return true;
    }
    bool observed(const live::Event&,bool& found,std::string& error)override{
        ++witnesses;found=false;if(witness_failure){error="actual action receipt lost";return false;}
        found=witnessed;error.clear();return true;
    }
    bool invoke(const live::Pin&,const live::Event& event,uintptr_t target,live::Receipt& receipt,std::string& error)override{
        ++calls;selected_target=target;
        if(state.counter!=event.counter_before){state.counter=event.counter_before;++receipt.counter_stores;}
        if(invoke_failure){error="original Instance operation failed";return false;}
        if(event.effect==live::Effect::Make){
            live::Member born{event.index,0xa000,0xb000,0xc000};state.members.push_back(born);
            state.counter=event.counter_after;receipt.made_instance=wrong_return?0xd000:born.instance;
        }else if(event.effect==live::Effect::Destroy){std::erase_if(state.members,[&](const auto& row){return row.instance==target;});}
        if(replace_survivor)state.members.front().instance=0xd000;
        if(change_owner)++state.local_generation;
        receipt.returned=true;error.clear();return true;
    }
};
int main()try{
    std::string error;const auto birth=make();
    require(live::valid(birth,error),"actual append/index transition valid");
    require(live::decode_event(live::encode(birth))==birth,"exact portable lifecycle round trip");
    auto value=birth;value.counter_after=39;require(!live::valid(value,error),"counter cannot skip a native index");
    value=birth;value.index=38;require(!live::valid(value,error),"Make uses original counter, not an invented ID");
    value=birth;value.after={9,2,37};require(!live::valid(value,error),"Make preserves original member order");
    value=birth;value.before={2,2};require(!live::valid(value,error),"duplicate current index rejected");
    value=birth;value.ordinal=0;require(!live::valid(value,error),"missing original operation ordinal rejected");
    value=birth;value.operation="invalid";require(!live::valid(value,error),"unbound operation rejected");
    value=birth;value.exterior="unknown";require(!live::valid(value,error),"foreign exterior key rejected");
    value=birth;value.effect=live::Effect(5);require(!live::valid(value,error),"unknown effect rejected");
    for(const char* field:{"effect","ordinal","index","counter_before","counter_after"}){
        auto encoded=live::encode(birth);encoded[field]=-1;bool rejected=false;
        try{(void)live::decode_event(encoded);}catch(const std::exception&){rejected=true;}
        require(rejected,"negative wire integer rejected");
    }
    for(const char* field:{"extra","action"}){
        auto encoded=live::encode(birth);encoded[field]=1;bool rejected=false;
        try{(void)live::decode_event(encoded);}catch(const std::exception&){rejected=true;}
        require(rejected,"wire fields and Boolean remain exact");
    }
    live::Receipt receipt;
    {Native native;live::Executor executor(native);
        require(executor.apply(birth,receipt,error),"original Make accepted after complete readback");
        require(receipt.confirmed&&receipt.calls==1&&native.calls==1&&receipt.counter_stores==1,"one Make adopts genuine Host counter");
        require(native.state.members.front().instance==0x4000&&native.state.counter==38,"existing allocation survives counter divergence");
        require(!receipt.observed_action&&native.witnesses==0,"simulation Make requires actual native call");}
    for(const auto effect:{live::Effect::Destroy,live::Effect::Reset,live::Effect::ForcePlayersOut}){
        Native native;live::Executor executor(native);value=birth;value.effect=effect;value.index=9;
        value.counter_before=7;value.counter_after=7;value.after=effect==live::Effect::Destroy?std::vector<int32_t>{2}:value.before;
        require(live::valid(value,error)&&executor.apply(value,receipt,error),"original destroy/reset/force-out confirmed");
        require(native.calls==1&&native.selected_target==0x7000,"native index resolves actual pointer, not array slot");
        require(native.state.members.front().instance==0x4000,"surviving native allocation retained");
    }
    {Native native;live::Executor executor(native);value=birth;value.before={9,2};value.after={9,2,37};
        require(!executor.apply(value,receipt,error)&&native.calls==0&&!receipt.attempted,"foreign pre-state rejected before original call");}
    {Native native;live::Executor executor(native);value=birth;value.action=true;native.witnessed=true;native.state.members.clear();
        require(executor.apply(value,receipt,error)&&receipt.observed_action&&receipt.confirmed,"actual action receipt confirms existing side effect");
        require(native.calls==0&&native.reads==0&&native.witnesses==1,"destroyed parent does not cause replay after actual action");}
    {Native native;live::Executor executor(native);value=birth;value.action=true;native.witness_failure=true;
        require(!executor.apply(value,receipt,error)&&native.calls==0&&!receipt.attempted,"lost action receipt cannot become success or retry");}
    {Native native;live::Executor executor(native);native.invoke_failure=true;
        require(!executor.apply(birth,receipt,error)&&receipt.unknown&&receipt.attempted&&!receipt.confirmed,"failed original call remains Unknown");
        require(error.find("original Instance operation failed")!=std::string::npos,"original failure cause retained");
        native.invoke_failure=false;require(!executor.apply(birth,receipt,error)&&native.calls==1,"Unknown cannot repeat the original call");}
    {Native native;live::Executor executor(native);native.wrong_return=true;
        require(!executor.apply(birth,receipt,error)&&receipt.unknown,"Make must return the actual appended allocation");}
    {Native native;live::Executor executor(native);native.replace_survivor=true;
        require(!executor.apply(birth,receipt,error)&&receipt.unknown,"matching indices cannot conceal replacement of a surviving object");}
    {Native native;live::Executor executor(native);native.change_owner=true;
        require(!executor.apply(birth,receipt,error)&&receipt.unknown,"world generation change cannot confirm an original call");}
    const live::journal::Origin origin{11,12,13,uuid};live::journal::Host host;live::journal::Client client;
    require(host.bind(origin,1,error)&&client.bind(origin,error),"existing journal binds both peers");
    require(live::append(host,origin,birth,error),"original lifecycle joins existing journal");
    const auto batch=host.pending();require(live::valid(batch,error),"canonical lifecycle batch valid");
    require(live::event(batch.rows.front())==birth,"retained journal decodes same original operation");
    std::vector<live::journal::Row> work;
    require(client.prepare(origin,batch,work,error)&&work.size()==1,"one original call planned");
    require(client.confirm(origin,batch,work,error),"native applied receipt commits existing journal");
    require(client.prepare(origin,batch,work,error)&&work.empty(),"repeated Frame does not repeat native operation");
    require(host.sent(origin,1,batch,error)&&host.acknowledge(origin,1,1,error)&&host.pending().rows.empty(),"existing full Frame ACK retires operation");
    auto malformed=batch;malformed.rows.front().message=" "+malformed.rows.front().message;
    require(!live::valid(malformed,error),"noncanonical operation rejected");
    malformed=batch;auto encoded=live::encode(birth);encoded["after"]={{{{{1}}}}};malformed.rows.front().message=encoded.dump();
    require(!live::valid(malformed,error),"nested payload rejected before native operation");
    std::cout<<checks<<" Dungeon Instance lifecycle checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
