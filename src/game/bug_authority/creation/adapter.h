#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace mt2mp::game::developer_staff::bug_creation {
struct Value {
    int32_t type=0,created=-1,discovered=-1,toons=0,variant=-1;
    float reproducibility=0;
    bool known=false,manual=false,debugged=false;
    bool operator==(const Value&)const=default;
};
enum class Parent {Building,Npc};
// All addresses are local. The caller resolves the parent and birth through the
// existing action or native-snapshot witness before authorizing this lease.
struct Lease {
    uintptr_t image=0,parent=0,collection=0,tracker=0,lounge=0;
    Parent kind=Parent::Building;
    std::string snapshot,parent_key,birth;
    uint64_t epoch=0,generation=0;
    bool operator==(const Lease&)const=default;
};
struct Bug {
    uintptr_t pointer=0,proxy=0,job=0,anchor=0;
    int32_t runtime_id=0;
    Value value;
    bool operator==(const Bug&)const=default;
};
struct Snapshot {
    std::vector<uintptr_t> collection,all,known;
    std::vector<Bug> bugs;
    bool operator==(const Snapshot&)const=default;
};
using Read=std::function<bool(uintptr_t,void*,size_t,std::string&)>;
bool observe(const Read&,const Lease&,Snapshot&,std::string&);
using Anchor=std::function<bool(uintptr_t bug,uintptr_t parent,uintptr_t return_address,std::string&)>;
struct Services {
    virtual ~Services()=default;
    // Must verify the current Mainthread/Clock0 Client context, exact image,
    // Origin, parent resolver, birth witness and published SetAnchor detour.
    virtual bool authorize(const Lease&,std::string&)=0;
    virtual bool read(const Lease&,Snapshot&,std::string&)=0;
    virtual bool add_specific(const Lease&,const Value&,const Anchor&,uintptr_t&,std::string&)=0;
    virtual bool scalars(const Lease&,uintptr_t,const Value&,std::string&)=0;
    virtual bool anchor(const Lease&,uintptr_t,std::string&)=0;
};
struct Receipt {
    Lease lease;
    Value requested;
    bool admitted=false,attempted=false,returned=false,anchor_seen=false;
    bool properties_returned=false,anchor_returned=false,confirmed=false,unknown=false;
    uintptr_t bug=0;
    std::string cause;
    std::string request_rejection;
};
// Reuse the same retained receipt for retries. An attempted receipt is never
// executed again, including after an exception or incomplete native return.
bool create(Services&,const Lease&,const Value&,Receipt&);
}
