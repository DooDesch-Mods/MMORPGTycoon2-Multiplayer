#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "object_ui_probe/contract.h"
#include "ui_remove/diagnostic/ui_diagnostic.h"
#include <iostream>
#include <stdexcept>
namespace contract=mt2mp::game::object_ui_probe;
namespace common=mt2mp::action_session::ui_diagnostic;
int object_checks=0;
void need(bool value,const char* cause){++object_checks;if(!value)throw std::runtime_error(cause);}
namespace actual_command {
struct Job {uint32_t attempted_mask=0,returned_mask=0,exception=0,cleanup_exception=0;bool command_accepted=false;};
#include "object_ui_command.inc"
inline int stage_fault=0,constructs=0,labels=0,commands=0,destroys=0;
inline bool label_result=true,command_result=true;
inline const char* expected_label="Duplicate";
void fault(int stage){if(stage_fault==stage)RaiseException(0xe0010000u+unsigned(stage),0,0,nullptr);}
void construct(void*,const char* label){++constructs;need(std::string(label)==expected_label,"actual original record label preserved");fault(1);}
bool label(const void*,const char* value){++labels;need(std::string(value)==expected_label,"actual original label equality uses exact command");fault(2);return label_result;}
bool invoke(void* window,void*){++commands;need(uintptr_t(window)==0x1234,"actual visible InfoWindow receiver preserved");fault(3);return command_result;}
void destroy(void*){++destroys;fault(4);}
void reset(){stage_fault=constructs=labels=commands=destroys=0;label_result=command_result=true;}
void checks(){
    common::Api api;api.construct=construct;api.label_equals=label;api.command=invoke;api.destroy=destroy;
    for(const char* name:{"Duplicate","Group","Ungroup"}){reset();expected_label=name;Job receipt;
        need(command(&api,0x1234,name,&receipt)==0&&receipt.command_accepted&&receipt.attempted_mask==60&&receipt.returned_mask==60,"actual command invokes and destroys one original Record");
        need(constructs==1&&labels==1&&commands==1&&destroys==1,"actual command has no replay or repeated native mutation");
    }
    reset();expected_label="Duplicate";label_result=false;Job mismatch;
    need(command(&api,0x1234,expected_label,&mismatch)==0&&!mismatch.command_accepted&&commands==0&&destroys==1,"failed genuine label skips UI mutation and still cleans Record");
    reset();command_result=false;Job refused;
    need(command(&api,0x1234,expected_label,&refused)==0&&!refused.command_accepted&&commands==1&&destroys==1,"original command false stays a rejection");
    for(int stage=1;stage<=4;++stage){reset();stage_fault=stage;Job receipt;const auto exception=command(&api,0x1234,expected_label,&receipt);
        need((stage==4?receipt.cleanup_exception:exception)==0xe0010000u+unsigned(stage),"actual original SEH cause preserved");
        need(destroys==(stage==1?0:1)&&commands<2,"fault neither destroys an unconstructed Record nor replays native command");
    }
}
}
int main()try{
    using contract::Json;const std::string id="12345678-1234-4234-8234-123456789abc";std::string error;contract::Input input;
    Json args={{"acceptance_token",id},{"snapshot_uuid",id},{"command","Duplicate"},{"record_keys",Json::array({"action:"+id})},{"expected_world_epoch",7},{"expected_generation",8},{"position",Json::array({1,2,3})}};
    need(contract::parse(args,input,error)&&input.position[2]==3,"actual Duplicate input schema");
    for(const char* command:{"Group","Ungroup"}){auto request=args;request["command"]=command;request.erase("position");need(contract::parse(request,input,error),"actual Group/Ungroup input schema");}
    for(int bad=0;bad<10;++bad){auto request=args;
        if(bad==0)request["acceptance_token"]="wrong";
        if(bad==1)request["snapshot_uuid"]="wrong";
        if(bad==2)request["expected_world_epoch"]=0;
        if(bad==3)request["expected_generation"]=true;
        if(bad==4)request["command"]="Move";
        if(bad==5)request["record_keys"].push_back(request["record_keys"][0]);
        if(bad==6)request["position"][0]=std::numeric_limits<double>::infinity();
        if(bad==7)request["position"][0]=true;
        if(bad==8)request["record_keys"]=Json::array();
        if(bad==9)request["other"]=1;
        need(!contract::parse(request,input,error)&&!error.empty(),"actual malformed request must fail before native effects");
    }
    const contract::Counters before{4,4,0,9},after{5,5,0,10};
    const auto complete=[&](contract::Counters measured,size_t count,bool confirmed,bool retired,bool pending,bool locked,bool same,int outcome,uint64_t elapsed,const std::string& cause){return contract::completion(before,measured,count,confirmed,retired,pending,locked,same,outcome,elapsed,cause,error);};
    need(complete(after,1,true,true,false,false,true,1,1,{})==contract::Completion::Confirmed,"one exact captured/submitted/retired authoritative operation confirms");
    need(complete(before,0,false,false,false,false,false,0,1,{})==contract::Completion::Waiting,"native return without capture is not success");
    need(complete(after,1,true,true,true,false,true,1,1,{})==contract::Completion::Waiting,"pending authoritative completion remains waiting");
    for(int bad=0;bad<10;++bad){auto measured=after;size_t count=1;bool locked=false,same=true;int outcome=1;std::string cause;
        if(bad==0)++measured.captures;
        if(bad==1)--measured.submitted;
        if(bad==2)++measured.rejected;
        if(bad==3)--measured.revision;
        if(bad==4)++measured.revision;
        if(bad==5)count=2;
        if(bad==6)locked=true;
        if(bad==7)same=false;
        if(bad==8)outcome=0;
        if(bad==9)cause="original authoritative failure";
        need(complete(measured,count,true,true,false,locked,same,outcome,1,cause)==contract::Completion::Unknown&&!error.empty(),"unmatched source/authority completion remains Unknown");
        if(bad==9)need(error==cause,"original native failure cause retained");
    }
    need(complete(before,0,false,false,false,false,false,0,30001,{})==contract::Completion::Unknown&&error.find("no_replay")!=std::string::npos,"read-only deadline preserves no-replay Unknown");
    actual_command::checks();std::cout<<object_checks<<" object UI checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<"failure after "<<object_checks<<": "<<error.what()<<'\n';return 1;}
