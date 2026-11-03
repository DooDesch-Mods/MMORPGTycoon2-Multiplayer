#define main fixture_control_regression
#include "test_character_offered_controls.cpp"
#undef main
namespace a {struct CharacterRenameIntent {int32_t kind=-1,id=-1;};}
struct UiJob {json arguments={{"kind",0},{"id",1}},result=json::object();int binding=0;struct{std::string snapshot_uuid="fixture";}context;bool attempted=false,waiting=false;unsigned waits=0;};
struct Sample {int value=0;};
unsigned fixture_page_calls=0;int fixture_page_world=0;bool fixture_page_effect=false;
bool sample(Memory&,int,const std::string&,const a::CharacterRenameIntent&,Sample& out,std::string&){out.value=fixture_page_world;return true;}
bool same_local(const Sample&,const Sample&){return true;}
uint32_t edit_set_page(uintptr_t tab,const char* text,size_t size){
    ++fixture_page_calls;if(tab!=0x5000||std::string_view(text,size)!="Costume")return 123;
    put(0x6000,0x418,0xa000);if(fixture_page_effect)++fixture_page_world;return 0;
}
#include "character_offered_page_actual.inc"
void page_setup(){setup();fixture_page_calls=0;fixture_page_world=0;fixture_page_effect=false;
    pane(0xa000,0x1112,0x6000,0x1000);fixture_text[{0xa000,0x1b0}]="Costume";pane(0x6000,0x16dfe90,0x5000,0x1000);}
int main()try{
    if(fixture_control_regression())return 1;
    UiJob job;std::string error;bool current=false;unsigned checks=0;
    const auto check=[&](bool value,const char* why){++checks;if(!value)throw std::runtime_error(std::string(why)+" cause="+error);error.clear();};
    page_setup();check(edit_page(job,0x2000,0x1000,"multi","Costume",true,current,error)&&!current&&job.waiting&&job.attempted&&fixture_page_calls==1,"original enabled page request");
    check(edit_page(job,0x2000,0x1000,"multi","Costume",false,current,error)&&!current&&fixture_page_calls==1,"pending Layout replayed SetPage");
    put(0x6000,0x410,0xa000);check(edit_page(job,0x2000,0x1000,"multi","Costume",false,current,error)&&current&&fixture_page_calls==1,"actual Layout did not attach offered page");
    page_setup();job={};put(0x8000,1,0);
    check(!edit_page(job,0x2000,0x1000,"multi","Costume",true,current,error)&&!job.attempted&&!fixture_page_calls,"locked page reached native SetPage");
    page_setup();job={};put(0x6000,0x410,0xa000);put(0x6000,0x418,0xa000);put(0xa000,0x278,0xa000);
    check(!edit_page(job,0x2000,0x1000,"multi","Costume",true,current,error)&&!job.attempted&&!fixture_page_calls&&!error.empty(),"invalid current ancestry fell through into native page mutation");
    page_setup();job={};put(0x6000,0x410,0xa000);put(0x6000,0x418,0xa000);fixture_values.erase({0xa000,0x69});
    check(!edit_page(job,0x2000,0x1000,"multi","Costume",true,current,error)&&!job.attempted&&!fixture_page_calls&&error=="fixture field absent","unreadable offered ancestry lost cause or invoked SetPage");
    page_setup();job={};fixture_page_effect=true;
    check(!edit_page(job,0x2000,0x1000,"multi","Costume",true,current,error)&&job.attempted&&fixture_page_calls==1,"original page gameplay drift was hidden");
    page_setup();job={};put(0x6000,0x418,0xa000);job.waits=8;
    check(!edit_page(job,0x2000,0x1000,"multi","Costume",false,current,error)&&!fixture_page_calls&&error.find("8 native UI updates")!=std::string::npos,"native Layout timeout was unbounded or replayed");
    std::cout<<checks<<" actual original-page request/ancestry/wait assertions passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
