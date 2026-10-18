#include "visual.h"
#include "random_diagnostic.h"
#include <exception>
namespace mt2mp::game::toon_weapon_visual {
bool reconcile(Calls& calls,uintptr_t toon,Receipt& r,std::string& error){
    auto fail=[&](const std::string& cause){
        r.attempted|=r.remove.attempted||r.cache.attempted;
        for(const auto& call:r.attachments)r.attempted|=call.attempted;
        error=cause;r.cause=cause;r.unknown=r.attempted;return false;
    };
    auto observed=[&](const Call& call){r.attempted|=call.attempted;};
    try{
        Lease before;
        if(!calls.pin(toon,before,r.owned,error)){
            r.subscriber_id=before.subscriber_id;r.authority_failure=before.authority_failure;
            return fail(error);
        }
        if(!r.owned)return true;
        RandomDiagnostic random(calls,r.random);
        r.subscriber_id=before.subscriber_id;
        Plan plan;if(!calls.plan(toon,plan,error))return fail(error);
        r.ilvl=plan.ilvl;r.variant=plan.variant;r.category=plan.category;r.attack=plan.attack;r.model_id=plan.model_id;
        if(!plan.parts.empty())r.bone=plan.parts.back().bone;
        if(plan.actor&&!calls.scene(plan,r.scene,error))return fail(error);
        bool matching=false;if(!calls.matches(plan,matching,error))return fail(error);
        Lease pinned;bool still_owned=false;
        if(!calls.pin(toon,pinned,still_owned,error))return fail(error);
        if(!still_owned||pinned!=before)return fail("Weapon visual owner changed before mutation");
        Plan fresh;if(!calls.plan(toon,fresh,error))return fail(error);
        fresh.scene=plan.scene;
        if(fresh!=plan)return fail("Weapon visual tuple or local owner changed before mutation");
        if(matching){
            if(!plan.parts.empty()&&!calls.matrix(plan,plan.parts.back(),r.matrix,error))return fail(error);
            r.deferred=!plan.actor;r.confirmed=true;return true;
        }
        if(plan.actor){const bool removed=calls.remove(plan,r.remove,error);observed(r.remove);if(!removed)return fail(error);}
        if(plan.prior_attack!=plan.attack){const bool changed=calls.cache(plan,r.cache,error);observed(r.cache);if(!changed)return fail(error);}
        for(const auto& part:plan.parts){
            r.attachments.emplace_back();const bool attached=calls.attach(plan,part,r.attachments.back(),error);
            observed(r.attachments.back());if(!attached)return fail(error);
        }
        bool after_owned=false;Lease after;
        if(!calls.pin(toon,after,after_owned,error))return fail(error);
        if(!after_owned||after!=before)return fail("Weapon visual owner changed after mutation");
        Plan actual;if(!calls.plan(toon,actual,error))return fail(error);
        actual.scene=plan.scene;plan.prior_attack=plan.attack;
        if(actual!=plan)return fail("Weapon visual tuple or local owner changed after mutation");
        if(!calls.matches(plan,matching,error))return fail(error);
        if(!matching)return fail("Weapon visual attachment/cache final readback differs");
        if(!plan.parts.empty()&&!calls.matrix(plan,plan.parts.back(),r.matrix,error))return fail(error);
        r.deferred=!plan.actor;r.confirmed=true;return true;
    }catch(const std::exception& e){return fail(std::string("Weapon visual exception: ")+e.what());}
    catch(...){return fail("Weapon visual nonstandard exception");}
}
}
