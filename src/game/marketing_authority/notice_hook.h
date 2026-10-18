#pragma once
#include <cstdint>
#include <string>
namespace mt2mp::game::marketing_authority::notice_hook {
using Original=bool(*)(uintptr_t);
struct Observer {virtual ~Observer()=default;virtual bool observe(uintptr_t,Original)=0;};
bool initialize(uintptr_t image,std::string&);
bool verify(uintptr_t image,std::string&);
bool arm(Observer*,uint32_t thread,std::string&);
void disarm(Observer*);
bool healthy(std::string&);
}
