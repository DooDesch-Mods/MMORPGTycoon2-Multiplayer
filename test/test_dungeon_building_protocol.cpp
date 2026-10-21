#include "dungeon_building_fixture.h"
#include <iostream>
int main()try{using namespace dungeon_building_fixture;unsigned checks=0;std::string error;std::vector<uint8_t> bytes;auto decoded=std::make_unique<Message>();
    const auto check=[&](bool ok,const std::string& why){++checks;need(ok,why+" cause="+error);};
    const auto rejects=[&](const nlohmann::json& value,const char* why){const auto text=value.dump();check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),*decoded,error),why);};
    for(const auto action:{Action::PlaceBuilding,Action::MoveBuilding,Action::RemoveSnapshotBuilding,Action::UndoBuilding,Action::RedoBuilding})for(bool world:{false,true}){
        for(bool confirmed:{false,true}){auto value=std::make_unique<Message>(message(action,world,confirmed));
            check(encode(*value,bytes,error),"Valid complete Dungeon Building rejected");check(decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==*value,"Full Dungeon Building/Mapping3/RNG/Ledger roundtrip changed");
            const auto wire=nlohmann::json::parse(bytes);check(wire.at("schema")==37,"Dungeon Building did not select exact schema37");
            if(confirmed){check(wire.at("evidence").at("dungeon_building_effect").at("mapping").at("schema")==3,"Populated nested Building mapping not current schema3");
                check(decoded->evidence.before==value->evidence.before&&decoded->evidence.after==value->evidence.after,"Complete before/after Ledger flattened");}
            for(const auto schema:{1u,2u,10u,35u,36u,38u}){auto wrong=wire;wrong["schema"]=schema;rejects(wrong,"Dungeon Building fields escaped exact schema37 admission");}
            for(const auto& scene:{"", "action:11111111-1111-4111-8111-111111111111/instance/0", "foreign"}){auto wrong=wire;wrong["request"]["dungeon_scene"]=scene;rejects(wrong,"Non-template/foreign Scene admitted");}
            auto wrong=wire;wrong["request"]["dungeon_building"]=false;rejects(wrong,"False owner with Dungeon payload accepted");
            wrong=wire;wrong["request"]["dungeon_building"]=1;rejects(wrong,"Non-Boolean Dungeon owner accepted");
            wrong=wire;wrong["request"]["action"]=66u;rejects(wrong,"Dungeon Building admitted as input66");
            wrong=wire;wrong["request"]["raw_pointer"]=123u;rejects(wrong,"Foreign native pointer field accepted");
            wrong=wire;wrong["evidence"]["foreign"]=0;rejects(wrong,"Unknown evidence field accepted");
            if(!confirmed)continue;
            wrong=wire;wrong["evidence"]["dungeon_building_effect"]=nullptr;rejects(wrong,"Confirmed original missing Scene proof accepted");
            wrong=wire;wrong["evidence"]["dungeon_building_effect"]["mapping"]["operation"]=source_id;rejects(wrong,"Foreign mapping operation accepted");
            wrong=wire;wrong["evidence"]["dungeon_building_effect"]["before"]["random"][0]=-1;rejects(wrong,"Signed RNG adopted");
            wrong=wire;wrong["evidence"]["dungeon_building_effect"]["after"]["references"]="bad";rejects(wrong,"Missing complete active-reference proof accepted");
            wrong=wire;wrong["evidence"]["dungeon_building_effect"]["after"]["authored"]["foreign"]=std::string(64,'f');rejects(wrong,"Foreign authored owner accepted");
            wrong=wire;wrong["evidence"]["dungeon_building_effect"]["inverse"]="bad";rejects(wrong,"Missing original inverse graph proof accepted");
            wrong=wire;wrong["evidence"]["dungeon_building_effect"]["mapping"]["members"][1][2]=2u;rejects(wrong,"Sparse Building mapping child index accepted");
            wrong=wire;wrong["evidence"]["dungeon_building_effect"]["mapping"]["schema"]=2u;rejects(wrong,"Building family smuggled into legacy Mapping2");
            wrong=wire;wrong["evidence"]["undo_confirmed"]=false;rejects(wrong,"Original Undo confirmation bypassed");
            wrong=wire;wrong["evidence"]["retirement_confirmed"]=false;rejects(wrong,"Unretired original effect broadcast as confirmed");
            if(action==Action::UndoBuilding||action==Action::RedoBuilding){wrong=wire;wrong["evidence"]["target_retired"]=value->evidence.object_after.alive;rejects(wrong,"History target retirement disagrees with exact restored/removed object lifetime");}
            wrong=wire;wrong["evidence"]["object_after"]["type"]=world?"DungeonPortal":"DungeonExterior";rejects(wrong,"Opposite Scene-family effect accepted");
            if(world){wrong=wire;const auto key=value->request.canonical_record_key;
                if(value->evidence.object_after.alive)wrong["evidence"]["dungeon_building_effect"]["after"]["authored"].erase(key);
                else wrong["evidence"]["dungeon_building_effect"]["after"]["authored"][key]=std::string(64,'a');
                rejects(wrong,"World Exterior alive state and authored membership diverged");
            }else{wrong=wire;wrong["evidence"]["dungeon_building_effect"]["before"]["authored"].clear();rejects(wrong,"Template omitted current exterior owner");
                wrong=wire;const auto foreign=std::string("action:")+action_id+"/template";
                wrong["evidence"]["dungeon_building_effect"]["mapping"]["scenes"][0][0]=foreign;
                for(auto& row:wrong["evidence"]["dungeon_building_effect"]["mapping"]["members"])row[0]=foreign;
                rejects(wrong,"Template action carried only a foreign current scene");}
        }
    }
    auto ordinary=std::make_unique<Message>(message(Action::PlaceBuilding,false,true));ordinary->request.dungeon_building=false;ordinary->request.dungeon_scene.clear();ordinary->evidence.dungeon_building_effect.reset();
    check(encode(*ordinary,bytes,error)&&nlohmann::json::parse(bytes).at("schema")==2,"Existing ordinary Building codec changed");
    check(decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==*ordinary,"Existing Building message roundtrip changed");
    ordinary->request.dungeon_scene="world";check(!encode(*ordinary,bytes,error),"New Scene escaped false-owner rejection");
    auto malformed=std::make_unique<Message>(message(Action::PlaceBuilding,false,true));malformed->evidence.after.cash=malformed->evidence.before.cash;
    malformed->evidence.after.record_cash=malformed->evidence.before.record_cash;
    check(encode(*malformed,bytes,error),"Dungeon protocol invented a required purchase delta");
    malformed->request.building.type="SmallInn";malformed->evidence.object_after.type="SmallInn";
    check(encode(*malformed,bytes,error),"Template Scene restricted to Portals instead of ordinary Buildings");
    malformed->request.dungeon_scene="world";check(!encode(*malformed,bytes,error),"Ordinary contained Building admitted as world Exterior");
    auto large=std::make_unique<Message>(message(Action::PlaceBuilding,false,true));for(unsigned i=0;i<1000;++i){auto key=std::string("action:")+source_id;
        key.replace(7,8,std::string(8,'a'));const auto text=std::to_string(i);key.replace(7+8-text.size(),text.size(),text);
        large->evidence.dungeon_building_effect->before.authored[key]=std::string(64,'a');}
    check(!encode(*large,bytes,error)&&error.find("60000")!=std::string::npos,"Oversized complete authored map bypassed existing packet cap");
    std::cout<<checks<<" actual Dungeon Building schema37/full proof/legacy Building checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
