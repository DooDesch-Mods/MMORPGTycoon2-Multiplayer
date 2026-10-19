#include "test_dungeon_gizmo_fixture.h"
#include <vendor/nlohmann/json.hpp>
#include <iostream>
#include <limits>
int main()try{size_t checks=0;std::string error;std::vector<uint8_t> bytes;const auto check=[&](bool value,const std::string& cause){++checks;need(value,cause);};
    for(unsigned edit=0;edit<5;++edit)for(bool saved:{false,true})for(bool instance:{false,true})for(bool equal:{false,true})for(int32_t desired:{0,5}){
        gizmo_edit=DungeonGizmoEdit(edit);saved_key=saved;instance_scene=instance;equal_apply=equal;desired_value=desired;
        Message decoded;auto capture=request();check(encode(capture,bytes,error),error);check(decode(bytes.data(),bytes.size(),decoded,error)&&decoded==capture,error);
        auto original=result();check(encode(original,bytes,error),error);check(decode(bytes.data(),bytes.size(),decoded,error)&&decoded==original,error);
        check(dungeon_gizmo_wire_fits(original.request,original.evidence.dungeon_gizmo_before,money(),error),error);
        for(int mode=0;mode<10;++mode){auto bad=original;auto& row=bad.evidence.dungeon_gizmo_after.scenes[0].rows[0];
            if(mode==0)++bad.evidence.after.cash;if(mode==1)bad.evidence.undo_confirmed=true;if(mode==2)bad.evidence.target_retired=true;if(mode==3)bad.evidence.selection_confirmed=true;
            if(mode==4)++bad.evidence.dungeon_gizmo_after.global_random[0];if(mode==5)row.value.parent.clear();if(mode==6)++row.value.position[0];
            if(mode==7)bad.evidence.dungeon_gizmo_after.scenes[0].rows.pop_back();if(mode==8)bad.evidence.dungeon_gizmo_after.protected_sha256=std::string(64,'d');if(mode==9)row.value.name="other";
            check(!encode(bad,bytes,error),"foreign Gizmo result accepted mode="+std::to_string(mode));
        }
    }
    for(int32_t bad_value:{-1,6,INT32_MAX}){auto bad=request();bad.request.dungeon_gizmo.value=bad_value;check(!encode(bad,bytes,error),"outside normal Spinner range accepted");}
    auto good=result();check(encode(good,bytes,error),error);auto document=nlohmann::json::parse(bytes.begin(),bytes.end());
    document["request"]["dungeon_gizmo"]["value"]=1.5;auto text=document.dump();Message decoded;check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,error),"fractional native integer accepted");
    auto bad=good;bad.request.dungeon_gizmo.scene=std::string(64,'b')+":world:building:18/template";check(!encode(bad,bytes,error),"foreign scene target accepted");
    bad=good;bad.evidence.dungeon_gizmo_before.scenes[0].rows[0].value.parent="action:33333333-3333-4333-8333-333333333333:dungeon:gizmo:0";
    bad.evidence.dungeon_gizmo_after=after(bad.evidence.dungeon_gizmo_before);check(!encode(bad,bytes,error),"missing prototype admitted");
    bad=good;bad.evidence.dungeon_gizmo_before.scenes[0].rows[0].value.position[0]=std::numeric_limits<float>::quiet_NaN();check(!encode(bad,bytes,error),"non-finite native authored field accepted");
    auto oversized=before();oversized.scenes[0].rows[1].value.name=std::string(max_payload,'x');
    check(!dungeon_gizmo_wire_fits(request().request,oversized,money(),error)&&error=="action payload exceeds "+std::to_string(max_payload)+" bytes","oversized full result not rejected before effect: "+error);
    auto mixed=request();mixed.request.dungeon_palette.key=intent().key;check(!encode(mixed,bytes,error),"cross-family intent accepted");
    std::cout<<checks<<" Gizmo complete protocol/result/range/reference/packet comparisons passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
