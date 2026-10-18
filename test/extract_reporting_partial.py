"""Compile the complete production callback against a failing prepare fixture."""
from pathlib import Path
import sys

source=Path(sys.argv[1]).read_text(encoding='utf-8')
cleanup=source[source.index('void stop_reporting() noexcept {'):source.index('void MT2FW_CALL stop(void*',source.index('void stop_reporting() noexcept {'))]
callback=source[source.index('void MT2FW_CALL update(void*'):source.index('void MT2FW_CALL render(void*')]
prefix=r'''
#include "mt2mp/core/reporting.h"
#include <windows.h>
#include <stdexcept>
#include <iostream>
#define MT2FW_CALL __cdecl
using namespace mt2mp;
struct {bool attempted=false,prepared=false,reporting_started=false,ready=false;DWORD thread=0;std::string failure;} state;
std::filesystem::path profile;
bool callback(void* owner){return owner==&state;}
void fail(const char* operation,const std::string& cause){state.failure=std::string(operation)+": "+cause;}
void prepare(){state.reporting_started=true;reporting::initialize(profile,nlohmann::json::object());throw std::runtime_error("original config failure");}
void stop(void*,const char*,size_t){throw std::runtime_error("prepared teardown used for a partial initialization");}
namespace mt2mp::reporting {void tick_session(){}}
'''
suffix=r'''
int main(int argc,char** argv){
  try{
    if(argc!=2)throw std::runtime_error("profile path required");
    profile=std::filesystem::path(argv[1])/reporting::random_id();state.thread=GetCurrentThreadId();
    update(&state,nullptr,0);
    if(state.reporting_started||reporting::status().at("ready").get<bool>())throw std::runtime_error("partial preparation leaked worker");
    if(state.failure!="initialization: original config failure")throw std::runtime_error("original failure was replaced");
    std::cout<<"PASS actual Framework update callback retires partial reporting and preserves original cause\n";return 0;
  }catch(const std::exception& error){reporting::shutdown();std::cerr<<error.what()<<'\n';return 1;}
}
'''
Path(sys.argv[2]).write_text(prefix+cleanup+callback+suffix,encoding='utf-8')
