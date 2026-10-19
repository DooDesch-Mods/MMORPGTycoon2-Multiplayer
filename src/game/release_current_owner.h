#pragma once
#include <cstdint>
#include <string>
namespace mt2mp::game::release_current_owner {
constexpr uintptr_t got_rva=0x164b030,current_rva=0x1a38700,manager_ap=0x1714ed0,release_ap=0x16c12b0;
struct Receipt {bool attempted=false,stored=false,confirmed=false;};
template<class Read>bool read(Read& memory,uintptr_t image,uintptr_t manager,uintptr_t required,std::string& e){
    e.clear();
    uintptr_t got=0,current=0,owned=0,ap=0;
    if(!required||!memory(image,got_rva,got,e)||got!=image+current_rva||!memory(got,0,current,e)||current!=required||
       !memory(manager,0,ap,e)||ap!=image+manager_ap||!memory(manager,0x38,owned,e)||owned!=required||!memory(required,0,ap,e)||ap!=image+release_ap){if(e.empty())e="Release static s_current/GOT/Manager slot/current class reciprocity differs";return false;}
    e.clear();return true;
}
template<class Read,class Exchange>bool adopt(Read& memory,Exchange& exchange,uintptr_t image,uintptr_t manager,uintptr_t old_release,uintptr_t new_release,Receipt& receipt,std::string& e){
    receipt={};e.clear();uintptr_t got=0,current=0,owned=0,ap=0;
    if(!old_release||!new_release||old_release==new_release||!memory(image,got_rva,got,e)||got!=image+current_rva||
       !memory(got,0,current,e)||current!=old_release||!memory(manager,0,ap,e)||ap!=image+manager_ap||!memory(manager,0x38,owned,e)||owned!=new_release||
       !memory(old_release,0,ap,e)||ap!=image+release_ap||!memory(new_release,0,ap,e)||ap!=image+release_ap){if(e.empty())e="Release fresh Load requires exact old static/new Manager reciprocal owners before CAS";return false;}
    receipt.attempted=true;if(!exchange(got,old_release,new_release,receipt.stored,e)){if(e.empty())e="Release static current pointer CAS failed";return false;}
    if(!receipt.stored){e="Release static current pointer changed concurrently before CAS";return false;}
    if(!read(memory,image,manager,new_release,e))return false;receipt.confirmed=true;e.clear();return true;
}
}
