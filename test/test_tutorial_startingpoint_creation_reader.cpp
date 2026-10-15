#include "reader_fixture.h"
#include "tutorial_replication/startingpoint.h"
namespace sp=mt2mp::game::tutorial_replication::startingpoint;
namespace {
constexpr uintptr_t catalogue_array=0x240000,developers_array=0x241000,developer=0x250000,other_source=0x260000;
unsigned checks=0;
void need(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
void pass(const char* name){++checks;std::cout<<"PASS "<<name<<'\n';}
void baseline(Fake& f){f.flags(0,0,0);f.put<uintptr_t>(lounge+0xa8,0);}
void owners(Fake& f){
    baseline(f);f.array(lounge+0xd8,catalogue_array,{project});f.array(lounge+0x70,developers_array,{developer});
    f.str(project+0x40,"startingpoint_art");f.put<int32_t>(project+0x60,2);
    f.put<uint8_t>(lounge+0x57,1);f.put<int32_t>(lounge+0x310,5);f.put<int32_t>(tech+0xb4,8);
    f.vt(developer,0x17422e0);f.put<int32_t>(developer+0x4c4,6);
}
StartingpointSnapshot read(Fake& f,bool creation){json diagnostic;StartingpointSnapshot out;std::string error;const auto memory=f.memory;need(detail::observe(f,diagnostic,error,&out,creation),"actual creation Reader rejected: "+error);need(error.empty()&&f.memory==memory,"Reader wrote physical fixture or lost success cause");return out;}
void rejects(Fake& f,const std::string& cause){json diagnostic;StartingpointSnapshot out;std::string error;const bool ok=detail::observe(f,diagnostic,error,&out,true);need(!ok&&diagnostic.is_null()&&error.find(cause)!=std::string::npos,"wrong actual Reader failure: "+error);}
}
int main(){try{
    {Fake f;owners(f);auto out=read(f,true);std::string error;need(out.row&&!out.row->purchased&&!out.row->project_present&&!out.row->deployed&&sp::valid(*out.row,error)&&out.creation&&out.creation->source==project&&out.creation->catalogue.size()==1&&out.creation->developers[0].job==6&&out.creation->release_owner==release&&out.creation->release_changes.empty(),"explicit baseline or actual owned catalogue/Developer/Release absent");pass("actual creation Reader returns explicit baseline and local owners without mutation");}
    {Fake f;baseline(f);auto out=read(f,false);need(out.row&&!out.creation,"ordinary Reader unexpectedly required creation catalogue");pass("ordinary deployment Reader keeps creation-only memory optional");}
    {Fake f;owners(f);f.put<uintptr_t>(tech+0x1e8,0);f.array(software+0x280,record_array,{});rejects(f,"native TechRecord absent");pass("missing owned TechRecord fails creation read with its specific cause");}
    {Fake f;owners(f);f.array(lounge+0xd8,catalogue_array,{});rejects(f,"catalogue project absent");pass("absent native catalogue project cannot become a creation source");}
    {Fake f;owners(f);f.vt(other_source,0x16e48c0);f.put<int32_t>(other_source+0x18,0);f.str(other_source+0x20,"startingpoint");f.str(other_source+0x40,"other");f.put<int32_t>(other_source+0x60,0);f.array(lounge+0xd8,catalogue_array,{project,other_source});rejects(f,"catalogue key duplicated");pass("duplicate Startingpoint catalogue identity is rejected");}
    {Fake f;owners(f);f.put<uintptr_t>(lounge+0xa8,project);rejects(f,"current project aliases owned catalogue member");pass("live current project cannot alias the immutable catalogue source");}
    {Fake f;owners(f);f.vt(developer,0x16e48c0);rejects(f,"vtable differs: Dev");pass("Developer membership requires actual native class");}
    {Fake f;owners(f);f.put<uint8_t>(lounge+0x57,2);rejects(f,"native automatic worker policy invalid");pass("invalid automatic worker flag cannot enter original SetCurrent");}
    {Fake f;owners(f);f.put<uint8_t>(lounge+0x57,0);f.put<int32_t>(lounge+0x310,-1);f.put<int32_t>(tech+0xb4,-1);auto out=read(f,true);need(out.creation&&out.creation->total_workers==-1&&out.creation->max_workers==-1,"disabled original worker policy was treated as active");pass("disabled worker policy retains raw values without applying unused bounds");}
    {Fake f;owners(f);f.put<int32_t>(lounge+0x310,0);auto out=read(f,true);need(out.creation&&out.creation->automatic_workers==1&&out.creation->total_workers==0,"original automatic worker zero boundary rejected");pass("enabled original worker policy preserves zero-worker boundary");}
    {Fake f;owners(f);f.unreadable=lounge+0xd8;rejects(f,"fixture memory original cause");pass("creation catalogue read failure preserves original cause");}
    {Fake f;owners(f);f.unstable=project+0x60;rejects(f,"changed during read");pass("new catalogue source bytes remain inside actual stable-read transaction");}
    std::cout<<checks<<" actual creation Reader scenarios passed\n";return 0;
}catch(const std::exception& cause){std::cerr<<"error: "<<cause.what()<<'\n';return 1;}}
