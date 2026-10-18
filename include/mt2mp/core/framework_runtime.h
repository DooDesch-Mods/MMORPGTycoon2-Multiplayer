#pragma once
#include "mt2mp/core/framework_profile.h"
#include <native_routes.h>
#include <native_ownership.h>

namespace mt2mp::framework_runtime {
bool prepared() noexcept;
void configure();
const framework_profile::Paths& paths();
const nlohmann::json& modules();
const MT2FW_MT2_RoutesV1& routes();
const MT2FW_MT2_OwnershipV1& ownership();
void publish_instance();
void render_capture();
}
