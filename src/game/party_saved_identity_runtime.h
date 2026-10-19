#pragma once
#include "party_saved_identity.h"
#include "party_snapshot_alias.h"
#include "dungeon_party_census.h"
#include "dungeon_scene_identity_runtime.h"
namespace mt2mp::game::party_saved_identity_runtime {
using PropertyCall=bool(*)(const void*,void*,void*,void*);
using Ticket=uint64_t;
struct Current {std::string key;party_identity::Entry binding;dungeon_party_census::Member member;};
struct NativeCensus {
    party_identity::Scope scope;
    dungeon_scene_identity_runtime::World world;
    std::vector<dungeon_party_census::Pool> pools;
    std::vector<dungeon_party_census::Member> members;
};
bool read_current_raw(NativeCensus&,std::string&);
bool read_current(std::vector<Current>&,std::string&);

struct CaptureWorld {void* game=nullptr;void* mode=nullptr;void* state=nullptr;void* world=nullptr;void* map=nullptr;};
bool initialize(std::string&);
void* entrypoint(bool save);
bool confirm_hooks(std::string&);
bool stage_scenes(dungeon_scene_identity_runtime::Census,std::string&);
bool arm_host(const std::string&,const CaptureWorld&,std::string&);
bool seal_host(const std::string&,const std::string&,const CaptureWorld&,std::string&);
bool prepare_aliases(const std::string&,const std::string&,uint64_t,const party_snapshot_alias::Manifest&,std::string&);
bool aliases(const std::string&,const std::string&,party_snapshot_alias::Manifest&,std::string&);
bool arm_client(const std::string&,const std::string&,std::string&);
bool seal_client(std::string&);
void cancel(const std::string&);
Ticket save_before(const void*,void*,void*,void*)noexcept;
void save_after(Ticket,const void*,void*,void*,void*,bool)noexcept;
Ticket load_before(const void*,void*,void*,void*)noexcept;
void load_after(Ticket,const void*,void*,void*,void*,bool)noexcept;
void children_before(uintptr_t,void*)noexcept;
void children_after(uintptr_t,void*,int32_t)noexcept;
void registered(uintptr_t,void*,int32_t,void*)noexcept;
void original_failed(Ticket,const std::string&)noexcept;
}
