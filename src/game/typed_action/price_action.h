#pragma once
#include <cstdint>

namespace mt2mp::action_session {
struct PriceIntent {
    uint8_t fields=0;
    int32_t purchase=0,subscription=0;
    bool operator==(const PriceIntent&) const=default;
};
struct PriceState {
    int32_t purchase=0,subscription=0;
    bool operator==(const PriceState&) const=default;
};
}
