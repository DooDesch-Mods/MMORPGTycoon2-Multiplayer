#pragma once
#include "job_state.h"
#include "lifetime.h"
namespace mt2mp::game::bug_authority {
inline std::string job_key(const Birth& birth){return std::to_string(int(birth.kind))+":"+birth.source+":"+std::to_string(birth.sequence)+":"+std::to_string(birth.occurrence);}
inline Row bug_row(const BugIdentity& value){return {value.parent_kind,value.parent_key,value.birth,{}};}
inline BugIdentity bug_identity(const Row& value){return {value.parent_kind,value.parent_key,value.birth};}
struct LocalWork {
    JobRow row;
    uintptr_t job=0,bug=0,parent=0;
    bool retired=false;
};
struct SavedJob {uintptr_t job=0,context=0;int32_t link=0;};
class JobLifetimes {
    std::vector<LocalWork> rows_;
public:
    const std::vector<LocalWork>& rows()const{return rows_;}
    void clear(){rows_.clear();}
    LocalWork* pointer(uintptr_t pointer){for(auto& row:rows_)if(!row.retired&&row.job==pointer)return &row;return nullptr;}
    LocalWork* identity(const Birth& birth){for(auto& row:rows_)if(!row.retired&&row.row.birth==birth)return &row;return nullptr;}
    bool consumed(const Birth& birth)const{for(const auto& row:rows_)if(row.row.birth==birth)return true;return false;}
    bool born(LocalWork value,std::string& error){
        if(!value.job||value.row.birth.source.empty()||
           (value.row.type==7&&(!value.bug||!value.parent))||
           (value.row.type!=7&&(value.row.type!=0||value.row.valid||value.row.developer_id<0)))return fail(error,"Bug Job birth lacks exact receiver/Bug/parent/witness");
        if(pointer(value.job)||consumed(value.row.birth))return fail(error,"Bug Job receiver or birth already consumed");
        rows_.push_back(std::move(value));return true;
    }
    void destroyed(uintptr_t job){if(auto* row=pointer(job))row->retired=true;}
};
}
