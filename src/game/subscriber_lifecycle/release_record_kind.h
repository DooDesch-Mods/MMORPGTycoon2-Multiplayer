#pragma once
#include <cstdint>

namespace mt2mp::subscriber_lifecycle {
enum class ReleaseRecordKind:uint8_t {Manager=0,PreparedDemands=1,GameMasterTickets=2};
// SaveToRecord labels the root with the original RTTI name, including ObjectArray.
constexpr const char* release_record_root(ReleaseRecordKind kind){
    switch(kind){
    case ReleaseRecordKind::Manager:return "mmoReleaseManager";
    case ReleaseRecordKind::PreparedDemands:return "vsObjectArray<mmoReleaseDemand>";
    case ReleaseRecordKind::GameMasterTickets:return "vsObjectArray<mmoTicket>";
    }
    return nullptr;
}
}
