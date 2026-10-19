#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
unsigned before_calls=0,after_calls=0,original_calls=0,failed_calls=0;int mode=0;bool last_result=false;
std::string cause;const void* expected_property=reinterpret_cast<void*>(1);
void* expected_manager=reinterpret_cast<void*>(2);void* expected_stream=reinterpret_cast<void*>(3);void* expected_context=reinterpret_cast<void*>(4);
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void receivers(const void* property,void* manager,void* stream,void* context){require(property==expected_property&&manager==expected_manager&&stream==expected_stream&&context==expected_context,"original receiver changed");}
namespace game::district_saved_identity_runtime {
using PropertyCall=bool(*)(const void*,void*,void*,void*);
uint64_t save_before(const void* p,void* m,void* s,void* c){receivers(p,m,s,c);++before_calls;return mode==3?0:71;}
uint64_t load_before(const void* p,void* m,void* s,void* c){return save_before(p,m,s,c);}
void save_after(uint64_t ticket,const void* p,void* m,void* s,void* c,bool result){receivers(p,m,s,c);require(ticket==(mode==3?0U:71U),"observer ticket changed");++after_calls;last_result=result;}
void load_after(uint64_t ticket,const void* p,void* m,void* s,void* c,bool result){save_after(ticket,p,m,s,c,result);}
void original_failed(uint64_t ticket,const std::string& error){require(ticket==71,"failure ticket changed");++failed_calls;cause=error;}
}
#include "district_property_wrappers_actual.inc"
bool original(const void* p,void* m,void* s,void* c){
    receivers(p,m,s,c);++original_calls;
    if(mode==1)throw std::runtime_error("actual original reason");
    if(mode==2)throw 19;
    return mode!=4;
}
int main()try{
    unsigned checks=0;original_save_district_property=original;original_load_district_property=original;
    for(auto wrapper:{save_district_property,load_district_property})for(mode=0;mode<5;++mode){
        before_calls=after_calls=original_calls=failed_calls=0;cause.clear();bool thrown=false;
        try{const bool result=wrapper(expected_property,expected_manager,expected_stream,expected_context);require(result==(mode!=4)&&result==last_result,"original result changed");}
        catch(const std::runtime_error& e){require(mode==1&&std::string(e.what())=="actual original reason","original exception replaced");thrown=true;}
        catch(int value){require(mode==2&&value==19,"nonstandard exception replaced");thrown=true;}
        require(before_calls==1&&original_calls==1,"original not exactly once");
        require(after_calls==(thrown?0U:1U)&&failed_calls==(thrown?1U:0U),"failed original reported as completed");
        if(thrown)require(!cause.empty(),"original cause not retained");++checks;
    }
    std::cout<<checks<<" actual District original Save/Load wrapper cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
