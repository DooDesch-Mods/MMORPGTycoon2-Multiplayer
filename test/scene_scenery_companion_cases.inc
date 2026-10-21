namespace scene_scenery_companion_fixture {
template<class Check>void run(Check& check){
    namespace c=mt2mp::game::monster_snapshot_companion;
    namespace k=mt2mp::game::scenery_scene_key;
    const std::string sha(64,'d');std::string error,bytes;c::Manifest original,decoded;
    original.native_sha256=sha;original.native_bytes=123;original.scene_scenery.present=true;
    const std::string action="action:12345678-1234-4234-8234-123456789abc";
    for(int instance:{-1,0,7}){const auto record=k::record(sha,67,3,instance,0);
        check(k::saved(record)&&k::valid(record),"Scene Scenery exact saved template/Instance path");
        original.scene_scenery.rows.push_back({record,instance==7?action:record});}
    std::sort(original.scene_scenery.rows.begin(),original.scene_scenery.rows.end(),[](const auto& a,const auto& b){return a.record<b.record;});
    original.scene_scenery.retired.insert(k::record(std::string(64,'c'),67,3,-1,9));
    check(c::encode(original,bytes,error),error.c_str());
    auto document=nlohmann::json::parse(bytes);check(document.at("revision")==6,"Scene Scenery companion uses additive revision6");
    check(c::decode(bytes,sha,123,decoded,error)&&decoded==original,"Complete Scene Scenery aliases/tombstones survive native companion round trip");
    check(!c::decode(bytes,std::string(64,'e'),123,decoded,error),"Scene Scenery companion cannot cross native snapshot SHA");
    check(!c::decode(bytes,sha,124,decoded,error),"Scene Scenery companion cannot cross native snapshot byte receipt");
    const auto reject=[&](const nlohmann::json& value){return !c::decode(value.dump(),sha,123,decoded,error);};
    auto bad=document;bad["scene_scenery"]["rows"].push_back(bad["scene_scenery"]["rows"][0]);
    check(reject(bad),"Scene Scenery duplicate saved slot rejected");
    bad=document;bad["scene_scenery"]["foreign"]=true;check(reject(bad),"Scene Scenery unknown companion field rejected");
    bad=document;bad["revision"]=5;check(reject(bad),"Legacy5 cannot carry Scene Scenery fields");
    bad=document;bad["scene_scenery"]["retired"].push_back(action);check(reject(bad),"Scene Scenery live/retired alias overlap rejected");
    for(const auto& invalid:{sha+":world:dungeon:scenery:region{067}/building[3]/template/scenery[0]",k::record(sha,67,3,-1,0)+"x",k::record(sha,67,3,-1,-1)})
        check(!k::valid(invalid),"Invalid original Scene Scenery record grammar rejected");
    original.scene_scenery.root_present=true;
    const auto root_record=k::root_record(sha,59,8,0),root_key=k::root_record(std::string(64,'a'),59,8,4);
    original.scene_scenery.rows.push_back({root_record,root_key});
    original.scene_scenery.retired.insert(k::root_record(std::string(64,'c'),59,19,8));
    std::sort(original.scene_scenery.rows.begin(),original.scene_scenery.rows.end(),[](const auto& a,const auto& b){return a.record<b.record;});
    check(c::encode(original,bytes,error),error.c_str());document=nlohmann::json::parse(bytes);
    check(document.at("revision")==7&&document.at("scene_scenery").at("root")==true,"complete root Scenery uses explicit revision7");
    check(c::decode(bytes,sha,123,decoded,error)&&decoded==original,"root/dungeon/action aliases and retired keys roundtrip together");
    bad=document;bad["revision"]=6;check(reject(bad),"legacy6 cannot carry root-complete witness");
    bad=document;bad["scene_scenery"].erase("root");check(reject(bad),"revision7 cannot omit root-complete witness");
    bad=document;bad["scene_scenery"]["root"]=false;check(reject(bad),"explicit incomplete root witness rejected");
    bad=document;bad["scene_scenery"]["rows"].push_back({{"record",k::root_record(sha,59,47,0)},{"key",root_key}});check(reject(bad),"native category47 outside original arrays rejected");
    check(!k::saved(sha+":world:scenery:inline:region{059}/sceneryManager/type[8]/scenery[0]"),"root RegionID leading zero rejected");
    auto incomplete=original;incomplete.scene_scenery.root_present=false;check(!c::encode(incomplete,bytes,error),"root saved rows cannot claim legacy scene-only schema");
    original.scene_scenery={};check(c::encode(original,bytes,error)&&c::decode(bytes,sha,123,decoded,error)&&decoded==original,"Existing scenery-free companion remains exact");
}
}
