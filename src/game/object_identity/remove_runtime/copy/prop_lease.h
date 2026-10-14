#pragma once
#include "weak_lifetime_guard.h"
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mt2mp::prop_lease {
using World=mt2mp::native_uid::World;
using FactoryFn=void*(*)(const void*);
using AssignFn=void(*)(void* native_weak_subobject,void* prop);
using DeleteFn=void(*)(void* native_link);
struct Api {
    FactoryFn create=nullptr;
    AssignFn assign=nullptr;
    DeleteFn destroy=nullptr;
    uintptr_t link_addresspoint=0,weak_addresspoint=0;
};
Api for_module(uintptr_t)noexcept;
bool validate_native_api(uintptr_t,const Api&,std::string&);
struct Proof {World world;bool main_thread=false,current_native_open=false,effects_idle=false;};
// Root reads a current full class and membership witness, never RTTI name alone.
struct Prop {
    uint64_t uid=0;
    void* registry_object=nullptr;
    bool typed_prop=false,complete_ownership=false;
    uintptr_t class_addresspoint=0;
    std::string membership_receipt;
};
struct Retirement {World old_world;std::string snapshot_receipt;uint64_t receipt=0;};
struct PrivateLease {
    void* holder=nullptr;
    void* object=nullptr;
    void* proxy=nullptr;
    uint64_t uid=0;
    World world;
    // A unique own-holder allocation receipt, not a native Prop creation counter.
    uint64_t generation=0;
    uintptr_t class_addresspoint=0;
    bool operator==(const PrivateLease&)const=default;
};
struct Host {
    virtual ~Host()=default;
    virtual bool validate_exact_api(const Api&,std::string&)=0;
    virtual bool current(Proof&,std::string&)=0;
    virtual bool read_prop(void*,uint64_t,Prop&,std::string&)=0;
    virtual bool reserve_generation(uint64_t&,std::string&)=0;
    virtual bool allocation_returned(const PrivateLease&,std::string&)=0;
    virtual bool validate_retirement(const PrivateLease&,const Retirement&,std::string&)=0;
    virtual bool invoke_native_delete_once(DeleteFn,const PrivateLease&,const Retirement&,std::string&,uint32_t&)=0;
    virtual bool confirm_native_delete(const PrivateLease&,const Retirement&,std::string&)=0;
};
enum class Phase {Absent,Prepared,Allocated,Held,Tombstone,Detached,Released,Failed,Unknown};
struct Receipt {
    Phase phase=Phase::Absent;
    uint32_t factory_attempted=0,factory_returned=0,retain_attempted=0,retain_confirmed=0;
    uint32_t release_attempted=0,release_confirmed=0,delete_attempted=0,delete_confirmed=0,seh_code=0;
    std::array<char,512> cause{};
    bool cause_truncated=false;
};
struct Status {size_t leases=0,held=0,tombstones=0,released=0;bool unknown_lock=false,native_game_proven=false,runtime_capability=false;};
struct Diagnostics {virtual ~Diagnostics()=default;virtual void error_unretired_native_ownership(size_t,bool)noexcept=0;};
struct PrivatePoolRecord {std::string key;PrivateLease lease;Receipt receipt;};
struct PrivateOwnershipRead {PrivateLease lease;Phase phase=Phase::Absent;uintptr_t proxy_target=0;int32_t proxy_references=0;};
namespace detail {bool read_owned_fields(const Api&,const PrivateLease&,Phase,PrivateOwnershipRead&,std::string&);}
class Pool {
public:
    Pool(Api api,Diagnostics& diagnostics):api_(api),diagnostics_(diagnostics){}
    Pool(const Pool&)=delete;Pool& operator=(const Pool&)=delete;
    Pool(Pool&&)=delete;Pool& operator=(Pool&&)=delete;
    Receipt acquire(const std::string&,void*,uint64_t,const World&,Host&);
    bool read(const std::string&,PrivateLease&,Host&,std::string&);
    Receipt tombstone(const std::string&,Host&);
    Receipt release(const std::string&,const Retirement&,Host&);
    Receipt observe(const std::string&)const;
    bool inspect(const std::string&,PrivateLease&,Receipt&)const;
    bool enumerate_private(std::vector<PrivatePoolRecord>&,std::string&)const;
    bool read_owned_private(const std::string&,PrivateOwnershipRead&,Host&,std::string&);
    bool busy()const noexcept{return busy_;}
    Status status()const;
    ~Pool()noexcept;
private:
    struct Record {PrivateLease lease;Receipt receipt;};
    Api api_;Diagnostics& diagnostics_;std::map<std::string,Record> records_;
    bool busy_=false,unknown_lock_=false;
    static Receipt error_receipt(Phase,const char*)noexcept;
    static void cause(Receipt&,const char*)noexcept;
    void unknown(Record&,const char*,uint32_t=0)noexcept;
    bool proof(Host&,const World*,std::string&);
    bool checked_live(Record&,Host&,bool,std::string&);
};
}
