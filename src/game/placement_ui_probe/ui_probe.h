#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace placement_ui_probe {
enum class Family { Building,Scenery,Terrain,Path,NetworkNode,NetworkPipe,TerrainHeight,RegionLevel,NPC,MonsterZone };
struct Request { std::string token,type="SmallInn",variant; std::array<float,3> position{}; Family family{Family::Building}; std::int32_t category{-1},variant_hash{},terrain_style{-1}; };
struct Context { std::uintptr_t image{},world{}; std::uint64_t epoch{},generation{}; std::uint32_t thread{}; bool operator==(const Context&) const=default; };
struct Binding { Context context; std::uintptr_t player{},inventory{},inventory_item{},type{},behaviour{},cursor{},bar{},content{},slot{},button{},item{},item_proxy{},type_proxy{}; Family family{Family::Building}; std::int32_t category{-1},variant_hash{},terrain_style{-1}; bool operator==(const Binding&) const=default; };
enum class Phase { Rejected, Attempted, ArmedAwaitingGameUpdate, CapturedAwaitingHost, HostConfirmed, Unknown };
struct NativePose {std::array<float,3> position{};std::array<float,4> rotation{};std::array<float,3> scale{};bool operator==(const NativePose&)const=default;};
struct Receipt { Phase phase{Phase::Rejected}; std::string token,action_uuid,cause; std::uint32_t attempted_mask{},returned_mask{},exception{}; std::uint64_t capture_count{},host_confirmed_count{}; NativePose native_pose;bool native_pose_measured=false; };
class Memory { public: virtual ~Memory()=default; virtual bool copy(std::uintptr_t,void*,std::size_t,std::string&)=0; };
class ProcessMemory final: public Memory { public: bool copy(std::uintptr_t,void*,std::size_t,std::string&) override; };
// This reader follows the actual player inventory and actionbar registry, not supplied handles.
bool read_existing_tool(Memory&,const Context&,const Request&,Binding&,std::string&);
bool read_scenery_mode_window(Memory&,const Binding&,std::uintptr_t&,std::string&);
bool read_scenery_mode_button(Memory&,const Binding&,std::uintptr_t window,std::uintptr_t button,std::string&);
bool read_palette_index(Memory&,const Binding&,const Request&,std::int32_t&,std::uintptr_t& model_list,std::string&);
bool read_live_pose_and_pending(Memory&,const Binding&,const Request&,bool require_pending,std::string&);
bool read_scenery_release_pose(Memory&,const Binding&,const Request&,NativePose&,std::string&);
bool merge_release_admission(const Request&,const Receipt&,Receipt&,std::string&);
bool classify_actual_admission(const Request&,Receipt&,std::string&);
class RootServices {
public:
    virtual ~RootServices()=default;
    virtual bool actual_context(Context&,std::string&)=0;
    virtual bool actual_input_routes(const Binding&,std::string&)=0;
    virtual bool reserve_job_once(const Request&,const Binding&,std::string&)=0;
    virtual bool read_actual_admission(const Request&,Receipt&,std::string&)=0;
    virtual void record_receipt(const Request&,const Receipt&) noexcept=0;
};
class Probe {
public:
    bool initialize(const Context&,std::string&);
    Receipt arm(const Request&,RootServices&);
    Receipt observe(const Request&,RootServices&);
private:
    Context context_{};
};
}
