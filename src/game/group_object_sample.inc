namespace group_object {
namespace {
namespace q=npc_quest_targets;
namespace ui=action::ui_capture;
using Family=action::DuplicateFamily;
using Handle=duplicate_native::Handle;
struct Sample {duplicate_edit::Sample props;action::GroupObjectState state;};
bool hash(const std::string& text,std::string& out,std::string& e){
    out=mt2mp::sha256(std::vector<unsigned char>(text.begin(),text.end()));
    return duplicate_transition::sha(out)||q::fail(e,"Group complete Record SHA256 failed");
}
bool sample(Runtime& r,const action::Binding& binding,const action::GroupObjectIntent& intent,Sample& out,std::string& e){
    out={};if(!duplicate_edit::sample_untouched_scenery(r,binding,nullptr,{},out.props,e))return false;
    out.state.ownership=out.props.groups.state;out.state.records.random=out.props.state.random;out.state.records.release=out.props.state.release;
    std::set<std::string> touched(intent.members.begin(),intent.members.end());touched.insert(intent.key);
    auto untouched=json::array();for(const auto& [key,row]:out.props.objects){
        if(touched.contains(key))out.state.records.sources.push_back(row);
        else untouched.push_back(scenery_stroke_state::row(row));
    }
    duplicate_edit::append_untouched_scenery(out.props,untouched);
    return hash(untouched.dump(),out.state.records.untouched_sha,e);
}
bool same_binding(const Sample& before,const Sample& after){return duplicate_edit::same_binding(before.props,after.props);}
void trim(Sample& value){
    value.props.census={};value.props.groups={};value.props.handles.clear();value.props.objects.clear();value.props.untouched_scenery.clear();value.props.props.clear();value.props.external_owners.clear();
    value.props.state.sources.clear();value.props.state.created.clear();
}
bool native_target(Runtime& r,const action::Binding& binding,uintptr_t manager,uintptr_t object,ui::LocalBinding& out,std::string& e){
    out={};group_edit::Sample current;
    if(!group_edit::sample(r,binding,{},current,e)||manager!=current.world.selection)return q::fail(e,e.empty()?"Group Destroy current SelectionManager differs":e);
    const auto found=std::find_if(current.local.handles.begin(),current.local.handles.end(),[&](const auto& row){return row.second.object==object&&group_object_key::valid(row.first);});
    if(found==current.local.handles.end())return q::fail(e,"Group Destroy full current manager/UID binding is absent");
    out={manager,object,found->second.uid,binding.native_epoch,binding.native_generation,net::current_snapshot_id(),found->first,false,false,false,false,false,true};return true;
}
}
bool remove_local_binding(uintptr_t manager,uintptr_t object,action::ui_capture::LocalBinding& out,std::string& e){
    out={};auto* r=published.load(std::memory_order_acquire);action::Binding binding;
    if(!r||!r->ready||r->thread!=GetCurrentThreadId()||!typed_action_runtime::network_binding(net::current_snapshot_id(),binding,e))
        return q::fail(e,e.empty()?"Group Destroy requires the bound ready Mainthread":e);
    return native_target(*r,binding,manager,object,out,e);
}
}
