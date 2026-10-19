namespace scenery_stroke {
namespace {
namespace q=npc_quest_targets;
namespace calls=scenery_stroke_calls;
namespace poly=ui_terrain_runtime::poly;
using Kind=action::SceneryStrokeKind;
using Family=action::DuplicateFamily;
using Saved=scenery_make_history::Saved;
struct Sample {
    duplicate_edit::Sample props;
    action::SceneryStrokeState state;
    uintptr_t region=0,region_random=0;
    std::map<uint64_t,Saved> scenery;
};
bool digest(const std::string& text,std::string& out,std::string& e){
    out=mt2mp::sha256(std::vector<unsigned char>(text.begin(),text.end()));
    return duplicate_transition::sha(out)||q::fail(e,"Scenery stroke SHA256 failed");
}
bool birth_key(const std::string& request,size_t index,std::string& out,std::string& e){
    std::string hash;if(!digest("mt2mp/scenery-stroke/"+request+"/"+std::to_string(index),hash,e))return false;
    hash.resize(32);hash[12]='4';const auto digit=hash[16]<='9'?hash[16]-'0':hash[16]-'a'+10;hash[16]="89ab"[digit&3];
    out="action:"+hash.substr(0,8)+"-"+hash.substr(8,4)+"-"+hash.substr(12,4)+"-"+hash.substr(16,4)+"-"+hash.substr(20,12);return true;
}
bool region(Runtime& r,const host::WorldState& world,int32_t id,uintptr_t expected,uintptr_t& out,std::string& e){
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return q::at(r.memory,p,offset,value,cause);};
    if(!region_saved_identity::resolve(r.image,uintptr_t(world.origin.map),world.root,id,expected,out,at,e))return false;
    uintptr_t data=0;int32_t count=0,capacity=0;
    if(!q::at(r.memory,uintptr_t(world.origin.map),0x358,data,e)||!q::at(r.memory,uintptr_t(world.origin.map),0x360,count,e)||
       !q::at(r.memory,uintptr_t(world.origin.map),0x364,capacity,e))return false;
    if(count<0||count>4096||capacity<count||(!data&&count))return q::fail(e,"Scenery stroke owned Region array differs");
    bool owned=false;for(int32_t i=0;i<count;++i){uintptr_t item=0;if(!q::at(r.memory,data,size_t(i)*8,item,e))return false;owned|=item==out;}
    return owned||q::fail(e,"Scenery stroke Region is not owned");
}
bool sample(Runtime& r,const action::Binding& binding,int32_t id,const std::map<uint64_t,std::string>& created,Sample& out,std::string& e){
    out={};
    if(!duplicate_edit::sample(r,binding,nullptr,created,out.props,e)||!region(r,out.props.world,id,0,out.region,e)||
       !q::at(r.memory,out.region,0x160,out.region_random,e)||!out.region_random||!q::at(r.memory,out.region_random,0,out.state.region_random,e)||
       !scenery_stroke_state::digest(out.props.objects,digest,out.state.props_sha,e)||
       !scenery_stroke_bugs::read(r.memory,r.image,out.region,digest,out.state.bug_count,out.state.bugs_sha,e))return false;
    out.state.random=out.props.state.random;out.state.release=out.props.state.release;
    auto find=[&](uint64_t uid,uintptr_t& pointer,std::string& cause){return group_edit::find(r,uid,pointer,cause);};
    for(const auto& [key,row]:out.props.objects)if(row.family==Family::Scenery&&row.region==id){
        const auto handle=out.props.handles.at(key);Saved saved;
        if(!scenery_make_history::live(r.memory,r.image,handle.object,handle.uid,out.props.world.root,out.region,find,saved,e)||
           !out.scenery.emplace(handle.uid,std::move(saved)).second)return q::fail(e,e.empty()?"Scenery stroke current complete full UID repeats":e);
    }
    host::WorldState repeat;if(!world(r,repeat,e)||!host::same_world(out.props.world.origin,repeat.origin))
        return q::fail(e,e.empty()?"Scenery stroke bound World changed during complete capture":e);
    return true;
}
bool same_binding(const Sample& first,const Sample& last){
    return duplicate_edit::same_binding(first.props,last.props)&&first.region==last.region&&first.region_random==last.region_random;
}
bool same_current(const Sample& first,const Sample& last){
    return same_binding(first,last)&&first.state==last.state&&first.props.money==last.props.money&&first.props.undo==last.props.undo&&
        first.props.objects==last.props.objects&&first.props.handles==last.props.handles&&first.scenery==last.scenery;
}
void trim_sample(Sample& sample){
    sample.scenery.clear();sample.props.census={};sample.props.groups={};sample.props.handles.clear();sample.props.objects.clear();
    sample.props.props.clear();sample.props.external_owners.clear();sample.props.state.sources.clear();sample.props.state.created.clear();
}
bool text(Runtime& r,uintptr_t object,std::string& out,std::string& e){return prop_text::text(r,object,out,e);}
uint32_t directory_call(uintptr_t image,int32_t category,uintptr_t& out){
    __try{out=uintptr_t(reinterpret_cast<const void*(*)(int32_t)>(image+0x448660)(category));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool catalog(Runtime& r,int32_t category,int32_t variant,std::string& type,std::string& file,std::string& e){
    type.clear();file.clear();if(category<0||category>=47)return q::fail(e,"Scenery stroke category selection is invalid");
    if(variant!=-1){scenery_kiss::NativeCatalog native(r.memory.process,r.image,r.thread);return native.identify(category,variant,type,file,e);}
    uintptr_t directory=0;const auto code=directory_call(r.image,category,directory);
    if(code)return q::fail(e,"Scenery stroke original random catalog directory failed SEH="+std::to_string(code));
    return text(r,directory,type,e);
}
bool resolve_catalog(Runtime& r,const action::SceneryStrokeIntent& intent,int32_t& category,int32_t& variant,std::string& e){
    category=-1;variant=-1;if(intent.kind==Kind::Erase)return true;
    if(!intent.random_variant){scenery_kiss::State state;state.type=intent.type;state.variant=intent.variant;state.alive=true;
        scenery_kiss::NativeCatalog native(r.memory.process,r.image,r.thread);return native.resolve(state,category,variant,e);}
    for(int32_t index=0;index<47;++index){std::string type,file;if(!catalog(r,index,-1,type,file,e))return false;
        if(type==intent.type){category=index;return true;}}
    return q::fail(e,"Scenery stroke random catalog directory is not present");
}
}
}
