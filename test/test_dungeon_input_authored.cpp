#include "dungeon_input_authored.h"
#include <iostream>
#include <stdexcept>
namespace fixture {
namespace authored=mt2mp::game::dungeon_input_authored;
using namespace mt2mp;
inline void need(bool ok,const std::string& cause){if(!ok)throw std::runtime_error(cause);}
inline authored::Scene scene(){authored::Scene v;v.dungeon=std::string(64,'b')+":world:building:17";v.key=v.dungeon+"/template";v.canvas_sha=std::string(64,'a');v.design_sha=std::string(64,'d');
    action_session::DungeonGizmoRow gizmo;gizmo.key=game::dungeon_gizmo_identity::birth("11111111-1111-4111-8111-111111111111",0);
    gizmo.value.name="chest";gizmo.value.type="Treasure";gizmo.value.variant="base";gizmo.value.scale={1,1,1};gizmo.value.rotation={0,0,0,1};v.gizmos.push_back(gizmo);
    v.monsters.push_back({"action:22222222-2222-4222-8222-222222222222",2,1,4});
    authored::Scenery decoration;decoration.category=3;decoration.bounds_valid=true;decoration.position={1,2,3};decoration.scale={1,1,1};decoration.rotation={0,0,0,1};
    v.scenery_counts[3]=1;v.scenery.push_back(decoration);return v;}
}
int main()try{using namespace fixture;size_t checks=0;std::string error,canvas,members;
    const auto check=[&](bool ok,const std::string& cause){++checks;need(ok,cause);};
    const auto original=scene();check(authored::digest({original},canvas,members,error),error);const auto canvas_before=canvas,member_before=members;
    for(int field=0;field<7;++field){auto changed=original;
        if(field==0)changed.canvas_sha=std::string(64,'d');if(field==1)changed.gizmos[0].value.gold=2;
        if(field==2)changed.monsters[0].type=5;if(field==3)changed.scenery[0].position[0]=6;
        if(field==4)changed.scenery[0].rotation={0,1,0,0};if(field==5)changed.gizmos[0].value.lock_id=9;
        if(field==6)changed.monsters.push_back({"action:33333333-3333-4333-8333-333333333333",0,1,4});
        check(authored::digest({changed},canvas,members,error),error);
        check(field==0?(canvas!=canvas_before&&members==member_before):(canvas==canvas_before&&members!=member_before),"authored effect was omitted or mixed with Canvas");}
    for(int field=0;field<7;++field){auto changed=original;
        if(field==0)changed.gizmos.push_back(changed.gizmos[0]);if(field==1)changed.monsters[0].key=changed.gizmos[0].key;
        if(field==2)changed.scenery_counts[3]=2;if(field==3)changed.scenery[0].category=4;
        if(field==4)changed.monsters[0].rank=3;if(field==5)changed.canvas_sha="not-a-sha";
        if(field==6)changed.key=std::string(64,'c')+":world:building:17/template";
        check(!authored::digest({changed},canvas,members,error)&&!error.empty(),"invalid authored owner/count silently admitted");}
    auto instance=original;instance.key=instance.dungeon+"/instance/8";instance.gizmos.clear();instance.monsters.clear();
    check(authored::digest({original,instance},canvas,members,error),error);
    instance.canvas_sha=std::string(64,'c');check(!authored::digest({original,instance},canvas,members,error),"two scene aliases disagree on one original Canvas");
    subscriber_lifecycle::Graph graph;subscriber_lifecycle::Record root;root.parent=SIZE_MAX;root.child_index=0;root.label.type=1;root.label.text="mmoCanvas";graph.records.push_back(root);
    check(authored::canvas(graph,canvas,error),error);const auto empty_canvas=canvas;
    subscriber_lifecycle::Record scalar;scalar.parent=0;scalar.child_index=0;scalar.label.type=1;scalar.label.text="bounds";scalar.tokens.push_back({4,7,{}});graph.records.push_back(scalar);
    check(authored::canvas(graph,canvas,error)&&canvas!=empty_canvas,"complete original Canvas value omitted token bits");
    subscriber_lifecycle::PrivateLink link;link.saved_link_id=1;link.object=0x1234;link.has_inline_definition=false;graph.private_links.push_back(link);
    check(!authored::canvas(graph,canvas,error)&&error.find("external SaveContext reference")!=std::string::npos,"unexpected Canvas external owner was normalized away");
    graph.private_links.clear();graph.records[1].parent=2;check(!authored::canvas(graph,canvas,error),"malformed native tree accepted");
    std::string cell,cell_changed;const std::string layout(64,'e');
    auto design_changed=original;design_changed.design_sha=std::string(64,'f');
    check(authored::digest({design_changed},canvas,members,error)&&canvas==canvas_before&&members!=member_before,"complete Grid/Layout/Plan effect omitted");
    design_changed.design_sha.clear();check(!authored::digest({design_changed},canvas,members,error),"incomplete native design admitted");
    instance.canvas_sha=original.canvas_sha;instance.design_sha=std::string(64,'f');
    check(!authored::digest({original,instance},canvas,members,error),"two scenes disagree on one original Grid/Layout/Plan");
    check(authored::cell_digest(canvas_before,layout,cell,error),error);
    check(authored::cell_digest(std::string(64,'f'),layout,cell_changed,error)&&cell_changed!=cell,"Canvas edit omitted from cell proof");
    check(authored::cell_digest(canvas_before,std::string(64,'f'),cell_changed,error)&&cell_changed!=cell,"layout edit omitted from cell proof");
    check(authored::cell_digest(layout,canvas_before,cell_changed,error)&&cell_changed!=cell,"Canvas and layout owners can be exchanged");
    for(const auto& invalid:std::vector<std::string>{"",std::string(63,'a'),std::string(64,'A'),"not-a-sha"}){
        check(!authored::cell_digest(invalid,layout,cell_changed,error)&&cell_changed.empty()&&!error.empty(),"incomplete Canvas admitted");
        check(!authored::cell_digest(canvas_before,invalid,cell_changed,error)&&cell_changed.empty()&&!error.empty(),"incomplete layout admitted");
    }
    std::cout<<checks<<" actual authored Canvas/roster/Gizmo/Scenery guards passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
