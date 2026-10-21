#pragma once
#include "raw.h"
#include <memory>
namespace mt2mp::game::npc_quest_target_runtime {
struct CloneData;
struct ClonePreview {std::shared_ptr<CloneData> state;};
bool clone_alias(const scene_clone::Raw&,const scene_clone::Pool&,const scene_clone::Member&,std::string&,std::string&);
bool preview_clones(const scene_clone::Raw&,const scene_clone::Mapping&,ClonePreview&,std::string&,const std::string& template_exterior={},const scene_clone::Raw* restoration=nullptr,const scene_clone::Raw* held=nullptr);
bool check_clones(const ClonePreview&,std::string&);
bool publish_clones(ClonePreview&,std::string&);
}
