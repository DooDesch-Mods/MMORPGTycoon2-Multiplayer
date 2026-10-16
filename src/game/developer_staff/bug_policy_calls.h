#pragma once
#include "bug_policy.h"
namespace mt2mp::game::developer_staff::bug_policy {
uint32_t original_page(uintptr_t image,uintptr_t tabs);
uint32_t original_input(uintptr_t image,uintptr_t checkbox,Input);
}
