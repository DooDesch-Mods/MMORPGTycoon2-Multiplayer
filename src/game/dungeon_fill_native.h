#pragma once
#include "dungeon_fill_record.h"
#include "subscriber_lifecycle/record_decode.h"
#include "network_object_identity.h"
namespace mt2mp::game::dungeon_fill {
struct Intent {std::string desired_sha256;nlohmann::json records=nlohmann::json::array();bool operator==(const Intent&)const=default;};
struct State {bool present=false;std::string sha256;bool operator==(const State&)const=default;};
struct Owned {uintptr_t pointer=0;bool constructed=false,adopted=false,unknown=false;};
inline bool valid(const Intent& intent){std::vector<subscriber_lifecycle::Record> rows;std::string error;
    return network_object_identity::sha(intent.desired_sha256)&&records(intent.records,rows,error);}
bool read(uintptr_t image,subscriber_lifecycle::Memory&,uintptr_t fill,State&,nlohmann::json& records,std::string&);
bool create(uintptr_t image,subscriber_lifecycle::Memory&,const Intent&,Owned&,std::string&);
bool discard(uintptr_t image,Owned&,std::string&);
}
