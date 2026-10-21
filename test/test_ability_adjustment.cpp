#include "ability_edit_fixture.h"
#include "ability_adjustment_view.h"
#include <iostream>
#include <limits>
#include <memory>
namespace adjustment_fixture {
namespace ui=mt2mp::game::ability_adjustment_view;
struct Calls {
    ui::State value{10,20,30,0,1,2};unsigned reads=0,preparations=0,originals=0,restores=0;
    int fail_read=0;bool prepare_ok=true,invoke_ok=true,restore_ok=true,drift=false,preserved=false;
    bool read(ui::State& out,std::string& error){++reads;if(int(reads)==fail_read){error="exact proposal read failed";return false;}out=value;if(drift&&reads==2)++out.index;return true;}
    bool prepare(const ui::State& desired,std::string& error){++preparations;value=desired;if(!prepare_ok){error="exact proposal preparation failed";return false;}return true;}
    bool restore(const ui::State& previous,std::string& error){++restores;if(!restore_ok){error="exact proposal restore failed";return false;}value=previous;return true;}
    bool invoke(bool preserve,std::string& error){++originals;preserved=preserve;if(!invoke_ok){error="exact original Confirm failed";return false;}if(!preserve)value.prototype=0;return true;}
};
}
int main()try{using namespace adjustment_fixture;unsigned checks=0;std::string error;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(std::string(why)+" cause="+error);};
    adjustment_mode=true;std::vector<uint8_t> bytes;auto decoded=std::make_unique<Message>();
    for(int32_t kind=0;kind<5;++kind)for(bool cost:{false,true})for(bool remove:{false,true}){
        character_kind=kind;adjustment_cost=cost;adjustment_remove=remove;auto m=std::make_unique<Message>(result());
        check(ability_edit_transition(m->request.ability_edit,m->evidence.ability_edit_before,m->evidence.ability_edit_after),"Exact original adjustment transition rejected");
        check(encode(*m,bytes,error)&&decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==*m,"Complete adjustment schema36 roundtrip failed");
        const auto document=nlohmann::json::parse(bytes);check(document.at("schema")==36,"Adjustment did not use schema36");
        for(unsigned bad=0;bad<10;++bad){auto j=document;
            if(bad==0)j["schema"]=35;if(bad==1)j["request"]["action"]=86;
            if(bad==2)j["request"]["ability_edit"]["adjustment"]["cost"]=1;
            if(bad==3)j["request"]["ability_edit"]["adjustment"]["index"]=-1;
            if(bad==4)j["request"]["ability_edit"]["adjustment"]["delta"]=0;
            if(bad==5)j["request"]["ability_edit"]["adjustment"]["foreign"]=0;
            if(bad==6)j["request"]["ability_edit"].erase("adjustment");
            if(bad==7)j["evidence"]["ability_edit_after"]["owner"]["edited"]=!m->evidence.ability_edit_before.owner.edited;
            if(bad==8)j["evidence"]["ability_edit_after"]["owner"]["release_edits"][7]=99;
            if(bad==9)j["evidence"]["ability_edit_after"]["record"]["text"][0]="Foreign rename";
            const auto text=j.dump();check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),*decoded,error),"Malformed or foreign adjustment admitted");}
        check(ability_edit_wire_fits(m->request,m->evidence.ability_edit_before,{},error),"Adjustment packet preflight failed");
        auto changed=std::make_unique<Message>(*m);++changed->evidence.after.cash;check(!encode(*changed,bytes,error),"Adjustment changed full Ledger");
        changed->evidence=m->evidence;++changed->evidence.ability_edit_after.owner.undo_count;check(!encode(*changed,bytes,error),"Adjustment invented native Undo history");
    }
    auto source=record();AbilityRecord target;
    check(ability_adjusted_record(source,{false,0,-3},target)&&target.effects.empty()&&target.costs==source.costs,"Original zero result did not retire only selected effect");
    source.effects.push_back(source.effects[0]);source.effects[1].count=33;
    check(ability_adjusted_record(source,{false,0,-3},target)&&target.effects.size()==1&&target.effects[0].count==33,"Original zero deletion lost stable surviving order");
    check(!ability_adjusted_record(source,{false,99,1},target),"Foreign effect index admitted");
    check(!ability_adjusted_record(source,{false,0,std::numeric_limits<float>::infinity()},target),"Nonfinite proposal admitted");
    check(!ability_adjusted_record(source,{false,0,-4},target),"Original SetAdjustment positive-kind sign assertion bypassed");
    source.effects[0].integers[0]=0;check(!ability_adjusted_record(source,{false,0,1},target),"Original SetAdjustment unsupported-kind assertion bypassed");
    adjustment_mode=false;auto legacy=std::make_unique<Message>(result());check(encode(*legacy,bytes,error),"Existing Apply encoding failed");
    auto old=nlohmann::json::parse(bytes);old["schema"]=35;old["request"]["ability_edit"].erase("adjustment");const auto text=old.dump();
    check(decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),*decoded,error)&&*decoded==*legacy,"Exact old schema35 Apply compatibility lost");
    for(unsigned scenario=0;scenario<8;++scenario){Calls calls;const auto previous=calls.value;auto prepared=previous;prepared.prototype=77;prepared.index=0;prepared.delta=1;bool attempted=false;
        if(scenario==1)calls.fail_read=1;if(scenario==2)calls.prepare_ok=false;if(scenario==3)calls.fail_read=2;
        if(scenario==4)calls.drift=true;if(scenario==5)calls.invoke_ok=false;if(scenario==6)calls.restore_ok=false;
        const bool preserve=scenario!=7;error.clear();const bool ok=ui::run(calls,prepared,preserve,attempted,error);
        if(scenario==0)check(ok&&attempted&&calls.originals==1&&calls.restores==1&&calls.value==previous&&calls.preserved,"Remote original-once did not preserve unrelated local proposal");
        if(scenario>=1&&scenario<=4)check(!ok&&!attempted&&!calls.originals&&!error.empty(),"Failed pre-effect UI admission entered original");
        if(scenario==2||scenario==3||scenario==4)check(calls.value==previous&&calls.restores==1,"Failed proposal setup was not restored");
        if(scenario==5)check(!ok&&attempted&&calls.originals==1&&calls.restores==1&&calls.value==previous&&error=="exact original Confirm failed","Original failure lost cause or proposal restoration");
        if(scenario==6)check(!ok&&attempted&&calls.originals==1&&error.find("exact proposal restore failed")!=std::string::npos,"Partial UI restore produced false success");
        if(scenario==7)check(ok&&attempted&&calls.originals==1&&!calls.restores&&!calls.value.prototype,"Initiating peer lost original presentation tail");
    }
    std::cout<<checks<<" actual Ability adjustment transition/codec/presentation-body checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
