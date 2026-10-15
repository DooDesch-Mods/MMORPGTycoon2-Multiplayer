#include "reader_fixture.h"
#include "tutorial_replication/admission_code.h"
namespace sp=mt2mp::game::tutorial_replication::startingpoint;
namespace {
unsigned checks=0;
void need(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
void pass(const char* name){++checks;std::cout<<"PASS "<<name<<'\n';}
void admission(Fake& f){
    for(const auto& code:mt2mp::game::startingpoint_adoption::code_guards)
        for(size_t i=0;i<code.bytes.size();++i)f.memory[base+code.rva+i]=code.bytes[i];
    f.put<uint8_t>(tech+0xf9,0);f.put<int32_t>(tech+0x160,0);f.put<uint64_t>(tech+0xe0,0);f.put<int32_t>(tech+0x210,10);
    f.put<uint8_t>(base+0x13b9820,1);constexpr uintptr_t node=0x230000,bucket=0x1a2dd00+0x38*0x38;
    f.put<uintptr_t>(base+bucket+0x10,node);f.put<uint64_t>(base+bucket+0x18,1);
    f.put<uintptr_t>(node,0);f.put<uintptr_t>(node+8,release+0x18);f.vt(release+0x18,0x16c1370);f.put<uintptr_t>(base+0x16c1370,base+0x4e4cf0);
    f.put<int32_t>(lounge+0xb8,0);f.put<int32_t>(lounge+0xbc,0);
}
StartingpointSnapshot read(Fake& f,json& diagnostic){
    const auto memory=f.memory;StartingpointSnapshot snapshot;std::string error="stale";
    need(detail::observe(f,diagnostic,error,&snapshot),"actual Reader rejected: "+error);
    need(error.empty()&&f.memory==memory,"Reader success retained cause or wrote memory");return snapshot;
}
}
int main(){try{
    {Fake f;admission(f);json diagnostic;auto snapshot=read(f,diagnostic);need(snapshot.row&&snapshot.row->project_present&&!snapshot.row->deployed&&snapshot.project==project&&snapshot.record==record&&snapshot.inventory==inventory&&snapshot.slots==slots&&snapshot.admitted,"fixed owned project snapshot differs");pass("actual Reader returns fixed project and local owners with accepted admission");}
    {Fake f;admission(f);f.flags(1,1,0);f.put<uintptr_t>(slots,item);f.array(release+0x150,change_array,{change});json diagnostic;auto snapshot=read(f,diagnostic);std::string error;need(snapshot.row&&snapshot.row->project_present&&snapshot.row->deployed&&snapshot.project==project&&!sp::valid(*snapshot.row,error)&&snapshot.admitted,"private Deploy intermediate was wire-validated or lost");need(snapshot.row->inventory_count==1&&snapshot.row->release_change_count==1&&snapshot.row->release_feature_count==1,"actual intermediate effects lost");pass("private Deploy readback retains existing project plus deployed record before destruction");}
    {Fake f;admission(f);f.flags(1,1,0);f.put<uintptr_t>(slots,item);f.array(release+0x150,change_array,{change});f.put<uintptr_t>(lounge+0xa8,0);f.put<float>(lounge+0xb0,0);for(size_t off:{size_t(0xb4),size_t(0xb8),size_t(0xbc),size_t(0xc4)})f.put<int32_t>(lounge+off,0);json diagnostic;auto snapshot=read(f,diagnostic);std::string error;need(snapshot.row&&!snapshot.row->project_present&&snapshot.row->deployed&&!snapshot.project&&sp::valid(*snapshot.row,error)&&snapshot.admitted,"retired full Row lost");pass("actual Reader retains retired project, native item and Release effects");}
    {Fake f;admission(f);f.flags(1,1,0);f.put<uintptr_t>(slots,item);f.array(release+0x150,change_array,{change});f.str(project+0x20,"inn");f.put<float>(lounge+0xb0,37.5f);f.put<int32_t>(lounge+0xb8,4);f.put<int32_t>(lounge+0xbc,2);json diagnostic;auto snapshot=read(f,diagnostic);std::string error;need(snapshot.row&&!snapshot.row->project_present&&snapshot.row->deployed&&snapshot.row->work==0&&snapshot.row->bugs_total==0&&sp::valid(*snapshot.row,error),"later project polluted fixed deployed Startingpoint Row");need(diagnostic["project"]["tech_key"]=="inn"&&diagnostic["project"]["work"]==37.5f&&diagnostic["project"]["bugs_total"]==4,"ordinary other-project diagnostic lost real state");pass("later different project preserves deployed Startingpoint Row and real project diagnostic");}
    {Fake f;admission(f);f.unreadable=base+0x1a2dd00+0x38*0x38+0x10;json diagnostic;auto snapshot=read(f,diagnostic);need(snapshot.row&&!snapshot.admitted&&snapshot.admission_cause=="fixture memory original cause"&&diagnostic["startingpoint_adoption"]["accepted"]==false,"admission read failure destroyed successful Development read");pass("admission-only read failure stays separate and preserves original cause");}
    {Fake f;admission(f);f.put<uint8_t>(base+0x13b9820,0);json diagnostic;auto snapshot=read(f,diagnostic);need(snapshot.row&&!snapshot.admitted&&snapshot.admission_cause=="startingpoint admission game events disabled/invalid","disabled events did not remain visible");pass("event-disabled admission does not fabricate native authorization");}
    {Fake f;admission(f);json diagnostic;auto snapshot=read(f,diagnostic);const auto& listener=diagnostic["startingpoint_adoption"]["listeners"][0];need(listener==json{{"class_rva",0x16c1370},{"handler_rva",0x4e4cf0}},"diagnostic exposed raw listener pointers");need(!diagnostic.contains("project_pointer")&&!diagnostic["startingpoint_adoption"].contains("release"),"raw owners leaked into public diagnostic");need(snapshot.release==release,"private release owner unavailable to native adoption");pass("public diagnostic has relative code identities while native owners stay private");}
    {Fake f;admission(f);f.unstable=release+0x1c4;json diagnostic;StartingpointSnapshot snapshot;std::string error;need(!detail::observe(f,diagnostic,error,&snapshot)&&diagnostic.is_null()&&error.find("changed during read")!=std::string::npos,"snapshot optional path skipped final double-read");pass("optional snapshot path remains under final stable-read guard");}
    std::cout<<checks<<" actual optional snapshot Reader scenarios passed\n";return 0;
}catch(const std::exception& cause){std::cerr<<"error: "<<cause.what()<<'\n';return 1;}}
