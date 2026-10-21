namespace gizmo_control_native_fixture {
namespace c=mt2mp::game::dungeon_gizmo_control;namespace n=mt2mp::game::dungeon_gizmo_control_native;
namespace dn=mt2mp::game::duplicate_native;namespace a=mt2mp::action_session;
struct Port{
    std::vector<dn::Handle> sources{{10,0x100000001},{11,0x200000001}};
    std::vector<std::string> trace;std::string failure;bool admitted=true;
    bool step(const char* label,std::string& error){trace.emplace_back(label);if(failure==label){error=std::string("actual original ")+label+" failure";return false;}return true;}
    bool preflight(std::string& error){if(!admitted){error="actual source owner changed";return false;}return step("preflight",error);}
    bool move(uintptr_t& graph,std::string& error){graph=20;return step("move_collection",error);}
    bool destroy(uintptr_t& graph,std::string& error){graph=20;return step("destroy_collection",error);}
    bool complete(uintptr_t graph,std::string& error){return graph==20&&step("complete_graph",error);}
    bool ready(const a::DuplicateIntent&,const std::vector<dn::Handle>&,int32_t,std::string& error){return step("duplicate_ready",error);}
    bool charge(int32_t,std::string& error){return step("charge",error);}
    bool begin_edit(bool& opened,std::string& error){opened=true;return step("begin",error);}
    bool end_edit(std::string& error){return step("end",error);}
    bool create_graph(uintptr_t& graph,std::string& error){graph=20;return step("graph",error);}
    bool cash_inverse(uintptr_t,int32_t,const std::array<uint32_t,3>&,std::string& error){return step("cash",error);}
    bool duplicate(const dn::Handle& source,const a::DuplicateTarget&,dn::Handle& result,std::string& error){result={source.object+100,source.uid+100};return step("duplicate",error);}
    bool destroy_inverse(uintptr_t,const dn::Handle&,std::string& error){return step("inverse",error);}
    bool select_created(const std::vector<dn::Handle>&,std::string& error){return step("select",error);}
    bool release_points(const a::DuplicateIntent&,const std::vector<dn::Handle>&,std::string& error){return step("release",error);}
    bool commit_graph(uintptr_t,const std::vector<dn::Handle>&,std::string& error){return step("commit",error);}
    bool readback(const a::DuplicateIntent&,const std::vector<dn::Handle>&,const dn::Receipt&,std::string& error){return step("readback",error);}
    bool restore_selection(std::string& error){return step("restore",error);}
};
template<class Check>void run(Check& check){std::string error;
    for(bool scene_scenery:{false,true})for(const auto command:{c::Command::Move,c::Command::Duplicate,c::Command::Destroy}){
        c::Intent intent;intent.command=command;
        for(unsigned i=0;i<2;++i){c::Row row;row.key=mt2mp::game::dungeon_gizmo_identity::birth("11111111-1111-4111-8111-111111111111",i);
            row.before.rotation={0,0,0,1};row.before.scale={1,1,1};row.after=c::pose(row.before);if(command==c::Command::Move)row.after[4]=std::bit_cast<uint32_t>(1.0f);if(scene_scenery&&i==1){row.scenery=c::Scenery{"Trees","oak",c::pose(row.before)};row.before={};
                row.key=mt2mp::game::scenery_scene_key::record(std::string(64,'a'),67,3,-1,0);}intent.rows.push_back(row);}
        Port port;n::Receipt receipt;check(n::execute(port,intent,receipt,error)&&receipt.confirmed&&!receipt.unknown,"actual Gizmo control original engine confirms complete graph");
        const auto attempts=port.trace.size();check(!n::execute(port,intent,receipt,error)&&port.trace.size()==attempts,"Gizmo control cannot replay confirmed effects");
        if(command==c::Command::Move||command==c::Command::Destroy)check(port.trace==std::vector<std::string>{"preflight",command==c::Command::Move?"move_collection":"destroy_collection","complete_graph"},"Gizmo collection executes one original shared transaction");
        else check(std::count(port.trace.begin(),port.trace.end(),"cash")==2&&receipt.duplicate.created.size()==2,"free Gizmo duplicate keeps both original Cash inverses and full returns");
        port={};port.admitted=false;receipt={};check(!n::execute(port,intent,receipt,error)&&!receipt.attempted&&port.trace.empty(),"foreign control owner is rejected before an original mutation");
        port={};port.failure="complete_graph";receipt={};check(!n::execute(port,intent,receipt,error)&&receipt.unknown&&!receipt.confirmed&&error=="actual original complete_graph failure","committed mismatching control is Unknown with original cause");
        const auto calls=port.trace.size();check(!n::execute(port,intent,receipt,error)&&port.trace.size()==calls,"Unknown control cannot replay");
    }
}
}
