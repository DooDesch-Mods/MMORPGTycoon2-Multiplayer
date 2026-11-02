#pragma once
#include "native_steps_draft.h"

namespace mt2mp::core {
inline bool countdown_owner(const MT2FW_MT2_StepReceipt& step){
    return step.phase==MT2FW_MT2_STEP_ENTERED&&step.workers_held==0&&step.callbacks_inflight==1&&
        step.original_attempted==1&&step.original_returned==0&&step.invocation&&step.lease&&step.session&&step.world_epoch;
}
}
