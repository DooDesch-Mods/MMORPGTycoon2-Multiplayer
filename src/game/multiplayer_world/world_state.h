#pragma once
#include "../typed_action/action_session.h"
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace mt2mp::world_state {
using Building=action_session::BuildingState;
struct Origin {
    std::string snapshot_uuid,sha256;
    uint64_t world_epoch=0,generation=0;
    bool operator==(const Origin&) const=default;
};
enum class Kind:uint8_t {Create,Move,Remove};
struct Change {
    Kind kind=Kind::Create;
    uint64_t revision=0;
    std::string action_uuid,object_key;
    Building before,after;
    bool operator==(const Change&) const=default;
};
struct SnapshotObject {
    std::string object_key,record_key;
    Building state;
};
struct Row {
    Building state;
    std::string snapshot_record_key;
    bool operator==(const Row&) const=default;
};
// A fresh Root registry/readback supplies the local UID; the shared table never owns it.
struct LocalBinding {
    Origin origin;
    std::string object_key;
    uint64_t local_uid=0;
    Building state;
};
class RegistryReader {
public:
    virtual ~RegistryReader()=default;
    virtual bool current(const std::string& object_key,LocalBinding&,std::string& error)=0;
};
enum class Admission:uint8_t {New,Duplicate,Rejected};
bool valid_object_key(const std::string&);
class Table {
public:
    bool bind_snapshot(const Origin&,const std::vector<SnapshotObject>&,uint64_t confirmed_revision,std::string&);
    bool rebind_snapshot(const Origin&,const std::vector<SnapshotObject>&,std::string&);
    Admission prepare(const Origin&,const Change&,std::string&) const;
    // Call only after native readback or the retained confirmed native Remove receipt.
    Admission commit(const Origin&,const Change&,const LocalBinding&,std::string&);
    Admission commit_moves(const Origin&,const std::vector<Change>&,const std::vector<LocalBinding>&,std::string&);
    Admission commit_creates(const Origin&,uint64_t revision,const std::string& action_uuid,
                             const std::vector<LocalBinding>&,std::string&);
    Admission commit_dungeon_census(const Origin&,uint64_t revision,const std::string& action_uuid,
        const std::string& target,const std::vector<LocalBinding>& before,
        const std::vector<LocalBinding>& after,std::string&);
    Admission commit_scenery_stroke(const Origin&,uint64_t revision,const std::string& action_uuid,
                                    const std::vector<std::string>& removed_keys,const std::vector<LocalBinding>& created,std::string&);
    Admission commit_scenery_history(const Origin&,uint64_t revision,const std::string& action_uuid,
                                     const std::vector<std::string>& removed_keys,const std::vector<LocalBinding>& restored,std::string&);
    // Update only native terrain settlement of an already confirmed placement.
    bool refresh_current(const Origin&,const std::string& object_key,const LocalBinding&,std::string&);
    bool retire_native_effects(const Origin&,uint64_t revision,const std::string& action_uuid,const std::vector<std::string>& removed_keys,std::string&);
    // Redo of one confirmed building uses its original ID and a fresh local native binding.
    bool restore_native_building(const Origin&,uint64_t revision,const std::string& action_uuid,
                                 const std::string& original_action_uuid,const LocalBinding&,std::string&);
    bool restore_native_building_key(const Origin&,uint64_t revision,const std::string& action_uuid,
                                     const std::string& object_key,const LocalBinding&,std::string&);
    bool restore_native_buildings(const Origin&,uint64_t revision,const std::string& action_uuid,
                                  const std::vector<LocalBinding>&,std::string&);
    bool resolve_current(const Origin&,const std::string& object_key,RegistryReader&,LocalBinding&,std::string&) const;
    const std::map<std::string,Row>& rows() const {return rows_;}
    uint64_t revision() const {return revision_;}
    Origin origin() const {return origin_;}
private:
    Admission commit_scenery_batch(const Origin&,uint64_t,const std::string&,const std::vector<std::string>&,
                                   const std::vector<LocalBinding>&,bool restore,std::string&);
    Origin origin_;
    uint64_t revision_=0;
    std::map<std::string,Row> rows_;
    std::optional<Change> last_;
};
}
