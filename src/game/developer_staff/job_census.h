#pragma once
#include "developer_staff/read.h"
#include <functional>
namespace mt2mp::game::developer_staff::job_census {
using Read=std::function<bool(uintptr_t,void*,size_t,std::string&)>;
// Local pointers are retained only for receiver binding and repeated reads.
// Queue indices and raw_id are observations, not persistent identities.
struct Bug {
    uintptr_t pointer=0,job=0,anchor=0;
    int32_t runtime_id=0,type=0,created=0,discovered=0,toons=0,variant=0;
    float reproducibility=0;
    uint8_t known=0,manual=0,debugged=0;
    bool operator==(const Bug&)const=default;
};
struct Job {
    uintptr_t pointer=0,developer=0,building=0,building_proxy=0,monster=0,monster_proxy=0,region=0;
    int32_t type=0,raw_id=0,developer_id=-1;
    uint8_t valid=0;
    std::array<float,3> position{},destination{};
    Bug bug;
    bool reciprocal=false;
    bool operator==(const Job&)const=default;
};
struct Developer {
    uintptr_t pointer=0,job=0,carried=0;
    int32_t id=-1,job_index=-1;
    bool reciprocal=false;
    bool operator==(const Developer&)const=default;
};
struct Census {
    uintptr_t queue=0,data=0;
    int32_t capacity=0;
    std::vector<Job> jobs;
    std::vector<Developer> developers;
    bool operator==(const Census&)const=default;
};
// The caller supplies a freshly authorized exact-image Mainthread Capture,
// then repeats read_process and compares full Origin/ledger/Clock/pool afterward.
// This helper makes two identical read-only queue/actor observations in between.
bool read(const Read&,const Capture&,Census&,std::string&);
nlohmann::json encode(const Census&);
}
