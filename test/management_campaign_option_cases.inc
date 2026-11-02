namespace campaign_option_fixture {
struct Runtime {uintptr_t image=1;size_t captures=0;bool pending=false;};
struct CampaignOption {uintptr_t data=0,value=0;int32_t count=0,capacity=0,index=-1;std::string text;};
struct CampaignUiState {uintptr_t choice=0;int32_t type=-1;bool ready=false;};
uintptr_t values=0x10000;int32_t count=3,capacity=3;std::vector<int32_t> types{0,1,2};
bool fail(std::string& e,const std::string& value){e=value;return false;}
template<class T>bool at(Runtime&,uintptr_t,size_t offset,T& out,std::string& e){
    if(offset==0x348){out=static_cast<T>(values);return true;}
    if(offset==0x350){out=static_cast<T>(count);return true;}
    if(offset==0x354){out=static_cast<T>(capacity);return true;}
    return fail(e,"unexpected offset");
}
bool text(Runtime&,uintptr_t value,std::string& out,size_t,std::string& e){if(value<values||(value-values)/32>=types.size())return fail(e,"invalid value");out="native option "+std::to_string((value-values)/32);return true;}
uint32_t campaign_value_type(uintptr_t,uintptr_t value,int32_t& type){type=types.at((value-values)/32);return 0;}
#include "management_campaign_option_actual.inc"
int calls=0;int32_t selected_id=1;bool selected_ready=true;uint32_t original_fault=0;
uint32_t campaign_select_original(uintptr_t,uintptr_t,uintptr_t){++calls;return original_fault;}
uint32_t campaign_selection_id(uintptr_t,uintptr_t,int32_t& id){id=selected_id;return 0;}
bool campaign_ui_find(Runtime&,CampaignUiState& ui,std::string&){ui={0x20000,1,selected_ready};return true;}
#include "management_campaign_select_actual.inc"
void run(){Runtime r;CampaignOption out;std::string e;
    need(campaign_option(r,0x20000,1,out,e)&&out.index==1&&out.value==values+32&&out.text=="native option 1","actual native Campaign option selection");
    e.clear();types[2]=1;need(!campaign_option(r,0x20000,1,out,e)&&!e.empty(),"duplicate native Campaign option accepted");types[2]=2;
    e.clear();need(!campaign_option(r,0x20000,7,out,e)&&!e.empty(),"absent native Campaign option accepted");
    e.clear();count=65;need(!campaign_option(r,0x20000,1,out,e)&&!e.empty(),"unbounded Campaign option array accepted");count=3;
    e.clear();capacity=2;need(!campaign_option(r,0x20000,1,out,e)&&!e.empty(),"invalid Campaign option capacity accepted");capacity=3;
    e.clear();need(campaign_option(r,0x20000,1,out,e),"fresh Campaign option absent");CampaignUiState ui{0x20000,9,false};uint32_t fault=0;
    calls=0;need(campaign_select_bound(r,ui,out,1,0,fault,e)&&calls==1&&!fault,"original selection was not called once and confirmed");
    calls=0;selected_id=2;need(!campaign_select_bound(r,ui,out,1,0,fault,e)&&calls==1,"wrong native selected index accepted");selected_id=1;
    calls=0;original_fault=7;need(!campaign_select_bound(r,ui,out,1,0,fault,e)&&calls==1&&fault==7,"original selection failure was lost or replayed");original_fault=0;
    calls=0;r.pending=true;need(!campaign_select_bound(r,ui,out,1,0,fault,e)&&calls==1,"selection with unexpected pending action accepted");r.pending=false;
}
}
