#include "control_input/control_local_native.h"
uint32_t control_capture_compile(const mt2mp::control_input::PresentationBinding& b,mt2mp::control_input::LocalInput& out){return mt2mp::control_input::capture_local_native(b,out);}
uint32_t control_present_compile(const mt2mp::control_input::PresentationBinding& b){return mt2mp::control_input::present_local_native(b);}
