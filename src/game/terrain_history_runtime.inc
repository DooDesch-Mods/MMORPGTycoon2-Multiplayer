namespace {
bool history_contour_code(region::Runtime& r,std::string& e){
    struct Pin {uintptr_t rva;size_t size;const char* sha;};
    static constexpr Pin pins[]{
        {0x55e050,256,"d3a9e3c8ac2c5529bca4be455419dd8fd3ceb529e2269bd4080894a4b043b801"},
        {0x55e150,112,"985f204b2c9b1c2bf229d3720f853b0bb1e7ecc18c824fc945716f9c019d0880"},
        {0x362450,192,"95573ab3ac6fe5574cb62b539417a57c23dc178e5a432d2b3f809e73f39d5c9e"},
        {0x362510,304,"f44b8fd7db59df03ea2c35cf4148fffab006a964cd77f0276dc89af16d119761"},
        {0xf6d80,1056,"f4583a4ade8c3eba4a25a029cdddb6704caeeaa3330f40ad89c293961a14e037"},
    };
    for(const auto& pin:pins){std::vector<uint8_t> bytes(pin.size);std::string actual;
        if(!r.memory.copy(r.image+pin.rva,bytes.data(),bytes.size(),e))return false;
        if(!region::sha256(std::string(reinterpret_cast<const char*>(bytes.data()),bytes.size()),actual,e))return false;
        if(actual!=pin.sha)return fail(e,"Terrain history contour original body differs RVA="+std::to_string(pin.rva));
    }
    uint32_t scale=0;
    if(!region::at(r,r.image,0x147f1b0,scale,e))return false;
    return scale==0x3f800000||fail(e,"Terrain history original Simplify scale differs actual="+std::to_string(scale)+" required=1065353216");
}

template<class Money>json history_ledger_diagnostic(const Money& value){
    return {{"cash",value.cash},{"flat_day",value.flat_day},{"record_present",value.record_present},
        {"record_day",value.record_day},{"record_cash",value.record_cash},{"record_income",value.record_income},
        {"record_expense",value.record_expense},{"line_counts",value.line_counts},{"line_income",value.line_income}};
}
template<class Terrain>json history_terrain_diagnostic(const Terrain& value){
    return {{"global_random",value.global_random},{"release_edits",value.release_edits},
        {"contours_sha256",value.contours_sha256},{"bugs_sha256",value.bugs_sha256},{"bug_count",value.bug_count},
        {"heights_sha256",value.heights_sha256},{"height_cell",value.height_cell},{"height_bits",value.height_bits}};
}
json history_uid_diagnostic(const std::vector<uint64_t>& expected,const std::vector<uint64_t>& actual){
    json missing=json::array(),extra=json::array();size_t removed=0,added=0,i=0,j=0;
    while(i<expected.size()||j<actual.size()){
        if(j==actual.size()||(i<expected.size()&&expected[i]<actual[j])){
            if(missing.size()<16)missing.push_back(expected[i]);++removed;++i;
        }else if(i==expected.size()||actual[j]<expected[i]){
            if(extra.size()<16)extra.push_back(actual[j]);++added;++j;
        }else{++i;++j;}
    }
    return {{"equal",expected==actual},{"expected_count",expected.size()},{"actual_count",actual.size()},
        {"missing_count",removed},{"extra_count",added},{"missing_uids",missing},{"extra_uids",extra},
        {"truncated",removed>missing.size()||added>extra.size()}};
}
std::string history_failure_diagnostic(const std::string& why,const json& settlement){
    return settlement.is_null()?why:why+" settlement_receipt="+settlement.dump();
}
namespace scenery_history=scenery_make_history;
struct HistoryMemory {
    region::Runtime& owner;
    bool read(uintptr_t address,void* out,size_t bytes,std::string& e){return owner.memory.copy(address,out,bytes,e);}
};
struct TerrainHistoryOperation {
    action::Binding binding;action::Request request;action::Evidence evidence;
    uintptr_t inverse=0;bool attempted=false,confirmed=false,retired=false,ordered=false;
    std::vector<std::string> keys;std::string cause;json height_receipt,contour_receipt;
};
std::map<std::string,TerrainHistoryOperation> terrain_history_operations;
uint32_t history_find_raw(uintptr_t image,uint64_t uid,uintptr_t& object){
    __try{object=reinterpret_cast<uintptr_t(*)(uint64_t)>(image+0x47ce20)(uid);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool history_find(region::Runtime& r,uint64_t uid,uintptr_t& object,std::string& e){
    const auto code=history_find_raw(r.image,uid,object);
    return !code||fail(e,"Terrain history original FindUID SEH="+std::to_string(code));
}
uint32_t history_native_height(uintptr_t image,uintptr_t root,const std::array<uint32_t,3>& position,float& height){
    __try{height=reinterpret_cast<float(*)(void*,const void*)>(image+0x508390)(reinterpret_cast<void*>(root),position.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool history_transform_code(region::Runtime& r,std::string& e){
    for(const auto& code:terrain_transform_native::codes){std::vector<uint8_t> bytes(code.size);
        if(!r.memory.copy(r.image+code.rva,bytes.data(),bytes.size(),e))return false;
        if(std::memcmp(bytes.data(),code.bytes,code.size))return fail(e,"Terrain history transform native body/constant differs RVA="+std::to_string(code.rva));
    }
    return true;
}
uint32_t history_transform_matrix(uintptr_t image,const std::array<uint8_t,0x6c>& input,std::array<uint32_t,16>& matrix){
    __try{
        alignas(16) auto copy=input;copy[0x28]=1;
        const auto* result=reinterpret_cast<const void*(*)(void*)>(image+0x9ff8d0)(copy.data());
        if(result!=copy.data()+0x2c||copy[0x28]||std::memcmp(copy.data(),input.data(),0x28))return 0xe0010001;
        std::memcpy(matrix.data(),result,sizeof matrix);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool history_region_content(region::Runtime& r,const region::LocalView& after,const action::RegionState& desired,
                            const terrain_scenery_diagnostic::Snapshot& expected,const terrain_scenery_diagnostic::Snapshot& current,bool ordered,const json& settlement,json& receipt,std::string& e){
    receipt={{"confirmed",false},{"checked_rows",0},{"derived_rows",0}};
    if(expected.total!=desired.scenery_count||after.scenery_diagnostic.total!=after.state.scenery_count)
        return fail(e,"Terrain history Scenery capture differs from complete Region count");
    auto region_expected=desired;region_expected.scenery_sha256=after.state.scenery_sha256;
    if(!action::same_region_content(after.state,region_expected))return fail(e,"Terrain history non-Scenery Region fields differ");
    auto ground=[&](uint64_t uid,const terrain_scenery_diagnostic::Row& row,float& value,std::string& why){
        if(!ordered||!settlement.value("confirmed",false)||!settlement.value("publications",uint64_t(0)))
            return fail(why,"Terrain history derived height lacks exact original publication");
        uintptr_t object=0;scenery_history::Saved saved;HistoryMemory memory{r};
        auto find=[&](uint64_t key,uintptr_t& found,std::string& error){return history_find(r,key,found,error);};
        if(!find(uid,object,why)||!scenery_history::live(memory,r.image,object,uid,after.world.root,after.region,find,saved,why))return false;
        std::array<uint32_t,3> position{};std::memcpy(position.data(),saved.transform.data()+0x10,sizeof position);
        if(position!=std::array<uint32_t,3>{row.pose[0],row.pose[1],row.pose[2]}||saved.height!=row.height[5])
            return fail(why,"Terrain history Scenery changed before native height read");
        const auto code=history_native_height(r.image,after.world.root,position,value);
        return !code||fail(why,"Terrain history original GetHeightAt SEH="+std::to_string(code));
    };
    const bool result=terrain_history_height::compare(expected,after.scenery_diagnostic,ground,region::sha256,receipt,e,ordered?&current:nullptr,ordered?&settlement:nullptr);
    receipt["original_sha256"]=desired.scenery_sha256;receipt["actual_sha256"]=after.state.scenery_sha256;
    return result;
}

bool history_capture(region::Runtime& r,const region::LocalView& before,const json& bindings,TerrainOperation& op,std::string& e){
    HistoryMemory memory{r};
    if(!terrain_history_height::complete(before.scenery_diagnostic))return fail(e,"Terrain history before Scenery capture incomplete: "+before.scenery_diagnostic.first_error);
    op.history_before_uids=before.scenery_uids;op.history_before_diagnostic=before.scenery_diagnostic;
    auto find=[&](uint64_t uid,uintptr_t& value,std::string& why){return history_find(r,uid,value,why);};
    for(auto uid:before.scenery_uids){uintptr_t object=0;scenery_history::Saved saved;
        if(!find(uid,object,e)||!scenery_history::live(memory,r.image,object,uid,before.world.root,before.region,find,saved,e))return false;
        op.history_scenery.emplace(uid,std::move(saved));}
    for(const auto& row:bindings.at("bindings")){
        const auto uid=row.at("local_uid").get<uint64_t>();
        if(!op.history_scenery.contains(uid)||row.at("revoked").get<bool>())continue;
        if(row.at("unknown").get<bool>()||!op.history_keys.emplace(uid,row.at("record_key").get<std::string>()).second)
            return fail(e,"Terrain history canonical before binding is Unknown or ambiguous");
    }
    return true;
}
json history_graph_counts(const undo::GraphWitness& graph){
    return {{"nodes",graph.nodes},{"operations",graph.operations},{"pending_delete",graph.pending_delete},{"locked",graph.locked}};
}
json history_graph_failure(region::Runtime& r,uintptr_t graph,const undo::GraphWitness& witness,const TerrainOperation& source,bool making){
    auto result=history_graph_counts(witness);
    result["required"]={{"locked",true},{"pending_delete",0},{"operations",source.removed_scenery_uids.size()+1}};
    result["finish_graph"]=source.graph_at_finish;result["same_source_graph"]=graph==source.graph;
    result["making"]=making;result["removed_scenery_count"]=source.removed_scenery_uids.size();
    result["unlocked"]=!witness.locked;result["pending_delete_nonzero"]=witness.pending_delete!=0;
    result["operation_count_differs"]=witness.operations!=source.removed_scenery_uids.size()+1;
    auto types=json::array();uintptr_t array=0;std::string detail;
    if(!region::at(r,graph,0x20,array,detail))result["type_read_error"]=detail;
    else for(int32_t i=0;i<witness.operations&&i<16;++i){uintptr_t node=0,ap=0;
        if(!region::at(r,array,size_t(i)*8,node,detail)||!region::at(r,node,0,ap,detail)){
            result["type_read_error"]=detail;result["type_read_failed_index"]=i;break;
        }
        types.push_back({{"index",i},{"in_image",ap>=r.image&&ap-r.image<0x2000000},
            {"addresspoint_rva",ap>=r.image&&ap-r.image<0x2000000?ap-r.image:0}});
    }
    result["types_omitted"]=size_t(witness.operations)>types.size()?size_t(witness.operations)-types.size():0;
    result["operation_types"]=std::move(types);return result;
}
bool history_graph(region::Runtime& r,uintptr_t graph,uintptr_t receiver,const std::string& contours,
                   const TerrainOperation& source,bool making,std::string& e,
                   const std::map<uint64_t,scenery_history::Saved>* current_make=nullptr,
                   std::map<uint64_t,scenery_history::Saved>* retained_make=nullptr){
    HistoryMemory memory{r};undo::GraphWitness witness;uintptr_t array=0;size_t painted=0;std::set<uint64_t> seen;
    if(!undo::read_graph(r.memory,r.image,graph,witness,e)||!region::at(r,graph,0x20,array,e))return false;
    if(!witness.locked||witness.pending_delete||witness.operations!=source.removed_scenery_uids.size()+1)
        return fail(e,"Terrain history complete original graph count/lifetime differs graph_boundary="+history_graph_failure(r,graph,witness,source,making).dump());
    for(int32_t i=0;i<witness.operations;++i){uintptr_t node=0,ap=0;
        if(!region::at(r,array,size_t(i)*8,node,e)||!region::at(r,node,0,ap,e))return false;
        if(ap==r.image+0x1723330){uintptr_t method=0;if(!region::at(r,ap,0x10,method,e))return false;
            if(++painted!=1||method!=r.image+0x55e150)return fail(e,"Terrain history original contour inverse method/count differs");continue;}
        uint64_t uid=0;if(!region::at(r,node,0xb8,uid,e))return false;
        const auto saved=source.history_scenery.find(uid);
        if(saved==source.history_scenery.end()||!seen.insert(uid).second)
            return fail(e,"Terrain history original graph changes unrelated or repeated Scenery UID");
        if(making){
            if(current_make){
                const auto current=current_make->find(uid);const auto row=source.history_current_diagnostic.rows.find(uid);
                if(!retained_make||current==current_make->end()||row==source.history_current_diagnostic.rows.end())
                    return fail(e,"Terrain history new Make lacks exact pre-Redo UID/row");
                scenery_history::Saved stored;auto matrix=[&](const auto& transform,auto& value,std::string& why){
                    const auto code=history_transform_matrix(r.image,transform,value);
                    return !code||fail(why,"Terrain history current Make original GetMatrix SEH="+std::to_string(code));};
                if(!terrain_history_make::capture(memory,r.image,node,current->second,row->second,matrix,stored,e))return false;
                retained_make->emplace(uid,std::move(stored));
            }else{
                const auto retained=source.history_make_scenery.find(uid);
                if(retained==source.history_make_scenery.end())return fail(e,"Terrain history exact retained Make payload missing UID="+std::to_string(uid));
                if(!scenery_history::make(memory,r.image,node,retained->second,e))return false;
            }
        }else if(!scenery_history::destroy(memory,r.image,node,uid,e))return false;
    }
    return (painted==1&&seen.size()==source.removed_scenery_uids.size()&&inverse(r,graph,receiver,contours,e))||
        fail(e,e.empty()?"Terrain history complete inverse UID set differs":e);
}
bool history_source(region::Runtime& r,const region::LocalView& before,const region::LocalView& after,TerrainOperation& op,std::string& e){
    if(!terrain_history_height::complete(after.scenery_diagnostic))return fail(e,"Terrain history after Scenery capture incomplete: "+after.scenery_diagnostic.first_error);
    op.history_after_uids=after.scenery_uids;op.history_after_diagnostic=after.scenery_diagnostic;
    op.history_current_region=after.state;op.history_current_diagnostic=after.scenery_diagnostic;
    const std::set<uint64_t> removed(op.removed_scenery_uids.begin(),op.removed_scenery_uids.end());
    std::erase_if(op.history_scenery,[&](const auto& row){return !removed.contains(row.first);});
    std::erase_if(op.history_keys,[&](const auto& row){return !removed.contains(row.first);});
    op.history_make_scenery=op.history_scenery;
    op.history_contours.initialize(op.evidence.terrain_before.contours_sha256,op.evidence.terrain_after.contours_sha256);
    return history_graph(r,op.graph,before.region,op.evidence.terrain_before.contours_sha256,op,true,e);
}
struct HistorySelection {std::vector<std::pair<uintptr_t,uint64_t>> rows;std::array<uint8_t,9> filter{};uintptr_t camera=0;};
bool history_selection(region::Runtime& r,const region::LocalView& view,HistorySelection& out,std::string& e){
    HistoryMemory memory{r};std::vector<uintptr_t> raw,filtered;uintptr_t manager=0;
    if(!group_reader::selection(memory,r.image,view.world.selection,raw,filtered,e)||
       !region::at(r,view.world.selection,0xc8,out.filter,e)||!region::at(r,r.image,0x13df440,manager,e)||!region::at(r,manager,0x60,out.camera,e))return false;
    if(std::any_of(out.filter.begin(),out.filter.end(),[](auto n){return n>1;}))return fail(e,"Terrain history local Selection filter invalid");
    for(auto pointer:raw){uint64_t uid=0;uintptr_t found=0;
        if(!region::at(r,pointer,0x18,uid,e)||!uid||!history_find(r,uid,found,e)||found!=pointer)return fail(e,"Terrain history local selected UID differs: "+e);
        out.rows.emplace_back(pointer,uid);}
    return true;
}
uint32_t history_filter(uintptr_t image,uintptr_t selection,const std::array<uint8_t,9>& values){
    __try{reinterpret_cast<void(*)(void*,const void*)>(image+0x2d58f0)(reinterpret_cast<void*>(selection),values.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool history_restore_selection(region::Runtime& r,const region::LocalView& view,const HistorySelection& before,const TerrainOperation& source,std::string& e){
    std::vector<uintptr_t> desired;
    for(const auto& [pointer,uid]:before.rows){uintptr_t found=0;if(!history_find(r,uid,found,e))return false;
        if(found==pointer)desired.push_back(found);
        else if(found||!source.history_scenery.contains(uid))return fail(e,"Terrain history replaced unrelated local selected UID");}
    const duplicate_calls::Array array{r.image+0x1748c50,desired.data(),int32_t(desired.size()),int32_t(desired.size())};
    const auto selected=duplicate_calls::select(r.image,view.world.selection,array);
    const auto filter=history_filter(r.image,view.world.selection,before.filter);
    if(selected||filter)return fail(e,"Terrain history Selection restore failed select_seh="+std::to_string(selected)+" filter_seh="+std::to_string(filter));
    HistorySelection after;if(!history_selection(r,view,after,e))return false;
    std::vector<uintptr_t> actual;for(const auto& row:after.rows)actual.push_back(row.first);
    return (desired==actual&&before.filter==after.filter&&before.camera==after.camera)||fail(e,"Terrain history changed local Selection/filter/camera");
}
}

action::Outcome perform_history(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=runtime(e);uint64_t revision=0;bool ordered=false;
    if(!command_scope(binding,request,authority,ordered,e))return action::Outcome::RejectedNoEffect;
    if(!r||!terrain.ready||request.snapshot_uuid!=net::current_snapshot_id()||(!ordered&&bool(authority)==Config::instance().is_host)||
       request.canonical_record_key!="terrain:"+request.target_action_uuid||
       (request.action!=action::Action::UndoBuilding&&request.action!=action::Action::RedoBuilding)||
       !ui_move_runtime::contract::pending(typed_action_runtime::status(),request,revision,e)){
        if(e.empty())e="Terrain history owner/direction/authority differs";return action::Outcome::RejectedNoEffect;}
    if(terrain_history_operations.contains(request.action_uuid)){e="Terrain history already attempted; no replay";return action::Outcome::Unknown;}
    const auto found=terrain.operations.find(request.target_action_uuid);
    if(found==terrain.operations.end()||!found->second.confirmed||!found->second.retired||found->second.ordered!=ordered||found->second.request.terrain.height||
       found->second.binding!=binding||found->second.request.snapshot_uuid!=request.snapshot_uuid||!found->second.graph){
        e="Terrain history lacks current confirmed retired original stroke";return action::Outcome::RejectedNoEffect;}
    auto& source=found->second;const bool undoing=request.action==action::Action::UndoBuilding;
    if(!history_transform_code(*r,e)||!history_contour_code(*r,e))return action::Outcome::RejectedNoEffect;
    region::LocalView before;action::TerrainState state;action::Ledger money;uintptr_t global=0,graph=0;HistorySelection selection;
    const auto& desired_region=undoing?source.evidence.region_before:source.evidence.region_after;
    const auto& expected_region=source.history_current_region;
    const auto& desired_terrain=undoing?source.evidence.terrain_before:source.evidence.terrain_after;
    const auto& expected_contours=source.history_contours.current;
    const auto stored_target=source.history_contours.target(undoing);
    if(!region::read(*r,source.request.terrain.id,0,before,e,true,SIZE_MAX)||!region::bound(before,binding,request.snapshot_uuid,e)||
       !terrain_state(*r,before,state,global,e)||!region::ledger(money,e)||!native_code(*r,true,e)||
       !building_history_slice::graph_for(request.action,request.target_action_uuid,graph,e)||
       !history_graph(*r,graph,before.region,stored_target,source,undoing,e)||!history_selection(*r,before,selection,e))return action::Outcome::RejectedNoEffect;
    if(!action::same_region_content(before.state,expected_region)||state.contours_sha256!=expected_contours||
       before.scenery_uids!=(undoing?source.history_after_uids:source.history_before_uids)){
        const auto& expected_uids=undoing?source.history_after_uids:source.history_before_uids;
        const json mismatch={{"scenery_field_difference",terrain_scenery_diagnostic::difference(source.history_current_diagnostic,before.scenery_diagnostic)},
            {"region_actual",region::state_json(before.state)},
            {"region_expected",region::state_json(expected_region)},
            {"contours_actual",state.contours_sha256},{"contours_expected",expected_contours},
            {"scenery_uids_equal",before.scenery_uids==expected_uids},
            {"scenery_uid_count_actual",before.scenery_uids.size()},{"scenery_uid_count_expected",expected_uids.size()}};
        e="Terrain history current full Region/Scenery/contours differ from original direction: "+mismatch.dump();
        return action::Outcome::RejectedNoEffect;}
    if(authority&&(money!=authority->before||!action::same_region_content(before.state,authority->region_before)||!action::same_terrain_content(state,authority->terrain_before))){
        e="Terrain history Client complete current state differs from Host";return action::Outcome::RejectedNoEffect;}
    for(const auto& [uid,key]:source.history_keys){
        if(undoing){if(!remove_diagnostics::history_target_absent(request.snapshot_uuid,key,uid,e))return action::Outcome::RejectedNoEffect;}
        else{remove_diagnostics::LocalTarget target;if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,key,target,e))return action::Outcome::RejectedNoEffect;
            if(target.uid!=uid){e="Terrain history current canonical Scenery full UID differs";return action::Outcome::RejectedNoEffect;}}
    }
    if(undoing)for(const auto& [uid,saved]:source.history_scenery){
        (void)uid;std::array<uint32_t,3> position{};std::memcpy(position.data(),saved.transform.data()+0x10,sizeof position);uintptr_t resolved=0;
        const auto code=duplicate_calls::region(r->image,saved.root,true,position,resolved);
        if(code||resolved!=before.region){e="Terrain history original Make RegionNear differs SEH="+std::to_string(code);return action::Outcome::RejectedNoEffect;}
    }
    std::map<uint64_t,scenery_history::Saved> current_make,retained_make;
    if(!undoing){
        HistoryMemory memory{*r};auto find=[&](uint64_t uid,uintptr_t& object,std::string& why){return history_find(*r,uid,object,why);};
        for(const auto& [uid,original]:source.history_scenery){
            uintptr_t object=0;scenery_history::Saved current;
            if(!find(uid,object,e)||!scenery_history::live(memory,r->image,object,uid,before.world.root,before.region,find,current,e))return action::Outcome::RejectedNoEffect;
            const auto row=source.history_current_diagnostic.rows.find(uid);
            if(row==source.history_current_diagnostic.rows.end()||!terrain_history_payload::row_matches(current,row->second)||
               current.root!=original.root||current.region!=original.region||current.type!=original.type||
               current.height!=original.height||current.automatic!=original.automatic){
                e="Terrain history pre-Redo payload differs from approved current row UID="+std::to_string(uid)+
                    " actual="+terrain_history_payload::fields(current).dump();return action::Outcome::RejectedNoEffect;}
            current_make.emplace(uid,std::move(current));
        }
    }
    auto& op=terrain_history_operations.try_emplace(request.action_uuid).first->second;
    op.binding=binding;op.request=request;op.ordered=ordered;op.evidence.before=money;op.evidence.region_before=authority?authority->region_before:before.state;
    op.evidence.terrain_before=authority?authority->terrain_before:state;
    json settlement;
    auto unknown=[&](const std::string& why){e=op.cause=history_failure_diagnostic(why,settlement);return action::Outcome::Unknown;};
    op.attempted=true;if(authority&&!native_error(seed(before.random,global,*authority),"history authority RNG seed",e))return unknown(e);
    const auto result=building_history_slice::perform(request.action,request.target_action_uuid,e);
    if(result!=action::Outcome::Confirmed)return unknown(e);
    if(!history_restore_selection(*r,before,selection,source,e))return unknown(e);
    if(!terrain_settlement::run(*r,request.action_uuid,ordered,settlement,e))return unknown(e);
    const auto opposite=undoing?action::Action::RedoBuilding:action::Action::UndoBuilding;
    region::LocalView after;uintptr_t after_global=0;
    if(!building_history_slice::graph_for(opposite,request.target_action_uuid,op.inverse,e)||
       !history_graph(*r,op.inverse,before.region,state.contours_sha256,source,!undoing,e,
           undoing?nullptr:&current_make,undoing?nullptr:&retained_make)||
       !region::read(*r,source.request.terrain.id,before.region,after,e,true,SIZE_MAX)||!region::bound(after,binding,request.snapshot_uuid,e)||
       !terrain_state(*r,after,op.evidence.terrain_after,after_global,e)||!region::ledger(op.evidence.after,e))return unknown(e);
    if(!history_contour_code(*r,e))return unknown(e);
    op.contour_receipt={{"confirmed",true},{"original_simplify",true},{"before_sha256",state.contours_sha256},
        {"stored_target_sha256",stored_target},{"original_target_sha256",desired_terrain.contours_sha256},
        {"actual_sha256",op.evidence.terrain_after.contours_sha256},{"simplify_scale_bits",uint32_t(0x3f800000)}};
    op.evidence.region_after=after.state;op.evidence.undo_confirmed=true;
    json height_receipt;
    const bool region_content=history_region_content(*r,after,desired_region,
        undoing?source.history_before_diagnostic:source.history_after_diagnostic,before.scenery_diagnostic,ordered,settlement,height_receipt,e);
    op.height_receipt=height_receipt;
    if(global!=after_global||!region_content||
       after.scenery_uids!=(undoing?source.history_before_uids:source.history_after_uids)||
       op.evidence.terrain_after.release_edits!=state.release_edits||op.evidence.after!=money){
        const auto& desired_uids=undoing?source.history_before_uids:source.history_after_uids;
        const auto& desired_diagnostic=undoing?source.history_before_diagnostic:source.history_after_diagnostic;
        const json mismatch={
            {"schema_version",1},{"action_uuid",request.action_uuid},{"target_action_uuid",request.target_action_uuid},{"undoing",undoing},
            {"derived_height",height_receipt},{"derived_height_cause",e},{"contour_original",op.contour_receipt},
            {"failed_guards",{{"global_receiver",global!=after_global},
                {"region_content",!region_content},
                {"scenery_uids",after.scenery_uids!=desired_uids},
                {"release_edits",op.evidence.terrain_after.release_edits!=state.release_edits},{"ledger",op.evidence.after!=money}}},
            {"region_before",region::state_json(before.state)},{"region_expected",region::state_json(desired_region)},
            {"region_actual",region::state_json(after.state)},
            {"terrain_before",history_terrain_diagnostic(state)},{"terrain_original_target",history_terrain_diagnostic(desired_terrain)},
            {"terrain_actual",history_terrain_diagnostic(op.evidence.terrain_after)},
            {"release_expected",state.release_edits},{"ledger_expected",history_ledger_diagnostic(money)},
            {"ledger_actual",history_ledger_diagnostic(op.evidence.after)},
            {"scenery_uid_difference",history_uid_diagnostic(desired_uids,after.scenery_uids)},
            {"scenery_expected_to_actual",terrain_scenery_diagnostic::difference(desired_diagnostic,after.scenery_diagnostic)},
            {"scenery_before_to_actual",terrain_scenery_diagnostic::difference(before.scenery_diagnostic,after.scenery_diagnostic)}};
        return unknown("Terrain history original complete consequence differs: "+mismatch.dump());
    }
    auto find=[&](uint64_t uid,uintptr_t& object,std::string& why){return history_find(*r,uid,object,why);};
    HistoryMemory memory{*r};
    terrain_history_payload::Report payload;
    auto matrix=[&](const auto& transform,auto& result,std::string& why){
        const auto code=history_transform_matrix(r->image,transform,result);
        return !code||fail(why,"Terrain history original copied GetMatrix SEH="+std::to_string(code));
    };
    for(const auto& [uid,saved]:source.history_scenery){uintptr_t object=0;if(!find(uid,object,e))return unknown(e);
        if(undoing){scenery_history::Saved actual;if(!scenery_history::live(memory,r->image,object,uid,before.world.root,before.region,find,actual,e))return unknown(e);
            const auto old_row=source.history_before_diagnostic.rows.find(uid),new_row=after.scenery_diagnostic.rows.find(uid);
            if(old_row==source.history_before_diagnostic.rows.end()||new_row==after.scenery_diagnostic.rows.end())return unknown("Terrain restored payload lacks complete approved height row UID="+std::to_string(uid));
            auto expected=saved;expected.object=object;
            payload.add(expected,actual,old_row->second,new_row->second,height_receipt.value("confirmed",false),matrix);
        }else if(object)return unknown("Terrain history original removed Scenery UID still exists");}
    if(payload.failed)return unknown("Terrain history original restored Scenery payload differs: "+payload.json().dump());
    if(undoing)LOG_INFO("[UITerrain] History restored payload action={} receipt={}",request.action_uuid,payload.json().dump());
    if(undoing){for(const auto& [uid,key]:source.history_keys){
        if(!remove_diagnostics::restore_redone_building(request.snapshot_uuid,key,uid,e))return unknown(e);op.keys.push_back(key);}}
    else if(!remove_diagnostics::retire_region_scenery(request.snapshot_uuid,request.action_uuid,source.removed_scenery_uids,op.keys,e))return unknown(e);
    std::sort(op.keys.begin(),op.keys.end());
    if(!region::sha256(json(op.keys).dump(),op.evidence.region_retired_sha256,e))return unknown(e);
    if(authority&&(op.evidence.region_after!=authority->region_after||op.evidence.terrain_after!=authority->terrain_after||
       op.evidence.after!=authority->after||op.evidence.region_retired_sha256!=authority->region_retired_sha256))return unknown("Terrain history Client original result differs from Host");
    source.history_contours.confirmed(undoing,state.contours_sha256,op.evidence.terrain_after.contours_sha256);
    if(!undoing)source.history_make_scenery=std::move(retained_make);
    source.history_current_region=after.state;source.history_current_diagnostic=after.scenery_diagnostic;
    LOG_INFO("[UITerrain] History derived height action={} receipt={}",request.action_uuid,height_receipt.dump());
    op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe_history(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=runtime(e);const auto found=terrain_history_operations.find(request.action_uuid);
    if(!r||found==terrain_history_operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)
        return fail(e,"Terrain history observation lacks exact confirmed operation");
    const auto& op=found->second;
    if(op.ordered&&!ordered_scope(binding,request,e))return false;
    const auto& source=terrain.operations.at(request.target_action_uuid);
    region::LocalView current;action::TerrainState state;action::Ledger money;uintptr_t global=0,graph=0;
    const auto opposite=request.action==action::Action::UndoBuilding?action::Action::RedoBuilding:action::Action::UndoBuilding;
    if(!region::read(*r,source.request.terrain.id,0,current,e)||!region::bound(current,binding,request.snapshot_uuid,e)||
       !terrain_state(*r,current,state,global,e)||!region::ledger(money,e)||
       !building_history_slice::graph_for(opposite,request.target_action_uuid,graph,e))return false;
    if(graph!=op.inverse)return fail(e,"Terrain history current inverse graph changed before ACK");
    if(current.state!=op.evidence.region_after||state!=op.evidence.terrain_after||money!=op.evidence.after)return fail(e,"Terrain history complete result changed before ACK");
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool history_keys(const action::Binding& binding,const action::Request& request,std::vector<std::string>& keys,std::string& e){
    action::Evidence actual;if(!observe_history(binding,request,actual,e))return false;keys=terrain_history_operations.at(request.action_uuid).keys;return true;
}
bool retire_history(const action::Binding& binding,const action::Request& request,std::string& e){
    action::Evidence actual;if(!observe_history(binding,request,actual,e))return false;
    auto& op=terrain_history_operations.at(request.action_uuid);if(op.retired)return fail(e,"Terrain history already retired");op.retired=true;return true;
}
