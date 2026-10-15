#include "tutorial_replication/message_progress.h"
#include "tutorial_replication/message_progress_contract.h"
#include "live_host_state/state.h"
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace mt2mp::game::tutorial_replication;
namespace progress=mt2mp::game::tutorial_replication::message_progress;
namespace {
constexpr uintptr_t image=0x10000000,sequence=0x20000,step=0x21000,window=0x22000,meter=0x23000,entries=0x24000;
void require(bool result,const char* cause){if(!result)throw std::runtime_error(cause);}
unsigned passed=0;
void pass(const char* name){++passed;std::cout<<"PASS "<<name<<'\n';}
std::vector<unsigned char> synthetic_code(unsigned char seed){
    std::vector<unsigned char> bytes(progress::function_size);
    for(size_t i=0;i<bytes.size();++i)bytes[i]=static_cast<unsigned char>((i*17+seed)%251);
    return bytes;
}
const std::string label_digest=mt2mp::sha256(synthetic_code(11)),value_digest=mt2mp::sha256(synthetic_code(29));
const progress::Fingerprints fixture_fingerprints{label_digest.c_str(),value_digest.c_str()};
template<class Memory,class Calls>bool apply(Memory& memory,uintptr_t base,uintptr_t seq,int32_t expected,
    const std::optional<MessageProgress>& target,Calls& calls,bool& attempted,std::string& error){
    return progress::apply(memory,base,seq,expected,target,calls,attempted,error,fixture_fingerprints);
}
struct Memory {
    std::map<uintptr_t,std::vector<unsigned char>> blocks;
    std::function<void(uintptr_t)> on_copy;
    bool copy(uintptr_t address,void* out,size_t size,std::string& error){
        if(on_copy)on_copy(address);
        for(const auto& [start,data]:blocks)if(address>=start&&address-start<=data.size()&&size<=data.size()-(address-start)){
            std::memcpy(out,data.data()+(address-start),size);return true;
        }
        error="fixture unreadable native field";return false;
    }
    template<class T>void put(uintptr_t owner,size_t offset,const T& value){
        auto& data=blocks.at(owner);require(offset<=data.size()&&sizeof(value)<=data.size()-offset,"fixture write outside owner");
        std::memcpy(data.data()+offset,&value,sizeof(value));
    }
    Memory(){
        for(const auto owner:{sequence,step,window,meter,entries})blocks[owner].resize(0x400);
        put(sequence,0,image+0x16dbd80);put(sequence,0x20,entries);put(sequence,0x28,int32_t(6));put(sequence,0x2c,int32_t(6));
        put(sequence,0x48,step);put(sequence,0x50,int32_t(6));put(entries,5*8,step);
        put(step,0,image+0x16fda00);put(step,0x1a8,window);put(step,0x1b0,meter);
        put(window,0,image+0x17883d0);put(window,0x69,uint8_t(1));
        put(meter,0,image+0x174a110);put(meter,0x280,window);put(meter,0x69,uint8_t(0));
        put(meter,0x3a4,0.0f);put(meter,0x3a8,0.0f);put(meter,0x3ac,100.0f);
        blocks[image+0x840f20]=synthetic_code(11);
        blocks[image+0x842ff0]=synthetic_code(29);
    }
};
struct Calls {
    Memory& memory;std::string order;bool fail_label=false,fail_value=false,wrong_readback=false,change_owner=false;
    bool label(uintptr_t target,std::string& error){require(target==meter,"wrong label target");order+='L';if(fail_label){error="original label call failure";return false;}return true;}
    bool shown(uintptr_t target,bool value,std::string&){require(target==meter,"wrong shown target");order+='S';memory.put(meter,0x69,uint8_t(value));return true;}
    bool value(uintptr_t target,const MessageProgress& value,std::string& error){
        require(target==meter,"wrong value target");order+='V';if(fail_value){error="original value call failure";return false;}
        memory.put(meter,0x3a4,wrong_readback?value.value+1:value.value);memory.put(meter,0x3a8,value.minimum);memory.put(meter,0x3ac,value.maximum);
        if(change_owner)memory.put(meter,0x280,sequence);return true;
    }
};
State sample(){Cursor cursor{"initial_tutorial","wasd",107,6,false,{"welcome"}};cursor.message_progress=MessageProgress{true,25,0,100};return {{cursor},false,false};}
void codec(){
    std::string error;State decoded;const auto source=sample();
    require(decode(encode(source),decoded,error)&&decoded==source,"new progress codec roundtrip differs");pass("new progress fields survive native-float payload roundtrip");
    auto hidden=source;hidden.active[0].message_progress->shown=false;hidden.active[0].message_progress->value=73;
    require(decode(encode(hidden),decoded,error)&&decoded==hidden,"hidden Host meter values were discarded");pass("hidden actual Host values remain payload state");
    auto old=encode(source);old["active"][0].erase("message_progress");
    require(decode(old,decoded,error)&&!decoded.active[0].message_progress,"old meter-absent cursor rejected");pass("meter-absent old cursor remains valid");
    for(const char* field:{"value","minimum","maximum"}){
        auto invalid=encode(source);invalid["active"][0]["message_progress"][field]=0;
        require(!decode(invalid,decoded,error)&&!error.empty(),"integer-shaped progress float accepted");
    }pass("progress float fields require exact float JSON type");
    auto overflow=encode(source);overflow["active"][0]["message_progress"]["value"]=1e300;
    require(!decode(overflow,decoded,error)&&!error.empty(),"overflowed wire float accepted");pass("wire float overflow rejects with its cause");
    for(const auto& value:{nlohmann::json(1),nlohmann::json(nullptr),nlohmann::json("true")}){
        auto invalid=encode(source);invalid["active"][0]["message_progress"]["shown"]=value;
        require(!decode(invalid,decoded,error),"nonboolean shown accepted");
    }pass("shown requires exact boolean type");
    auto extra=encode(source);extra["active"][0]["message_progress"]["pointer"]=123;
    require(!decode(extra,decoded,error),"extra progress pointer field accepted");pass("progress fields are exact and pointer-free");
    for(const float invalid:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        auto bad=source;bad.active[0].message_progress->value=invalid;
        require(!valid(bad,error)&&!error.empty(),"nonfinite native progress accepted");
    }pass("nonfinite progress rejects before frame encoding");
    auto reversed=source;reversed.active[0].message_progress->minimum=101;
    require(!valid(reversed,error),"reversed native meter range accepted");pass("reversed native range rejects");
    auto no_step=source;no_step.active[0].next=0;
    require(!valid(no_step,error)&&!error.empty(),"progress without indexed current step accepted");pass("missing active step reports a cause");
    mt2mp::game::live_host_state::Frame frame;frame.session=1;frame.epoch=2;frame.source_generation=3;frame.revision=4;
    frame.snapshot="11111111-1111-4111-8111-111111111111";frame.values.calendar={1,1,1,0,0,7};frame.tutorial=source;
    std::vector<uint8_t> bytes;mt2mp::game::live_host_state::Frame received;
    require(mt2mp::game::live_host_state::encode(frame,bytes,error)&&mt2mp::game::live_host_state::decode(bytes.data(),bytes.size(),received,error)&&received==frame,"new progress exceeds existing schema9 Host-frame shape/depth");
    pass("existing authenticated schema9 Host frame carries progress");
    mt2mp::action_session::Context context{1,2,1,3,mt2mp::action_session::Role::Client,frame.snapshot};
    auto conflicting=received;conflicting.tutorial->active[0].message_progress->value=26;
    require(mt2mp::game::live_host_state::admit(1,context,2,3,&received,conflicting,0,0,false,error)==mt2mp::game::live_host_state::Admission::Rejected,"same-revision conflicting progress accepted");
    pass("same Host revision cannot change only progress");
}
void native_presentation(){
    std::string error;Memory memory;progress::Binding out;
    require(progress::read(memory,image,sequence,6,out,error)&&out.values==MessageProgress{false,0,0,100},"hidden current Host meter was not sampled");
    pass("actual hidden meter shown/value/min/max sampled");
    require(progress::verify_code(memory,image,error,fixture_fingerprints),"complete synthetic code digests rejected");
    memory.blocks.at(image+0x842ff0).back()^=1;
    require(!progress::verify_code(memory,image,error,fixture_fingerprints),"modified suffix accepted despite intact prologue");pass("complete-body fingerprint rejects suffix mutation");
    Memory production;
    require(!progress::verify_code(production,image,error)&&error=="tutorial progress native SetLabel body differs","production contract accepted synthetic fixture");
    pass("default production fingerprints reject synthetic test functions");
    for(const auto address:{image+0x840f20,image+0x842ff0}){
        Memory truncated;truncated.blocks.at(address).pop_back();
        require(!progress::verify_code(truncated,image,error,fixture_fingerprints)&&error=="fixture unreadable native field","full size or original read failure lost");
    }pass("both complete function sizes required and read cause preserved");
    const auto broken_hash=[](const std::vector<unsigned char>&)->std::string{throw std::runtime_error("fixture SHA provider failed");};
    require(!progress::verify_code(production,image,error,fixture_fingerprints,broken_hash)&&error=="fixture SHA provider failed","original hash failure lost");
    pass("hash failure retains original cause");
    Calls bad_code{memory};bool no_attempt=false;
    require(!apply(memory,image,sequence,6,MessageProgress{false,73,0,100},bad_code,no_attempt,error)&&!no_attempt&&bad_code.order.empty(),"hidden presentation mutated before full code guard");
    pass("full code guard runs before hidden or visible UI effects");
    memory=Memory{};Calls calls{memory};bool attempted=false;
    const auto sequence_before=memory.blocks.at(sequence),step_before=memory.blocks.at(step);
    require(apply(memory,image,sequence,6,MessageProgress{true,25,0,100},calls,attempted,error)&&attempted&&calls.order=="LSV","native presentation order/readback differs");
    require(memory.blocks.at(sequence)==sequence_before&&memory.blocks.at(step)==step_before,"presentation advanced/completed tutorial step or changed predicate accumulator");
    pass("Host display applied without Sequence/Step Update, progress accumulator or completion writes");
    attempted=false;calls.order.clear();
    require(apply(memory,image,sequence,6,MessageProgress{true,25,0,100},calls,attempted,error)&&!attempted&&calls.order.empty(),"identical meter state replayed native calls");
    pass("same Host progress has no replay");
    require(apply(memory,image,sequence,6,MessageProgress{false,73,0,100},calls,attempted,error)&&calls.order=="SV","hidden actual fields or visibility not applied");
    pass("hidden target still adopts measured scalars without label callback");
    for(unsigned broken=0;broken<8;++broken){
        Memory invalid;Calls rejected{invalid};attempted=false;
        if(broken==0)invalid.put(sequence,0x48,window);
        if(broken==1)invalid.put(entries,0,step);
        if(broken==2)invalid.put(meter,0,image+0x174a111);
        if(broken==3)invalid.put(window,0,image+0x17883d1);
        if(broken==4)invalid.put(meter,0x280,sequence);
        if(broken==5)invalid.put(meter,0x69,uint8_t(2));
        if(broken==6)invalid.put(sequence,0x50,int32_t(5));
        if(broken==7)invalid.put(step,0x18,uint8_t(2));
        require(!apply(invalid,image,sequence,6,MessageProgress{true,25,0,100},rejected,attempted,error)&&!attempted&&rejected.order.empty()&&!error.empty(),"invalid native ownership allowed a UI call");
    }pass("wrong step/index/AP/Window/Meter owner/boolean reject before calls");
    memory=Memory{};memory.put(step,0x1b0,uintptr_t(0));calls.order.clear();attempted=false;
    require(apply(memory,image,sequence,6,std::nullopt,calls,attempted,error)&&!attempted,"meter-absent Message step was fabricated");
    require(!apply(memory,image,sequence,6,MessageProgress{true,25,0,100},calls,attempted,error)&&!attempted,"Host meter presence mismatch accepted");
    pass("absent Message meter stays valid; mismatched Host presence rejects");
    memory=Memory{};unsigned reads=0;memory.on_copy=[&](uintptr_t address){if(address==meter+0x3a4&&++reads==2)memory.put(meter,0x3a4,1.0f);};
    require(!progress::read(memory,image,sequence,6,out,error),"changed value between native reads accepted");pass("double read rejects changed actual progress");
    memory=Memory{};calls.fail_label=true;calls.order.clear();attempted=false;
    require(!apply(memory,image,sequence,6,MessageProgress{true,25,0,100},calls,attempted,error)&&attempted&&error=="original label call failure"&&calls.order=="L","original label failure hidden or retried");
    pass("original label failure and attempted state preserved");
    memory=Memory{};calls.fail_label=false;calls.fail_value=true;calls.order.clear();attempted=false;
    require(!apply(memory,image,sequence,6,MessageProgress{true,25,0,100},calls,attempted,error)&&attempted&&error=="original value call failure"&&calls.order=="LSV","partial value failure hidden or retried");
    pass("partial native value failure retained for existing Unknown fence");
    memory=Memory{};calls.fail_value=false;calls.wrong_readback=true;calls.order.clear();attempted=false;
    require(!apply(memory,image,sequence,6,MessageProgress{true,25,0,100},calls,attempted,error)&&attempted,"wrong native UI readback acknowledged");
    pass("wrong native readback rejects acknowledgement");
    memory=Memory{};calls.wrong_readback=false;calls.change_owner=true;calls.order.clear();attempted=false;
    require(!apply(memory,image,sequence,6,MessageProgress{true,25,0,100},calls,attempted,error)&&attempted&&!error.empty(),"changed Window ownership during native presentation acknowledged");
    pass("native Window ownership change after an attempted call rejects acknowledgement");
}
}
int main(){try{codec();native_presentation();std::cout<<passed<<" progress-only scenarios passed\n";return 0;}catch(const std::exception& cause){std::cerr<<"error: tutorial progress: "<<cause.what()<<'\n';return 1;}}
