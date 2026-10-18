#pragma once
#include "../typed_action/action_session.h"
#include "../placement_ui_probe/ui_probe.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mt2mp::game::toon_weapon_visual {
struct Lease {
    action_session::Context context;
    action_session::Binding binding;
    uintptr_t subscriber=0,toon=0;
    int32_t subscriber_id=-1;
    std::array<uintptr_t,5> origin{};
    uint64_t origin_bytes=0;
    std::string origin_uuid,origin_sha;
    nlohmann::json authority_failure=nullptr;
    bool operator==(const Lease&)const=default;
};
struct Call { bool attempted=false,returned=false;uint32_t exception=0; };
struct RandomObservation {
    bool before_observed=false,after_observed=false,observed=false,unchanged=false;
    std::array<uint64_t,4> before{},after{};
    std::string before_cause,after_cause,cause;
};
struct Receipt {
    RandomObservation random;
    nlohmann::json authority_failure=nullptr,native_callers=nullptr;
    int32_t subscriber_id=-1;
    bool owned=false,deferred=false,attempted=false,confirmed=false,unknown=false;
    bool hook=false,forwarded=false;
    int32_t ilvl=0,variant=0,bone=-1,model_id=-1,attack=13;
    std::string category,cause;
    Call scene,matrix,remove,cache;
    std::vector<Call> attachments;
};
// Set owned before validating an identified Client. Only unrelated receivers pass
// through on failure. The provider pins the full current World and actual census.
using Owner=bool(*)(uintptr_t toon,Lease&,bool& owned,std::string&);
using Publish=void(*)(uintptr_t toon,const Receipt&) noexcept;
struct Callbacks {Owner owner=nullptr;Publish publish=nullptr;};
struct Part {
    uintptr_t descriptor=0,attachment=0,name=0,transform=0;
    int32_t bone=-1;
    std::array<float,10> trs{};
    bool operator==(const Part&)const=default;
};
struct Plan {
    uintptr_t image=0,toon=0,type=0,costume=0,actor=0,actor_object=0,skeleton=0,shard=0;
    uintptr_t manager=0,rez_manager=0,model=0,scene=0;
    int32_t ilvl=0,variant=0,prior_attack=13,attack=13,model_id=-1;
    std::string category;
    std::vector<Part> parts;
    bool humanoid=false;
    bool operator==(const Plan&)const=default;
};
bool read_plan(placement_ui_probe::Memory&,uintptr_t image,uintptr_t toon,Plan&,std::string&);
bool read_attachment(placement_ui_probe::Memory&,const Plan&,bool& matching,std::string&);

// Native calls are separate from the actual orchestration so its partial effects
// and no-replay behavior can be tested without replacing the orchestration body.
class Calls {
public:
    virtual ~Calls()=default;
    virtual bool random(std::array<uint64_t,4>&,std::string& error){error="Weapon RNG observer unavailable";return false;}
    virtual bool pin(uintptr_t,Lease&,bool&,std::string&)=0;
    virtual bool plan(uintptr_t,Plan&,std::string&)=0;
    virtual bool scene(Plan&,Call&,std::string&)=0;
    virtual bool matches(const Plan&,bool&,std::string&)=0;
    virtual bool remove(const Plan&,Call&,std::string&)=0;
    virtual bool cache(const Plan&,Call&,std::string&)=0;
    virtual bool attach(const Plan&,const Part&,Call&,std::string&)=0;
    virtual bool matrix(const Plan&,const Part&,Call&,std::string&)=0;
};
bool reconcile(Calls&,uintptr_t,Receipt&,std::string&);
bool initialize(uintptr_t image,const Callbacks&,std::string&);
// initialize installs/enables only this owned hook, following existing native owners.
bool verify_hook(std::string&);
bool refresh(uintptr_t toon,Receipt&,std::string&);
}
