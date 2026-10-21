#include "character_import_view.h"
#include <iostream>
#include <vector>
namespace character_import_view_fixture {
namespace ui=mt2mp::game::character_import_view;
struct Calls {
    ui::State value{1,2,3,4,5};std::vector<std::string> order;
    unsigned reads=0,writes=0,original_calls=0;int scenario=0;bool original_returned=false;
    bool read(ui::State& out,std::string& error){++reads;order.push_back("read");
        if(scenario==1&&reads==1){error="exact UI owner read failed";return false;}
        out=value;return true;}
    bool write(const ui::State& target,std::string& error){++writes;order.push_back("write");
        if(scenario==2&&writes==1){error="exact scoped UI prepare failed";return false;}
        if(scenario==3&&writes==2){error="exact scoped UI restore failed";return false;}
        value=target;return true;}
    bool attempted()const{return original_calls!=0;}
    bool returned()const{return original_returned;}
    bool invoke(bool preserve,std::string& error){++original_calls;order.push_back("original");
        if(value.selected!=9|| (preserve&&value.notice)){error="fixed original input differs";return false;}
        if(scenario==4){error="original image import failed";return false;}
        original_returned=true;if(scenario==5)value.prefabs=99;return true;}
};
}
int main()try{using namespace character_import_view_fixture;unsigned checks=0;std::string error;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(std::string(why)+" cause="+error);};
    for(int scenario=0;scenario<6;++scenario){Calls calls;calls.scenario=scenario;const auto before=calls.value;ui::Receipt receipt;error.clear();
        const bool ok=ui::run(calls,before,9,true,receipt,error);
        if(scenario==0){check(ok&&receipt.confirmed&&receipt.attempted&&receipt.returned&&receipt.restored,"Remote original transaction not confirmed");
            check(calls.value==before&&calls.writes==2&&calls.original_calls==1,"Remote unsaved local selection/base/notice not restored");}
        if(scenario==1)check(!ok&&!receipt.attempted&&calls.original_calls==0&&error=="exact UI owner read failed","Owner failure lost cause or entered original");
        if(scenario==2)check(!ok&&!receipt.attempted&&calls.original_calls==0&&error=="exact scoped UI prepare failed","Prepare failure entered original");
        if(scenario==3)check(!ok&&receipt.attempted&&receipt.returned&&!receipt.confirmed&&error=="exact scoped UI restore failed","Restore failure reported rendered success");
        if(scenario==4)check(!ok&&receipt.attempted&&!receipt.returned&&receipt.restored&&calls.value==before&&error=="original image import failed","Original failure lost local restore or original cause");
        if(scenario==5)check(!ok&&!receipt.restored&&!receipt.confirmed&&calls.writes==1&&error.find("owner changed")!=std::string::npos,"Changed local owner was overwritten during restore");
        check(calls.original_calls<=1,"Original import replayed during cleanup");
    }
    Calls initiating;initiating.value.selected=9;const auto saved=initiating.value;ui::Receipt receipt;error.clear();
    check(ui::run(initiating,saved,9,false,receipt,error)&&receipt.confirmed&&initiating.writes==0&&initiating.original_calls==1,"Initiator original presentation changed");
    Calls changed;auto wrong=changed.value;wrong.base=100;
    check(!ui::run(changed,wrong,9,true,receipt,error)&&!changed.writes&&!changed.original_calls,"Stale base/view witness entered original");
    check(!ui::run(changed,changed.value,9,false,receipt,error)&&!changed.original_calls,"Initiating selection changed before execution");
    std::cout<<checks<<" actual Character import local presentation/restore/no-replay checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
