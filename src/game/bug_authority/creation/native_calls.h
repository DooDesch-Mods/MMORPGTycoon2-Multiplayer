#pragma once
#include "adapter.h"
namespace mt2mp::game::developer_staff::bug_creation {
using SetAnchor=void(*)(uintptr_t,uintptr_t);
using Authorize=std::function<bool(const Lease&,std::string&)>;
struct CollectionReceipt {
    Lease requested;
    bool admitted=false,attempted=false,returned=false,confirmed=false,unknown=false;
    uintptr_t collection=0;
    std::string cause;
    std::string request_rejection;
};
// The integration owns the exact-image detour and Mainthread authorization.
// Its hook passes _ReturnAddress() directly to dispatch_anchor. There is no
// hook installation or native endpoint in this private adapter.
class NativeServices final:public Services {
public:
    NativeServices(Read read,Authorize authorize,SetAnchor original):read_(std::move(read)),authorize_(std::move(authorize)),original_(original){}
    bool authorize(const Lease&,std::string&)override;
    bool read(const Lease&,Snapshot&,std::string&)override;
    bool add_specific(const Lease&,const Value&,const Anchor&,uintptr_t&,std::string&)override;
    bool scalars(const Lease&,uintptr_t,const Value&,std::string&)override;
    bool anchor(const Lease&,uintptr_t,std::string&)override;
    bool ensure_collection(const Lease&,CollectionReceipt&);
    static bool dispatch_anchor(uintptr_t bug,uintptr_t parent,uintptr_t return_address);
private:
    Read read_;
    Authorize authorize_;
    SetAnchor original_=nullptr;
};
}
