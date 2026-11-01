#pragma once
#include "mt2mp/core/framework_profile.h"
#include <native_routes.h>
#include <native_ownership.h>
#include <native_steps_draft.h>
#include <native_step_recovery_draft.h>
#include "mt2mp/hooks/private/native_release.h"

namespace mt2mp::framework_runtime {
bool prepared() noexcept;
void configure();
const framework_profile::Paths& paths();
const nlohmann::json& modules();
const MT2FW_MT2_RoutesV1& routes();
const MT2FW_MT2_OwnershipV1& ownership();
const MT2FW_MT2_ReleaseV1& release();
bool ordered_services(MT2FW_MT2_StepServiceDraftV1&,MT2FW_MT2_PatchesV1&,std::string&);
bool ordered_recovery(MT2FW_MT2_StepRecoveryDraftV1&,std::string&);
void publish_instance();
void render_capture();
}
