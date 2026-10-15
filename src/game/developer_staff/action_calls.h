#pragma once
#include "probe.h"
#include "ui_capture.h"
namespace mt2mp::game::developer_staff::action_runtime {
uint32_t invoke_ui(initial_release::UiCommand,uintptr_t image,uintptr_t window,bool hire,NativeReceipt*);
uint32_t invoke_count(uintptr_t image,uintptr_t lounge,int32_t count);
UiIntent classify(uintptr_t image,const void* record,uint32_t& fault)noexcept;
}
