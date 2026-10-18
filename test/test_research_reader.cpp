#include "reader_fixture.h"
#include "tutorial_replication/admission_read.h"
#include "tutorial_replication/admission_code.h"
#include "initial_release/startingpoint_read.h"
namespace admission=mt2mp::game::startingpoint_adoption;
namespace release_read=mt2mp::game::initial_release;
namespace {
unsigned checks=0;
void need(bool ok,const std::string& cause){++checks;if(!ok)throw std::runtime_error(cause);}
constexpr uintptr_t foreign_tech=0x260000,foreign_project=0x261000,catalogue=0x262000,developers=0x263000;
void prepare(Fake& f){
    f.str(tech+0x20,"bridges");f.str(record+0x38,"bridges");f.str(project+0x20,"bridges");
    f.str(tech+0x1f0,"@Build Bridge");f.str(type+0x48,"@Build Bridge");
    for(const auto& guard:admission::code_guards)for(size_t i=0;i<guard.bytes.size();++i)f.memory[base+guard.rva+i]=guard.bytes[i];
    f.put<uint8_t>(tech+0xf9,0);f.put<int32_t>(tech+0x160,0);f.put<uint64_t>(tech+0xe0,0);f.put<int32_t>(tech+0x210,10);f.put<uint8_t>(base+0x13b9820,1);
    constexpr uintptr_t node=0x230000,bucket=0x1a2dd00+0x38*0x38;
    f.put<uintptr_t>(base+bucket+0x10,node);f.put<uint64_t>(base+bucket+0x18,1);f.put<uintptr_t>(node,0);f.put<uintptr_t>(node+8,release+0x18);
    f.vt(release+0x18,0x16c1370);f.put<uintptr_t>(base+0x16c1370,base+0x4e4cf0);
}
void creation(Fake& f){
    f.flags(0,0,0);f.put<uintptr_t>(lounge+0xa8,0);f.str(project+0x40,"");f.put<int32_t>(project+0x60,0);
    f.vt(foreign_project,0x16e48c0);f.put<int32_t>(foreign_project+0x18,0);f.str(foreign_project+0x20,"startingpoint");f.str(foreign_project+0x40,"");f.put<int32_t>(foreign_project+0x60,0);
    f.array(lounge+0xd8,catalogue,{foreign_project,project});f.array(lounge+0x70,developers,{});
    f.put<uint8_t>(lounge+0x57,1);f.put<int32_t>(lounge+0x310,1);f.put<int32_t>(tech+0xb4,3);
    f.put<uintptr_t>(change2+0x70,foreign_tech);f.array(release+0x150,change_array,{change2});
}
StartingpointSnapshot read(Fake& f,bool create=false){
    const auto before=f.memory;StartingpointSnapshot out;json diagnostic;std::string e;
    const bool ok=detail::observe(f,diagnostic,e,&out,create,false,"bridges");
    need(ok,"keyed actual Reader rejected: "+e);need(f.memory==before&&e.empty(),"read modified memory or retained error");
    need(diagnostic["startingpoint"]["key"]=="bridges"&&diagnostic["startingpoint"]["item_type_name"]=="@Build Bridge","selected key/item identity lost");return out;
}
void rejects(Fake& f,const std::string& part,bool create=false,const std::string& key="bridges"){
    json result={{"stale",true}};StartingpointSnapshot out;std::string e;
    const bool ok=detail::observe(f,result,e,&out,create,false,key);need(!ok&&result.is_null()&&e.find(part)!=std::string::npos,"wrong keyed Reader rejection: "+e+" expected "+part);
}
struct Memory:release_read::Memory {Fake& f;explicit Memory(Fake& source):f(source){}bool copy(uintptr_t p,void* out,size_t n,std::string& e)override{return f.copy(p,out,n,e);}};
void keyed_reader(){
    {Fake f;prepare(f);auto out=read(f);need(out.row&&out.row->project_present&&out.row->bugs_total==3&&out.row->bugs_known==1&&out.admitted&&out.tech==tech&&out.record==record,"Bridges current project lost exact bugs or admission");}
    {Fake f;prepare(f);creation(f);auto out=read(f,true);need(out.creation&&out.creation->source==project&&out.creation->catalogue.size()==2&&out.creation->catalogue.front().key=="startingpoint"&&out.creation->max_workers==3,"keyed creation chose Startingpoint or lost catalogue");need(out.creation->release_changes==std::vector<uintptr_t>{change2}&&out.row&&out.row->release_change_count==1&&out.row->release_feature_count==0&&out.row->release_counter==7,"existing foreign Release membership lost");}
    {Fake f;prepare(f);creation(f);f.array(release+0x150,change_array,{change2,change});auto out=read(f,true);need(out.creation->release_changes==std::vector<uintptr_t>({change2,change})&&out.row->release_change_count==2&&out.row->release_feature_count==1,"mixed retained Release membership was filtered");}
    {Fake f;prepare(f);creation(f);f.array(release+0x150,change_array,{change,change2});auto out=read(f,true);need(out.creation->release_changes==std::vector<uintptr_t>({change,change2})&&out.row->release_feature_count==1,"sorted native member order lost");}
    {Fake f;prepare(f);f.flags(1,1,0);f.put<uintptr_t>(slots,item);f.str(project+0x20,"inn");f.array(release+0x150,change_array,{change});auto out=read(f);need(out.row&&out.row->deployed&&!out.row->project_present&&out.row->bugs_total==0&&out.row->work==0&&out.row->inventory_count==1&&out.row->release_feature_count==1,"later unrelated project polluted retired Bridges");}
    {Fake f;prepare(f);f.str(tech+0x1f0,"@StartingPoints");rejects(f,"native itemTypeName differs");}
    {Fake f;prepare(f);f.str(type+0x48,"@StartingPoints");rejects(f,"ItemType catalogue member absent");}
    {Fake f;prepare(f);f.str(record+0x38,"startingpoint");rejects(f,"not owned by current SoftwareStatus");}
    {Fake f;prepare(f);f.put<uintptr_t>(record+0x60,foreign_tech);rejects(f,"reciprocal link differs");}
    {Fake f;prepare(f);creation(f);f.str(foreign_project+0x20,"bridges");rejects(f,"catalogue key duplicated",true);}
    {Fake f;prepare(f);creation(f);f.array(lounge+0xd8,catalogue,{foreign_project});rejects(f,"catalogue project absent",true);}
    {Fake f;prepare(f);rejects(f,"unsupported selected research key",false,"friendslist");need(f.current_calls==0&&f.reads.empty(),"unsupported key read native state");}
    {Fake f;prepare(f);rejects(f,"TechTree member absent",false,"startingpoint");}
    {Fake f;prepare(f);f.unstable=tech+0x1f0;rejects(f,"changed during read");}
    {Fake f;prepare(f);creation(f);f.unstable=change2+0x70;rejects(f,"changed during read",true);}
    {Fake f;prepare(f);f.unreadable=base+0x1a2dd00+0x38*0x38+0x10;auto out=read(f);need(!out.admitted&&out.admission_cause=="fixture memory original cause","admission read failure lost original cause");}
}
void direct_release(){
    std::string e;
    {Fake f;prepare(f);Memory m(f);release_read::StartingpointRelease out;f.put<uintptr_t>(change2+0x70,foreign_tech);f.array(release+0x150,change_array,{change2,change});need(release_read::read_research_release(m,base,state,tech,"bridges",out,e)&&out.change_count==2&&out.feature_count==1&&out.mutation_counter==7,"keyed mixed Release read: "+e);need(!release_read::read_startingpoint_release(m,base,state,tech,out,e)&&e.find("supplied Tech key differs")!=std::string::npos,"old Startingpoint Release wrapper widened");}
    {Fake f;Memory m(f);release_read::StartingpointRelease out;need(release_read::read_startingpoint_release(m,base,state,tech,out,e),"old Startingpoint Release wrapper regressed: "+e);need(!release_read::read_research_release(m,base,state,tech,"bridges",out,e),"Bridges release accepted Startingpoint Tech");}
    {Fake f;prepare(f);Memory m(f);release_read::StartingpointRelease out;need(!release_read::read_research_release(m,base,state,tech,"inn",out,e)&&f.reads.empty(),"unproven release key read memory");}
    {Fake f;prepare(f);Memory m(f);release_read::StartingpointRelease out;f.array(release+0x150,change_array,{change,change});need(!release_read::read_research_release(m,base,state,tech,"bridges",out,e)&&e.find("duplicate")!=std::string::npos,"duplicate retained Release member accepted");}
    {Fake f;prepare(f);Memory m(f);release_read::StartingpointRelease out;f.array(release+0x150,change_array,{change2});f.str(change2+0x18,"tech_purchase");need(release_read::read_research_release(m,base,state,tech,"bridges",out,e)&&out.change_count==1&&!out.feature_count,"unrelated native feature was counted or discarded");}
}
void direct_admission(){
    std::string e;
    {Fake f;prepare(f);Memory m(f);admission::Admission out;need(admission::read_research_admission(m,base,state,tech,"bridges",out,e)&&out.release==release&&out.listeners.size()==1,"Bridges original admission failed: "+e);need(!admission::read_admission(m,base,state,tech,out,e)&&e.find("key length differs")!=std::string::npos,"old Startingpoint admission wrapper widened");}
    {Fake f;prepare(f);f.str(tech+0x20,"startingpoint");Memory m(f);admission::Admission out;need(admission::read_admission(m,base,state,tech,out,e),"Startingpoint original admission regressed: "+e);}
    {Fake f;prepare(f);Memory m(f);admission::Admission out;need(!admission::read_research_admission(m,base,state,tech,"friendslist",out,e)&&f.reads.empty(),"unsupported admission key read memory");}
    {Fake f;prepare(f);f.memory[tech+0x20+0x1000000+7]=1;Memory m(f);admission::Admission out;need(!admission::read_research_admission(m,base,state,tech,"bridges",out,e)&&e.find("Tech key differs")!=std::string::npos,"Bridges missing terminator accepted");}
    for(int field=0;field<4;++field){Fake f;prepare(f);if(field==0)f.put<uint8_t>(tech+0xf9,1);if(field==1)f.put<int32_t>(tech+0x160,1);if(field==2)f.put<uint64_t>(tech+0xe0,1);if(field==3)f.put<int32_t>(tech+0x210,9);Memory m(f);admission::Admission out;need(!admission::read_research_admission(m,base,state,tech,"bridges",out,e)&&e.find("effects outside slice")!=std::string::npos,"Bridges introduced unsupported deployment side effects");}
    {Fake f;prepare(f);f.put<uintptr_t>(base+0x16c1370,base+0x4e4cf1);Memory m(f);admission::Admission out;need(!admission::read_research_admission(m,base,state,tech,"bridges",out,e)&&e.find("current Release subobject/thunk")!=std::string::npos,"foreign event handler admitted");}
}
}
int main(){try{keyed_reader();direct_release();direct_admission();std::cout<<"PASS "<<checks<<" keyed Research reader/admission/Release assertions\n";return 0;}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
