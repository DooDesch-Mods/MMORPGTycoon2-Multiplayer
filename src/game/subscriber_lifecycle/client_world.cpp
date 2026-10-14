#include "client_world.h"
#include "mt2mp/core/config.h"
#include <algorithm>
#include <map>
#include <set>
#include <Windows.h>
namespace mt2mp::subscriber_lifecycle::first_append {
namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
template<class T>bool at(Memory& m,uintptr_t p,size_t o,T& value,std::string& e){
    if(!p||p>UINTPTR_MAX-o||p+o>UINTPTR_MAX-sizeof value)return fail(e,"Client Subscriber read extent invalid");
    return m.read(p+o,&value,sizeof value,e);
}
bool ap(Memory& m,const ClientWorld& w,uintptr_t p,uintptr_t r,std::string& e){uintptr_t v=0;return at(m,p,0,v,e)&&(v==w.image+r||fail(e,"Client Subscriber addresspoint mismatch"));}
bool array(Memory& m,uintptr_t owner,size_t base,std::vector<uintptr_t>& out,std::string& e){
    uintptr_t data=0;int32_t count=-1,capacity=-1;
    if(!at(m,owner,base+8,data,e)||!at(m,owner,base+0x10,count,e)||!at(m,owner,base+0x14,capacity,e))return false;
    if(count<0||capacity<count||(count&&!data)||size_t(count)>(UINTPTR_MAX-data)/8)return fail(e,"Client Subscriber array bounds invalid");
    out.clear();out.reserve(size_t(count));
    for(int32_t i=0;i<count;++i){uintptr_t p=0;if(!at(m,data,size_t(i)*8,p,e))return false;out.push_back(p);}return true;
}
bool root_once(Memory& m,const std::string& sha,const std::string& origin,bool held,ClientWorld& out,std::string& e){
    out={};const auto& cfg=Config::instance();
    if(!cfg.local_test||cfg.is_host||cfg.peer_id<2||cfg.peer_id>4)return fail(e,"Subscriber append requires owned Client2..4");
    std::string actual;
    if(!game::native::read_resolver_receipt(out.image,out.thread,actual,e))return false;
    if(actual!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"||out.thread!=GetCurrentThreadId())return fail(e,"Subscriber append build/mainthread differs");
    if(!game::native_open_runtime::read_current_world(out.origin,e))return false;
    if(sha.size()!=64||out.origin.sha256!=sha||out.origin.uuid!=origin||!out.origin.world_epoch||!out.origin.generation)return fail(e,"Subscriber append actual loaded baseline SHA/origin differs");
    game::native::WorldStatus status;game::native::ClockState clock;
    if(!game::native::read_world_status(status,e)||!game::native::read_clock(clock,e))return false;
    if(status.active_mode_index!=3||status.in_game_state!=4||!status.world_present||status.quitting||status.title_transition_pending||status.map_task_pending||status.edit_operations!=0||status.saves_in_progress!=(held?1:0))return fail(e,"Subscriber append active World/effects guard failed");
    if(clock.time_scale!=0||!clock.paused)return fail(e,"Subscriber append requires actual Client Clock0");
    const auto state=reinterpret_cast<uintptr_t>(out.origin.state),game=reinterpret_cast<uintptr_t>(out.origin.game);uintptr_t single=0;
    if(!ap(m,out,state,0x16de920,e)||!at(m,state,0x1b8,out.manager,e)||!at(m,out.image,0x13df690,single,e)||single!=out.manager||!ap(m,out,out.manager,0x1725f40,e))return fail(e,e.empty()?"Subscriber Manager ownership differs":e.c_str());
    if(!at(m,out.image,0x13df340,out.task,e)||!at(m,out.image,0x13df350,out.tick,e)||!at(m,out.image,0x13df4a0,out.update,e)||
       !ap(m,out,out.task,0x16fdaf0,e)||!ap(m,out,out.tick,0x16fdc40,e)||!ap(m,out,out.update,0x1709650,e))return false;
    uintptr_t task=0,tick=0,update=0;
    if(!at(m,game,0x68,task,e)||!at(m,game,0x70,update,e)||!at(m,game,0x150,tick,e))return false;
    if(task!=out.task||tick!=out.tick||update!=out.update)return fail(e,"Subscriber append actual Game scheduler ownership differs");
    uint8_t tp=0,kp=0,up=0;
    if(!at(m,task,0xf8,tp,e)||!at(m,tick,0x28,kp,e)||!at(m,update,0x68,up,e))return false;
    if(tp!=uint8_t(held)||kp!=uint8_t(held)||up!=uint8_t(held))return fail(e,"Subscriber append original scheduler pause state differs");
    if(held){int32_t running=-1,waiting=-1;if(!at(m,task,0xa8,running,e)||!at(m,task,0x90,waiting,e))return false;if(running||waiting)return fail(e,"Subscriber append original TaskManager not drained");}
    return true;
}
}
bool same_client_owner(const ClientWorld& a,const ClientWorld& b){
    const auto& x=a.origin;const auto& y=b.origin;
    return x.game==y.game&&x.mode==y.mode&&x.state==y.state&&x.world==y.world&&x.map==y.map&&x.uuid==y.uuid&&x.sha256==y.sha256&&x.generation==y.generation&&x.world_epoch==y.world_epoch&&a.image==b.image&&a.thread==b.thread&&a.manager==b.manager&&a.task==b.task&&a.tick==b.tick&&a.update==b.update;
}
bool read_cohort(Memory& m,const ClientWorld& w,Census& out,std::string& e,bool partition){
    out={};out.epoch=w.origin.world_epoch;out.origin_generation=w.origin.generation;out.world=reinterpret_cast<uintptr_t>(w.origin.world);out.manager=w.manager;
    if(!at(m,w.manager,0x68,out.next_id,e))return false;
    if(out.next_id<0)return fail(e,"Subscriber manager nextID negative");
    std::vector<uintptr_t> members,on,off;
    if(!array(m,w.manager,0x38,members,e)||!array(m,w.manager,0x80,on,e)||!array(m,w.manager,0x98,off,e))return false;
    std::map<uintptr_t,int32_t> ids;std::set<int32_t> unique;
    for(auto p:members){Member item;item.subscriber=p;uint8_t logged=0;
        if(!ap(m,w,p,0x16e75f0,e)||!at(m,p,0x20,item.id,e)||!at(m,p,0x80,item.toon,e)||!at(m,p,0x88,item.brain,e))return false;
        if(item.id<0||(partition&&item.id>=out.next_id)||!unique.insert(item.id).second||!ids.emplace(p,item.id).second)return fail(e,"Subscriber full cohort IDs/objects duplicate or invalid");
        if(item.toon){uintptr_t reverse=0;if(!ap(m,w,item.toon,0x1744e90,e)||!at(m,item.toon,0x940,reverse,e)||!at(m,item.toon,0x825,logged,e))return false;if(reverse!=p||logged>1)return fail(e,"Subscriber Toon backlink/login differs");}
        item.online=logged!=0;
        if(item.brain){uintptr_t owner=0;if(!ap(m,w,item.brain,0x16fdd60,e)||!at(m,item.brain,0x28,owner,e))return false;if(owner!=p)return fail(e,"Subscriber Brain ownership differs");}
        out.members.push_back(item);
    }
    std::set<uintptr_t> classified;
    for(const auto& pair:{std::pair{&on,&out.online},std::pair{&off,&out.offline}})for(auto p:*pair.first){auto it=ids.find(p);if(it==ids.end()||!classified.insert(p).second)return fail(e,"Subscriber login partition duplicate/foreign member");pair.second->push_back(it->second);}
    if(partition){for(const auto& member:out.members){const auto& values=member.online?out.online:out.offline;if(std::find(values.begin(),values.end(),member.id)==values.end())return fail(e,"Subscriber actual login partition incomplete");}}
    return true;
}
bool read_client_world(Memory& memory,const std::string& sha,const std::string& origin,bool held,ClientWorld& out,std::string& e){
    ClientWorld a,b;
    if(!root_once(memory,sha,origin,held,a,e)||!read_cohort(memory,a,a.census,e)||!root_once(memory,sha,origin,held,b,e)||!read_cohort(memory,b,b.census,e))return false;
    if(!same_client_owner(a,b)||a.census.next_id!=b.census.next_id||a.census.members!=b.census.members||a.census.online!=b.census.online||a.census.offline!=b.census.offline)return fail(e,"Subscriber Client double-read census changed");
    out=std::move(b);e.clear();return true;
}
}
