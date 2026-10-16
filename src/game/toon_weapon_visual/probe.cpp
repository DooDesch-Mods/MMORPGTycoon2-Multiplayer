#include "probe.h"
#include "authority.h"
namespace mt2mp::game::toon_weapon_visual {
bool read_scene(placement_ui_probe::Memory& memory,Plan& p,std::string& error){
    int32_t kind=-1;uintptr_t owner=0,data=0,screen_global=0;int32_t count=0;
    if(!visual_read(memory,p.shard,0x48,kind,error))return false;
    if(kind==0){
        // Shard::GetScene selects Screen::GetScene(0). Read its existing array only.
        if(!visual_read(memory,p.image,0x164b420,screen_global,error)||!visual_read(memory,screen_global,0,owner,error)||
           !visual_read(memory,owner,0x30,count,error)||!visual_read(memory,owner,0x10,data,error))return false;
        if(count<=0||count>1048576)return visual_fail(error,"Weapon probe current Screen scene count differs");
        if(!visual_read(memory,data,0,p.scene,error))return false;
    }else if(kind==1){
        if(!visual_read(memory,p.shard,0x18,owner,error)||!visual_read(memory,owner,0x780,p.scene,error))return false;
    }else if(kind==2){
        if(!visual_read(memory,p.shard,0x20,owner,error)||!visual_read(memory,owner,0xc0,p.scene,error))return false;
    }else return visual_fail(error,"Weapon probe actual Shard kind unsupported");
    return p.scene!=0||visual_fail(error,"Weapon probe actual scene absent");
}
bool read_observation(placement_ui_probe::Memory& memory,Plan plan,Observation& out,std::string& error){
    out={};out.plan=plan;out.actor_present=plan.actor!=0;
    if(!visual_read(memory,plan.toon,0x4dc,out.actual_attack,error))return false;
    if(!read_attachment(memory,plan,out.matching,error))return false;
    if(!plan.actor)return true;
    if(!visual_read(memory,plan.skeleton,0xe8,out.instance,error)||!visual_read(memory,plan.skeleton,0xf0,out.transform,error)||
       !visual_read(memory,plan.skeleton,0xf8,out.actual_bone,error)||!visual_read(memory,plan.skeleton,0xe4,out.skeleton_visible,error)||
       !visual_read(memory,plan.skeleton,0xa0,out.skeleton_color,error))return false;
    out.attachment_present=out.instance!=0;
    if(!out.instance)return true;
    if(plan.parts.empty()||!out.transform)return visual_fail(error,"Weapon probe existing attachment lacks an admitted costume transform");
    // read_attachment has already checked this instance's original catalogue/scene owner.
    if(!visual_read(memory,out.instance,0x50,out.group,error)||!visual_read(memory,out.group,0x58,out.model,error)||
       !visual_read(memory,out.instance,0,out.matrix,error)||!visual_read(memory,out.instance,0x40,out.color,error)||
       !visual_read(memory,out.instance,0x70,out.visible,error)||!visual_read(memory,out.transform,0,out.actual_trs,error))return false;
    uintptr_t groups=0;int32_t count=0,capacity=0;
    if(!visual_read(memory,plan.rez_manager,0x98,groups,error)||!visual_read(memory,plan.rez_manager,0xa0,count,error)||
       !visual_read(memory,plan.rez_manager,0xa4,capacity,error))return false;
    if(count<0||capacity<count||capacity>1048576||(count&&!groups))return visual_fail(error,"Weapon probe attachment group bounds changed");
    int owners=0;
    for(int32_t i=0;i<count;++i){uintptr_t record=0,group=0;
        if(!visual_read(memory,groups,size_t(i)*8,record,error)||!visual_read(memory,record,8,group,error))return false;
        if(group==out.group){++owners;if(!visual_read(memory,record,0,out.actual_model_id,error))return false;}
    }
    if(owners!=1)return visual_fail(error,"Weapon probe attachment group owner changed during read");
    uintptr_t matrices=0;int32_t bones=0;
    if(!visual_read(memory,plan.skeleton,0x18,matrices,error)||!visual_read(memory,plan.skeleton,0xb0,bones,error))return false;
    if(out.actual_bone<0||out.actual_bone>=bones)return visual_fail(error,"Weapon probe actual attachment bone outside current skeleton");
    return visual_read(memory,matrices,size_t(out.actual_bone)*0x40,out.bone_matrix,error);
}
nlohmann::json encode_observation(const Observation& o){
    const auto& p=o.plan;
    return {{"read_only",true},{"attempted",false},{"read_confirmed",true},{"actor_absent",!o.actor_present},
        {"tuple",{{"weapon_ilvl",p.ilvl},{"weapon_category",p.category},{"weapon_variant_id",p.variant}}},
        {"toon_address",p.toon},{"actor_address",p.actor},{"actor_object_address",p.actor_object},{"skeleton_address",p.skeleton},
        {"scene_address",p.scene},{"attachment_address",o.instance},{"group_address",o.group},{"model_address",o.model},
        {"selected_model_id",p.model_id},{"selected_model_address",p.model},{"actual_model_id",o.actual_model_id},{"selected_bone",p.parts.empty()?-1:p.parts.back().bone},
        {"actual_bone",o.actual_bone},{"attack",o.actual_attack},{"attachment_present",o.attachment_present},
        {"attachment_matching",o.matching},{"visible",o.visible!=0},{"skeleton_visible",o.skeleton_visible!=0},
        {"actual_transform",o.actual_trs},{"actual_matrix",o.matrix},{"bone_matrix",o.bone_matrix},
        {"color",o.color},{"skeleton_color",o.skeleton_color},
        {"matrix_observed",o.matrix_observed},{"matrix_matching",o.matrix_matching},{"matrix_cause",o.matrix_cause},
        {"matrix_call",{{"attempted",o.matrix_call.attempted},{"returned",o.matrix_call.returned},{"exception",o.matrix_call.exception}}}};
}
}
