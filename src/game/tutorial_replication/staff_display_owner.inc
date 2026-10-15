namespace display_native {
using json=nlohmann::json;
struct Developer {uintptr_t address=0;int32_t id=0,job=0,row=-1;uint8_t employed=0,on_map=0,wanted=0;bool operator==(const Developer&)const=default;};
struct Owners {tutorial_development::StartingpointSnapshot snapshot;std::vector<Developer> developers;int32_t count=0;};
struct Cache {action_session::Context context;action_session::Binding binding;World world;uintptr_t project=0,tech=0,record=0;staff_display::State target;Owners owners;};
std::optional<Cache> cache;
struct Receipt {bool rendered=false,deferred=false,unknown=false;std::string cause;uint64_t original_calls=0,post_calls=0,confirmed_renders=0;json visible=nullptr;} receipt;
std::string native_fault;
void clear(){cache.reset();receipt.rendered=false;receipt.deferred=false;receipt.visible=nullptr;if(!receipt.unknown)receipt.cause.clear();}
void defer(const std::string& cause){receipt.rendered=false;receipt.deferred=true;receipt.visible=nullptr;if(receipt.cause!=cause)LOG_INFO("[TutorialStaffDisplay] deferred cause={}",cause);receipt.cause=cause;}
void unknown(const std::string& cause){receipt.rendered=false;receipt.deferred=false;receipt.unknown=true;receipt.visible=nullptr;if(native_fault.empty()){native_fault=cause;LOG_ERROR("[TutorialStaffDisplay] native display Unknown; no replay cause={}",cause);}receipt.cause=native_fault;}
bool read_owners(const tutorial_development::StartingpointSnapshot& snapshot,Owners& out,std::string& e){
    out={};out.snapshot=snapshot;
    if(!snapshot.creation||!snapshot.row||!snapshot.project||!snapshot.row->project_present||!snapshot.row->purchased||snapshot.row->deployed)
        return fail(e,"Staff display requires owned current purchased Startingpoint project");
    uintptr_t business=0;int32_t business_count=0;
    if(!at(reinterpret_cast<uintptr_t>(snapshot.origin.state),0x1c0,business,e)||!at(business,0x4c,business_count,e)||!at(snapshot.lounge,0x310,out.count,e))return false;
    if(out.count<0||out.count>50||business_count!=out.count||snapshot.creation->developers.size()!=50)
        return fail(e,"Staff display Business/Lounge hired count or50-entry catalogue differs");
    std::set<int32_t> ids;int32_t row=0;
    for(const auto& source:snapshot.creation->developers){Developer d;d.address=source.address;d.job=source.job;d.row=row++;
        if(!type(d.address,0x17422e0,e)||!at(d.address,0x4c0,d.id,e)||!at(d.address,0x4d8,d.wanted,e)||!at(d.address,0x4d9,d.employed,e)||!at(d.address,0x368,d.on_map,e))return false;
        if(d.id<0||d.id>49||!ids.insert(d.id).second||d.job<0||d.job>7||d.wanted>1||d.employed>1||d.on_map>1||d.wanted!=(d.row<out.count))
            return fail(e,"Staff display actual Developer identity/job/employment prefix differs");
        out.developers.push_back(d);
    }return true;
}
bool bind_target(const staff_display::State& target,const Owners& owners,std::string& e){
    if(!staff_display::valid(target,e))return false;
    std::set<int32_t> actual;for(const auto& d:owners.developers)if(d.employed)actual.insert(d.id);
    std::set<int32_t> wanted;for(const auto& d:target.developers)wanted.insert(d.id);
    return actual==wanted||fail(e,"Staff display Host/local employed Developer IDs differ");
}
bool cache_bound(const Cache& value,const action_session::Context& c,const action_session::Binding& b,const tutorial_development::StartingpointSnapshot& s){
    return value.context==c&&value.binding==b&&same_world(value.world,s.origin)&&value.project==s.project&&value.tech==s.tech&&value.record==s.record&&s.row&&s.row->project_present&&!s.row->deployed;
}
bool capture(const action_session::Context& c,const action_session::Binding& b,const tutorial_development::StartingpointSnapshot& snapshot,std::optional<staff_display::State>& out,std::string& e){
    out.reset();if(!snapshot.row||!snapshot.row->project_present||snapshot.row->deployed)return true;
    Owners owners;if(!read_owners(snapshot,owners,e))return false;
    if(c.role==action_session::Role::Client){if(cache&&cache_bound(*cache,c,b,snapshot)){if(!bind_target(cache->target,owners,e))return false;out=cache->target;}return true;}
    staff_display::State value;
    for(const auto& d:owners.developers)if(d.employed){value.developers.push_back({d.id,d.job,d.on_map!=0});if(!d.on_map&&d.job>=5&&d.job<=7)++value.active_tech;}
    // Native GetActiveDevCount includes every outdoor manager entry, including
    // a departing developer still completing its original employment lifetime.
    int32_t outside=0;for(const auto& d:owners.developers)if(d.on_map)++outside;
    value.active_developers=value.active_tech+outside;
    std::sort(value.developers.begin(),value.developers.end(),[](const auto& a,const auto& z){return a.id<z.id;});
    if(!staff_display::valid(value,e))return false;out=std::move(value);return true;
}
bool admit(const action_session::Context& c,const action_session::Binding& b,const std::optional<staff_display::State>& target,std::string& e,bool install){
    if(!target){if(install)clear();return true;}
    World world;if(!origin(c,b,world,e))return false;
    tutorial_development::StartingpointSnapshot s;json d;if(!tutorial_development::read_startingpoint(s,d,e,true)||!same_world(world,s.origin))return fail(e,e.empty()?"Staff display full Origin differs":e);
    // A creation frame is checked against its catalogue before creation and
    // against its actual newly allocated current project when installed.
    if(!s.project&&!install)return true;
    Owners owners;if(!read_owners(s,owners,e)||!bind_target(*target,owners,e))return false;
    World after;if(!origin(c,b,after,e)||!same_world(world,after))return fail(e,e.empty()?"Staff display Origin changed during admission":e);
    if(install){const bool changed=!cache||!cache_bound(*cache,c,b,s)||cache->target!=*target;cache=Cache{c,b,world,s.project,s.tech,s.record,*target,owners};if(changed){receipt.rendered=false;receipt.visible=nullptr;receipt.deferred=true;}}
    return true;
}
json status(){return {{"adopted",cache.has_value()},{"rendered",receipt.rendered},{"deferred",receipt.deferred},{"unknown",receipt.unknown},{"cause",receipt.cause},
    {"original_calls",receipt.original_calls},{"post_calls",receipt.post_calls},{"confirmed_renders",receipt.confirmed_renders},{"visible",receipt.visible},
    {"host",cache?staff_display::encode(cache->target):json(nullptr)}};}
}
