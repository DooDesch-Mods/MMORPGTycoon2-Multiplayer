#pragma once
#include "runtime.h"
#include "job_lifetime.h"
#include "developer_staff/job_census.h"
namespace mt2mp::game::bug_authority {
bool capture_work(const developer_staff::Capture&,const developer_staff::job_census::Census&,WorkState&,std::vector<uintptr_t>&,std::string&);
bool has_work();
bool preflight_work(const developer_staff::Capture&,const WorkState&,std::string&);
bool begin_work(const developer_staff::Capture&,const WorkState&,bool& attempted,std::string&);
bool work_actor_links(const developer_staff::Capture&,const WorkState&,uintptr_t developer_property,uintptr_t job_property,bool attach,bool& attempted,std::string&);
bool finish_work(const developer_staff::Capture&,const WorkState&,bool& attempted,std::string&);
}
