#include "duplicate_sample.inc"
#include "duplicate_port.inc"
namespace duplicate_edit {
namespace {
struct Pending {action::Binding binding;std::string id;duplicate_reader::Capture source;action::DuplicateIntent intent;Sample before;};
struct Operation {action::Binding binding;action::Request request;Sample before,after;action::Evidence evidence;
    duplicate_native::Receipt native;std::map<uint64_t,std::string> created;std::vector<uintptr_t> cash_ops,destroy_ops;
    std::map<uint64_t,child_npc::Receipt> children;
    bool confirmed=false,retired=false;std::string cause;};
std::optional<Pending> captured;std::map<std::string,Operation> operations;std::set<std::string> local_sources;
uint64_t captures=0,submitted=0,rejections=0;std::string cause;
json last_submission=nullptr,last_source_records=nullptr,last_observation=nullptr;
void retain_source_records(const std::string& id,const std::string& snapshot,const action::Binding& binding,
                           const action::DuplicateIntent& intent,const Sample& sample,const char* phase){
    if(!Config::instance().diagnostic_mailbox_enabled){last_source_records=nullptr;return;}
    json keys=json::array();for(const auto& row:intent.rows)keys.push_back(row.key);
    last_source_records={{"action_uuid",id},{"snapshot_uuid",snapshot},{"native_epoch",binding.native_epoch},
        {"native_generation",binding.native_generation},{"phase",phase},{"selected_keys",std::move(keys)},
        {"source_limit",duplicate_records::diagnostic_sources},{"truncated_sources",sample.selected_records.size()<intent.rows.size()},
        {"records",sample.selected_records}};
}
void retain_submission(const std::string& id,const std::string& snapshot,const action::Binding& binding,const action::DuplicateIntent& intent){
    json keys=json::array(),poses=json::array();for(const auto& row:intent.rows){keys.push_back(row.key);poses.push_back(row.after);}
    last_submission={{"action_uuid",id},{"snapshot_uuid",snapshot},{"native_epoch",binding.native_epoch},
        {"native_generation",binding.native_generation},{"selected_keys",std::move(keys)},{"target_poses",std::move(poses)},
        {"cash_position",intent.cash_position},{"capture_count",captures},{"submission_count",submitted},{"submitted",false}};
}
void reject(const std::string& e){++rejections;if(cause!=e){cause=e;LOG_ERROR("[Duplicate] {}",e);}}
bool same_binding(const Sample& a,const Sample& b){return host::same_world(a.world.origin,b.world.origin)&&
    a.world.root==b.world.root&&a.world.selection==b.world.selection&&a.business==b.business&&a.release==b.release&&a.random==b.random;}
bool cash_position(Runtime& r,uintptr_t behaviour,std::array<uint32_t,3>& out,std::string& e){
    uintptr_t cursor=0,current=0,axis=0;std::array<float,3> point{},up{};float scale=0;
    if(!q::at(r.memory,r.image,0x13df9d0,cursor,e)||!q::typed(r.memory,r.image,cursor,0x1786dd0,e)||
       !q::at(r.memory,cursor,0x190,current,e)||current!=behaviour||!q::at(r.memory,cursor,0x1e4,point,e)||
       !q::at(r.memory,r.image,0x164b0d0,axis,e)||axis!=r.image+0x1c4e8f0||!q::at(r.memory,axis,0,up,e)||
       !q::at(r.memory,r.image,0x14ee978,scale,e)||scale!=20.0f)return q::fail(e,e.empty()?"Duplicate original Cursor/YAxis/offset owner differs":e);
    for(size_t i=0;i<3;++i){const float offset=up[i]*scale;const float value=point[i]+offset;
        if(!std::isfinite(point[i])||!std::isfinite(up[i])||!std::isfinite(value))return q::fail(e,"Duplicate original cash point is nonfinite");out[i]=std::bit_cast<uint32_t>(value);}
    return true;
}
bool capture_read(Runtime& r,uintptr_t behaviour,duplicate_reader::Capture& out,std::string& e){
    auto point=[&](auto& result,std::string& why){return cash_position(r,behaviour,result,why);};
    return duplicate_reader::capture(r.memory,r.image,behaviour,point,out,e);
}
bool source_matches(const action::DuplicateIntent& intent,const Sample& value,std::string& e){
    if(value.state.sources.size()!=intent.rows.size()||!value.state.created.empty())return q::fail(e,"Duplicate current complete source count differs");
    for(size_t i=0;i<intent.rows.size();++i){const auto& required=intent.rows[i];const auto& actual=value.state.sources[i];
        json differences=json::object();
        if(actual.key!=required.key)differences["key"]={{"required",required.key},{"actual",actual.key}};
        std::string admission_sha=actual.record_sha;
        if(required.captured_uptime){const auto proof=value.uptime.find(required.key);
            if(required.family!=action::DuplicateFamily::Building||!mt2mp::game::building_uptime::valid(*required.captured_uptime)||proof==value.uptime.end()||
               proof->second.captured!=required.captured_uptime||proof->second.historical_sha.empty())
                return q::fail(e,"Duplicate captured Building uptime lacks complete historical Record proof key="+required.key);
            admission_sha=proof->second.historical_sha;
        }
        if(admission_sha!=required.record_sha)differences["record_sha"]={{"required",required.record_sha},{"actual",admission_sha}};
        if(actual.family!=required.family)differences["family"]={{"required",uint32_t(required.family)},{"actual",uint32_t(actual.family)}};
        if(actual.region!=required.region)differences["region"]={{"required",required.region},{"actual",actual.region}};
        if(actual.pose!=required.before)differences["pose_bits"]={{"required",required.before},{"actual",actual.pose}};
        if(actual.members!=required.members)differences["members"]={{"required",required.members},{"actual",actual.members}};
        if(!actual.parent.empty())differences["parent"]={{"required",""},{"actual",actual.parent}};
        if(!differences.empty()){
            const auto found=value.handles.find(actual.key);
            const json context={{"snapshot",value.world.origin.uuid},{"native_epoch",value.world.origin.world_epoch},
                {"native_generation",value.world.origin.generation},{"local_uid",found==value.handles.end()?json(nullptr):json(found->second.uid)},
                {"record_admission",required.captured_uptime?"captured_building_uptime":"current_record"},{"current_record_sha",actual.record_sha}};
            return q::fail(e,"Duplicate current original record/owner/pose differs key="+required.key+" context="+context.dump()+" differences="+differences.dump());
        }
        for(const auto& key:required.created_keys)if(value.objects.contains(key))return q::fail(e,"Duplicate new canonical key already exists key="+key);}
    return true;
}
bool pending_source_matches(const action::DuplicateIntent& intent,const Sample& current,const Sample& captured_before,std::string& e){
    if(!source_matches(intent,current,e))return false;
    auto historical=current.state;
    for(size_t i=0;i<intent.rows.size();++i)if(intent.rows[i].captured_uptime)
        historical.sources[i].record_sha=intent.rows[i].record_sha;
    return historical==captured_before.state||q::fail(e,"Duplicate pending complete state changed outside selected Building uptime");
}
}
#include "duplicate_history_runtime.inc"
bool pending(){return bool(captured);}
bool capture_purchase(uintptr_t behaviour,uintptr_t caller,bool& handled,std::string& e){
    handled=false;auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||caller!=r->image+0x5c2a20||GetCurrentThreadId()!=r->thread||!net::session_wire::connected())return true;
    uint8_t duplicate=0;if(!q::at(r->memory,behaviour,0x499,duplicate,e))return false;if(!duplicate)return true;handled=true;
    action::Binding binding;const auto controller=typed_action_runtime::controller_status();
    if(captured||r->pending||selection_move::pending()||group_edit::pending()||district_title::pending()||district_text::pending()||
       prop_text::pending()||boss_phase::pending()||service_membership::pending()||!controller.value("started",false)||
       controller.value("locked",true)||controller.value("pending",true)||
       !typed_action_runtime::network_binding(net::current_snapshot_id(),binding,e)){if(e.empty())e="Duplicate capture requires idle bound Controller";reject(e);return false;}
    if(!duplicate_code::check(r->memory,r->image,e)){reject(e);return false;}
    duplicate_reader::Capture source,repeat;Sample first,before;
    if(!capture_read(*r,behaviour,source,e)||!sample_duplicate(*r,binding,nullptr,{},first,e)){reject(e);return false;}
    action::DuplicateIntent intent;intent.cash_position=source.cash_position;
    for(size_t i=0;i<source.targets.size();++i){const auto& target=source.targets[i];std::string key;
        for(const auto& [candidate,h]:first.handles)if(h==target){key=candidate;break;}
        if(key.empty()){e="Duplicate original source lacks current canonical full UID";reject(e);return false;}
        const auto& row=first.objects.at(key);int32_t cost=0;bool allowed=false;
        if(!row.parent.empty()||!duplicate_code::target(r->memory,r->image,target.object,e)){if(e.empty())e="Duplicate source must be an original top-level selection";reject(e);return false;}
        const auto seh=duplicate_calls::cost(target.object,cost,allowed);
        if(seh||!allowed||cost<0){e="Duplicate original CanDuplicate/Cost rejected SEH="+std::to_string(seh);reject(e);return false;}
        action::DuplicateTarget value;value.key=key;value.record_sha=row.record_sha;value.family=row.family;value.region=row.region;
        value.cost=cost;value.before=row.pose;value.after=source.poses[i];value.members=row.members;
        if(row.family==Family::Building)value.captured_uptime=first.uptime.at(key).actual;
        const auto count=row.family==Family::Group?row.members.size():1;
        for(size_t n=0;n<count;++n){const auto id=uuid(e);if(id.empty()){reject(e);return false;}value.created_keys.push_back("action:"+id);}
        intent.rows.push_back(std::move(value));
    }
    if(!action::valid_duplicate(intent)||!sample_duplicate(*r,binding,&intent,{},before,e)||!capture_read(*r,behaviour,repeat,e)||source!=repeat||
       !same_binding(first,before)||first.objects!=before.objects||first.untouched_scenery!=before.untouched_scenery||first.handles!=before.handles||first.census!=before.census||
       first.money!=before.money||first.undo!=before.undo||first.state.random!=before.state.random||first.state.release!=before.state.release||
       !source_matches(intent,before,e)){if(e.empty())e="Duplicate complete original capture changed";reject(e);return false;}
    const auto id=uuid(e);if(id.empty()){reject(e);return false;}action::Request request;request.action=action::Action::DuplicateSelection;
    request.action_uuid=id;request.snapshot_uuid=net::current_snapshot_id();request.peer=1;request.sequence=1;request.canonical_record_key="duplicate:"+id;request.duplicate=intent;
    if(!action::duplicate_wire_fits(request,before.state,before.money,e)){reject(e);return false;}
    captured=Pending{binding,id,std::move(source),std::move(intent),std::move(before)};++captures;return true;
}
void tick(){
    if(!net::session_wire::connected()){if(captured)reject("Duplicate source cancelled after session retirement");retire_session();return;}
    if(!captured)return;auto pending_value=std::move(*captured);captured.reset();auto* r=published.load(std::memory_order_acquire);std::string e;
    action::Binding binding;Sample current;
    if(!r||!duplicate_reader::cleaned(r->memory,r->image,pending_value.source,e)||
       !typed_action_runtime::network_binding(net::current_snapshot_id(),binding,e)||binding!=pending_value.binding||
       !sample_duplicate(*r,binding,&pending_value.intent,{},current,e)||!same_binding(current,pending_value.before)||
       !pending_source_matches(pending_value.intent,current,pending_value.before,e)||current.money!=pending_value.before.money||current.undo!=pending_value.before.undo||
       current.handles!=pending_value.before.handles){
        reject(e.empty()?"Duplicate source cleanup/current complete owner changed before submit":e);return;}
    // Host submission can execute synchronously. Mark the originating peer
    // before entering Controller so its normal new selection is retained.
    local_sources.insert(pending_value.id);
    retain_source_records(pending_value.id,net::current_snapshot_id(),binding,pending_value.intent,current,"local_submit");
    retain_submission(pending_value.id,net::current_snapshot_id(),binding,pending_value.intent);
    if(!typed_action_runtime::submit_duplicate(pending_value.id,pending_value.intent,e)){reject(e);return;}++submitted;
    last_submission["submitted"]=true;last_submission["submission_count"]=submitted;
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=published.load(std::memory_order_acquire);uint64_t revision=0;
    if(!r||!r->ready||GetCurrentThreadId()!=r->thread||request.action!=action::Action::DuplicateSelection||!action::valid_duplicate(request.duplicate)||
       request.snapshot_uuid!=net::current_snapshot_id()||!contract::pending(typed_action_runtime::controller_status(),request,revision,e)||
       bool(authority)==Config::instance().is_host){if(e.empty())e="Duplicate exact pending action/role unavailable";return action::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){e="Duplicate already attempted; no replay";return action::Outcome::Unknown;}
    if(authority&&!action::valid_duplicate_effect(request,*authority)){e="Duplicate Host complete evidence invalid";return action::Outcome::RejectedNoEffect;}
    Sample before;if(!sample_duplicate(*r,binding,&request.duplicate,{},before,e))return action::Outcome::RejectedNoEffect;
    retain_source_records(request.action_uuid,request.snapshot_uuid,binding,request.duplicate,before,"authority_source");
    if(!source_matches(request.duplicate,before,e))return action::Outcome::RejectedNoEffect;
    if(authority&&(before.state!=authority->duplicate_before||before.money!=authority->before)){e="Duplicate Client complete source/records/RNG/Release/Ledger differ from Host";return action::Outcome::RejectedNoEffect;}
    if(!action::duplicate_wire_fits(request,before.state,before.money,e))return action::Outcome::RejectedNoEffect;
    auto& op=operations.emplace(request.action_uuid,Operation{binding,request}).first->second;op.before=std::move(before);
    op.evidence.before=op.before.money;op.evidence.duplicate_before=op.before.state;
    Port port{*r,binding,request,op.before,{},op.native};port.source_peer=local_sources.contains(request.action_uuid);
    std::vector<Handle> sources;for(const auto& row:request.duplicate.rows)sources.push_back(op.before.handles.at(row.key));
    const bool returned=duplicate_native::execute(port,request.duplicate,sources,op.native,e);
    op.created=port.created;op.children=std::move(port.children);op.cash_ops=port.cash_ops;op.destroy_ops=port.destroy_ops;op.after=std::move(port.after);
    if(!returned){op.cause=e;return op.native.attempted?action::Outcome::Unknown:action::Outcome::RejectedNoEffect;}
    auto unknown=[&](const std::string& why){e=op.cause=why;return action::Outcome::Unknown;};
    op.evidence.after=op.after.money;op.evidence.duplicate_after=op.after.state;
    op.evidence.undo_confirmed=op.native.graph_committed;op.evidence.selection_confirmed=op.native.selection_restored;
    if(!action::valid_duplicate_effect(request,op.evidence)||(authority&&(op.evidence.after!=authority->after||op.evidence.duplicate_after!=authority->duplicate_after)))
        return unknown("Duplicate actual complete original result differs from Host/request");
    for(const auto& [uid,key]:op.created)if(!remove_diagnostics::admit_created_building(request.snapshot_uuid,key,uid,e))return unknown(e);
    op.confirmed=true;return action::Outcome::Confirmed;
}
void retain_observation(const action::Request& request,const Operation& op,const Sample& current,bool sampled,const std::string& source_cause)try{
    if(!Config::instance().diagnostic_mailbox_enabled){last_observation=nullptr;return;}
    json differences=json::object();
    auto changed=[&](const char* name,const auto& expected,const auto& actual){
        if(expected!=actual)differences[name]={{"expected",expected},{"actual",actual}};
    };
    if(sampled){
        if(!same_binding(op.before,current))differences["binding"]={{"same_world_and_owners",false},
            {"expected_origin",{{"uuid",op.before.world.origin.uuid},{"epoch",op.before.world.origin.world_epoch},{"generation",op.before.world.origin.generation}}},
            {"actual_origin",{{"uuid",current.world.origin.uuid},{"epoch",current.world.origin.world_epoch},{"generation",current.world.origin.generation}}},
            {"root_equal",op.before.world.root==current.world.root},{"selection_equal",op.before.world.selection==current.world.selection},
            {"business_equal",op.before.business==current.business},{"release_owner_equal",op.before.release==current.release},
            {"random_owner_equal",op.before.random==current.random}};
        changed("untouched_sha",op.after.state.untouched_sha,current.state.untouched_sha);
        changed("random",op.after.state.random,current.state.random);
        changed("release",op.after.state.release,current.state.release);
        if(op.after.state.history!=current.state.history)differences["history"]={{"equal",false}};
        auto objects=[&](const char* name,const auto& expected,const auto& actual){
            if(expected==actual)return;
            json rows=json::array();size_t differing=0;
            for(size_t i=0;i<std::max(expected.size(),actual.size());++i){
                if(i<expected.size()&&i<actual.size()&&expected[i]==actual[i])continue;
                ++differing;if(rows.size()==duplicate_records::diagnostic_sources)continue;
                json fields=json::object();
                if(i>=expected.size()||i>=actual.size()){
                    fields["presence"]={{"expected",i<expected.size()},{"actual",i<actual.size()}};
                }else{
                    const auto& a=expected[i];const auto& b=actual[i];
                    auto field=[&](const char* label,const auto& left,const auto& right){
                        if(left!=right)fields[label]={{"expected",left},{"actual",right}};};
                    field("key",a.key,b.key);field("parent",a.parent,b.parent);field("record_sha",a.record_sha,b.record_sha);
                    field("family",uint32_t(a.family),uint32_t(b.family));field("region",a.region,b.region);field("pose_bits",a.pose,b.pose);
                    if(a.members!=b.members)fields["members"]={{"expected_count",a.members.size()},{"actual_count",b.members.size()},
                        {"expected_sha",duplicate_records::diagnostic_sha(json(a.members).dump())},
                        {"actual_sha",duplicate_records::diagnostic_sha(json(b.members).dump())}};
                }
                rows.push_back({{"index",i},{"expected_key",i<expected.size()?json(expected[i].key):json(nullptr)},
                    {"actual_key",i<actual.size()?json(actual[i].key):json(nullptr)},{"fields",std::move(fields)}});
            }
            differences[name]={{"expected_count",expected.size()},{"actual_count",actual.size()},
                {"differing_rows",differing},{"truncated",differing>rows.size()},{"rows",std::move(rows)}};
        };
        objects("sources",op.after.state.sources,current.state.sources);objects("created",op.after.state.created,current.state.created);
        if(op.after.money!=current.money){
            const auto ledger=[](const auto& value){return json{{"cash",value.cash},{"flat_day",value.flat_day},
                {"record_present",value.record_present},{"record_day",value.record_day},{"record_cash",value.record_cash},
                {"record_income",value.record_income},{"record_expense",value.record_expense},
                {"line_counts",value.line_counts},{"line_income",value.line_income}};};
            differences["money"]={{"expected",ledger(op.after.money)},{"actual",ledger(current.money)}};
        }
        if(op.after.handles!=current.handles){
            json rows=json::array();size_t differing=0;
            auto handle=[&](const std::string& key,const Handle* expected,const Handle* actual){
                if(expected&&actual&&*expected==*actual)return;
                ++differing;if(rows.size()==duplicate_records::diagnostic_sources)return;
                rows.push_back({{"key",key},{"expected_uid",expected?json(expected->uid):json(nullptr)},
                    {"actual_uid",actual?json(actual->uid):json(nullptr)},
                    {"same_object",expected&&actual&&expected->object==actual->object}});
            };
            for(const auto& [key,value]:op.after.handles){const auto found=current.handles.find(key);
                handle(key,&value,found==current.handles.end()?nullptr:&found->second);}
            for(const auto& [key,value]:current.handles)if(!op.after.handles.contains(key))handle(key,nullptr,&value);
            differences["handles"]={{"expected_count",op.after.handles.size()},{"actual_count",current.handles.size()},
                {"differing_rows",differing},{"truncated",differing>rows.size()},{"rows",std::move(rows)}};
        }
    }
    last_observation={{"action_uuid",request.action_uuid},{"snapshot_uuid",request.snapshot_uuid},
        {"native_epoch",op.binding.native_epoch},{"native_generation",op.binding.native_generation},
        {"sampled",sampled},{"source_cause",source_cause},{"differences",std::move(differences)},
        {"expected_records",op.after.selected_records},{"current_records",sampled?current.selected_records:json(nullptr)},
        {"record_limit_each",duplicate_records::diagnostic_sources},{"record_bytes_each",duplicate_records::diagnostic_bytes}};
}catch(const std::exception& exception){
    last_observation={{"action_uuid",request.action_uuid},{"state","diagnostic_failed"},
        {"cause","Duplicate observation diagnostic generation failed"},{"exception_sha",duplicate_records::diagnostic_sha(exception.what())}};
}

bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);const auto found=operations.find(request.action_uuid);
    if(!r||found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)
        return q::fail(e,"Duplicate observation lacks exact confirmed operation");
    auto& op=found->second;Sample current;
    const bool sampled=sample_duplicate(*r,binding,&request.duplicate,op.created,current,e);
    if(!sampled||!same_binding(op.before,current)||
       current.state!=op.after.state||current.money!=op.after.money||current.handles!=op.after.handles){
        retain_observation(request,op,current,sampled,e);
        op.cause=e.empty()?"Duplicate complete current result changed before ACK":e;
        if(Config::instance().diagnostic_mailbox_enabled&&last_observation.contains("differences")){
            json fields=json::array();for(const auto& [field,value]:last_observation.at("differences").items()){(void)value;fields.push_back(field);}
            op.cause+=" differing_fields="+fields.dump();
        }
        return q::fail(e,op.cause);
    }
    Port port{*r,binding,request,op.before,std::move(current),op.native};port.cash_ops=op.cash_ops;port.destroy_ops=op.destroy_ops;
    if(!port.inverse(e))return false;out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& e){
    action::Evidence value;if(!observe(binding,request,value,e))return false;auto& op=operations.at(request.action_uuid);
    if(op.retired)return q::fail(e,"Duplicate already retired");auto* r=published.load(std::memory_order_acquire);
    if(!r||!remember_history(*r,op,e))return false;op.retired=true;return true;
}
void retire_session(){captured.reset();operations.clear();local_sources.clear();history_sources.clear();history_operations.clear();last_submission=nullptr;last_source_records=nullptr;last_observation=nullptr;}
json status(){json rows=json::object();for(const auto& [id,op]:operations){json created=json::array(),sources=json::array();
        for(const auto& [uid,key]:op.created)created.push_back({{"key",key},{"local_uid",uid}});
        for(const auto& row:op.request.duplicate.rows)sources.push_back({{"key",row.key},{"local_uid",op.before.handles.at(row.key).uid}});
        rows[id]={{"group_key",op.request.canonical_record_key},{"sources",std::move(sources)},{"created",std::move(created)},
            {"attempted",op.native.attempted},{"charged",op.native.charged},{"original_calls",op.native.duplicate_calls},
            {"original_returns",op.native.duplicate_returns},{"graph",op.native.graph},{"cash_inverses",op.native.cash_inverses},
            {"destroy_inverses",op.native.destroy_inverses},{"graph_committed",op.native.graph_committed},
            {"selection_restored",op.native.selection_restored},{"confirmed",op.confirmed},{"retired",op.retired},{"cause",op.cause},{"cleanup_cause",op.native.cleanup_cause}};}
    json histories=json::object();for(const auto& [id,op]:history_operations)histories[id]={
        {"source_action_uuid",op.request.target_action_uuid},{"graph",op.graph},{"inverse",op.inverse},{"attempted",op.attempted},{"returned",op.returned},
        {"selection_restored",op.selection_restored},{"confirmed",op.confirmed},{"retired",op.retired},{"cause",op.cause},{"cleanup_cause",op.cleanup_cause}};
    return {{"last_observation",Config::instance().diagnostic_mailbox_enabled?last_observation:json(nullptr)},{"last_submission",last_submission},{"last_source_records",Config::instance().diagnostic_mailbox_enabled?last_source_records:json(nullptr)},{"history",std::move(histories)},{"pending",pending()},{"captures",captures},{"submitted",submitted},{"rejected",rejections},{"cause",cause},{"operations",std::move(rows)}};
}
}
