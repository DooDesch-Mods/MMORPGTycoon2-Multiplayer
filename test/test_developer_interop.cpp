#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
namespace fixture {bool throw_hash=false;}
namespace mt2mp {std::string sha256(const std::vector<unsigned char>&);}
namespace presentation {
#include "developer_presentation.inc"
}
namespace mt2mp {std::string sha256(const std::vector<unsigned char>& b){
    if(fixture::throw_hash)throw std::runtime_error("original hash backend error");
    if(!b.empty()&&b[0])return "changed body";
    for(const auto& body:presentation::ui_bodies)if(b.size()==body.size)return body.sha;
    throw std::runtime_error("unexpected test hash size");
}}
namespace {
unsigned checks=0,owned_calls=0,copies=0;bool allow=true;uintptr_t owner_target=0,unreadable=0,changed=0;
constexpr uintptr_t image=0x140000000;
void need(bool b,const char* e){++checks;if(!b)throw std::runtime_error(e);}
bool owned(uintptr_t target,std::string& e){++owned_calls;owner_target=target;if(!allow)e="actual owned hook relay differs";return allow;}
bool copy(uintptr_t target,void* out,size_t n,std::string& e){++copies;if(target==unreadable){e="original memory read error";return false;}std::fill_n(static_cast<unsigned char*>(out),n,static_cast<unsigned char>(0));if(target==changed)*static_cast<unsigned char*>(out)=1;return true;}
void reset(){owned_calls=copies=0;owner_target=unreadable=changed=0;allow=true;fixture::throw_hash=false;}
}
int main(){try{std::string e;
    reset();need(presentation::verify_ui(image,copy,e,owned)&&owned_calls==1&&owner_target==image+0x6c7b70&&copies==4,"shared UI hook was not routed to its exact owner while checking the other four complete bodies");
    reset();allow=false;need(!presentation::verify_ui(image,copy,e,owned)&&e=="actual owned hook relay differs"&&!copies,"owner verification failure was hidden or original patched entry accepted");
    reset();need(presentation::verify_ui(image,copy,e)&&!owned_calls&&copies==5,"unhooked presentation verifier lost its complete original-body checks");
    reset();changed=image+0x6c37f0;need(!presentation::verify_ui(image,copy,e,owned)&&e.find("body differs")!=std::string::npos&&owned_calls==1,"owned UI bypassed another changed complete body");
    reset();unreadable=image+0x6c37f0;need(!presentation::verify_ui(image,copy,e,owned)&&e=="original memory read error","shared verifier overwrote a memory read cause");
    reset();fixture::throw_hash=true;need(!presentation::verify_ui(image,copy,e,owned)&&e.find("original hash backend error")!=std::string::npos,"shared verifier hid the original hash failure");
    reset();need(!presentation::verify_ui(0,copy,e,owned)&&!owned_calls&&!copies,"invalid image reached an owner or memory read");
    std::cout<<"PASS "<<checks<<" actual original-UI guard interoperability checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
