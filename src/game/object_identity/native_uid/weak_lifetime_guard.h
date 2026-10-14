#pragma once
#include <cstdint>
#include <string>

namespace mt2mp::native_uid {
inline constexpr const char* required_exe_sha="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
struct World {
    uint64_t epoch=0,origin_generation=0;
    void* world=nullptr;
    void* map=nullptr;
    bool operator==(const World&)const=default;
};
struct Proof {
    World world;
    std::string exe_sha,weak_contract_sha;
    bool exact_entryguards=false,current_native_open=false,main_thread=false,effects_idle=false;
};
// A retained native proxy is private. It must stay owned across remove tombstones.
struct Binding {World world;void* object=nullptr;void* proxy=nullptr;uint64_t uid=0;};
struct LeaseReadback {void* proxy=nullptr;bool native_retain_confirmed=false,continuously_owned=false;};
struct ProxyReadback {void* target=nullptr;int32_t references=0;};
struct BuildingReadback {
    void* registry_object=nullptr;
    void* weak_proxy=nullptr;
    uint64_t uid=0;
    bool typed_building=false,complete_ownership=false;
};
struct Backend {
    virtual ~Backend()=default;
    virtual bool current_proof(Proof&,std::string&)=0;
    virtual bool retained_lease(const Binding&,LeaseReadback&,std::string&)=0;
    virtual bool proxy_read(void*,ProxyReadback&,std::string&)=0;
    virtual bool building_read(void*,uint64_t,BuildingReadback&,std::string&)=0;
};
struct Result {bool capable=false,removed=false;uint64_t private_lifetime_token=0;std::string cause;};
// This guard does not acquire/release native references or install hooks.
Result check_retained(const Binding&,const std::string& frozen_weak_contract_sha,Backend&);
using NativeWeakAssignFn=void(*)(void* existing_native_weak,void* building);
using NativeProxyDecRefFn=void(*)(void* retained_proxy);
using NativeFindUIDFn=void*(*)(uint64_t uid);
}
