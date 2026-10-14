#pragma once
#include "reference_map.h"
#include <array>
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::subscriber_lifecycle::first_append {
struct ExternalReference {int32_t local_link_id=0;ReferenceKey key;};
struct Payload {
    std::string baseline_sha,record_sha,references_sha;
    int32_t id=-1,next_id=-1;bool online=false;
    std::vector<Record> records;
    std::vector<int32_t> local_ids;
    std::vector<ExternalReference> external;
    std::array<uint32_t,7> needs_bits{};
};
bool parse_payload(const nlohmann::json&,Payload&,std::string&);
}
