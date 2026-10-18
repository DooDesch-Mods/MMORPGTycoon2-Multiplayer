#include "live_host_state/memory_query_timing.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <limits>
namespace timing=mt2mp::game::live_host_state::apply_timing;
int checks=0;
void need(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
namespace host_copy {
bool fail(std::string& e,const char* reason){if(e.empty())e=reason;return false;}
struct ProcessMemory {bool copy(uintptr_t,void*,size_t,std::string&);};
#include "host_copy.inc"
}
#include "root_region_scope_cases.inc"
int main(int argc,char** argv)try{
    need(argc==2,"root fixture requires explicit baseline/candidate mode");
    const std::string mode=argv[1];need(mode=="baseline"||mode=="candidate","unknown root fixture mode");
    timing::Scope measured(timing::Part::Take);
    root_region_scope::run(mode=="baseline");
    std::cout<<checks<<" actual root-region checks passed\n";return 0;
}catch(const std::exception& problem){std::cerr<<"Root-region fixture failed after "<<checks<<": "<<problem.what()<<'\n';return 1;}
