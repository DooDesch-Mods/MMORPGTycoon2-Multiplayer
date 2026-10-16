#pragma once
#include "job_state.h"
#include "job_birth.h"
namespace mt2mp::game::bug_authority {
struct ParentOwner {uintptr_t image=0,object=0,shard=0;ParentKind kind=ParentKind::Building;std::string key;};
bool read_parent_visual(const JobRead&,const ParentOwner&,ParentVisual&,std::string&);
struct ParentServices {
    virtual ~ParentServices()=default;
    virtual bool authorize(const ParentOwner&,std::string&)=0;
    virtual bool observe(const ParentOwner&,ParentVisual&,std::string&)=0;
    virtual bool state(const ParentOwner&,int32_t,JobCall&,std::string&)=0;
    virtual bool detail(const ParentOwner&,const ParentVisual&,const ParentVisual&,bool&,std::string&)=0;
    virtual bool refresh(const ParentOwner&,JobCall&,std::string&)=0;
};
struct ParentReceipt {bool attempted=false,confirmed=false,unknown=false;std::string cause;};
bool apply_parent_visual(ParentServices&,const ParentOwner&,const ParentVisual&,ParentReceipt&);
}
