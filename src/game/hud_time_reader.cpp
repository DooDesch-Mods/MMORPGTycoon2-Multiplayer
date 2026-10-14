#include "hud_time_reader.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <vector>

namespace mt2mp::game::hud_time {
namespace {
constexpr char build_hash[]="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
constexpr std::array<float,4> scales{0,1,3,9};
constexpr std::array<const char*,4> names{"@Time Pause","@Time Normal","@Time Fast","@Time Fastest"};
struct Busy { bool& value; explicit Busy(bool& v):value(v){value=true;} ~Busy(){value=false;} };
bool fail(std::string& e,const std::string& reason){e=reason;return false;}
}
bool Reader::Witness::operator==(const Witness& other) const {
    return roots==other.roots && items==other.items && types==other.types && slots==other.slots && buttons==other.buttons && proxies==other.proxies&&type_proxies==other.type_proxies;
}
bool Reader::bytes(uintptr_t p,void* out,size_t n,std::string& e) const {
    if (!p || n>UINTPTR_MAX-p) return fail(e,"hud_time_read_address_overflow");
    if (!io_.read(io_.user,p,out,n,e)) { e="hud_time_read_failed: "+e;return false; }
    return true;
}
bool Reader::vtable(uintptr_t p,uintptr_t expected,const char* label,std::string& e) const {
    uintptr_t actual=0;
    if (!at(p,0,actual,e)) {e=std::string(label)+": "+e;return false;}
    return actual==config_.image+expected || fail(e,std::string(label)+"_vtable_mismatch");
}
bool Reader::boolean(uintptr_t p,size_t offset,bool& out,const char* label,std::string& e) const {
    uint8_t value=0;
    if (!at(p,offset,value,e)) {e=std::string(label)+": "+e;return false;}
    if (value>1) return fail(e,std::string(label)+"_not_boolean");
    out=value!=0;return true;
}
bool Reader::name(uintptr_t p,size_t offset,const char* expected,std::string& e) const {
    uintptr_t text=0;uint64_t length=0;
    if (!at(p,offset,text,e)||!at(p,offset+8,length,e)) return false;
    const size_t n=std::strlen(expected);std::array<char,24> actual{};
    if (n>actual.size()||length!=n) return fail(e,"hud_time_name_length_mismatch");
    if (!bytes(text,actual.data(),n,e)) return false;
    return std::memcmp(actual.data(),expected,n)==0 || fail(e,"hud_time_name_mismatch");
}
bool Reader::initialize(const Config& config,const Services& io,std::string& e) {
    if (busy_) return fail(e,"hud_time_reentrant");
    Busy lock(busy_);
    if (attempted_) return fail(e,"hud_time_initialize_already_attempted");
    attempted_=true;
    try {
        if (!io.read||!io.thread_id||!config.image||config.image>UINTPTR_MAX-0x2000000||!config.game_thread||config.executable_sha256!=build_hash)
            return fail(e,"hud_time_config_build_guard");
        io_=io;config_=config;
        if (io_.thread_id(io_.user)!=config_.game_thread) return fail(e,"hud_time_wrong_thread");
        struct Guard{uintptr_t rva;const char* bytes;size_t count;};
        constexpr Guard guards[]{
            {0x62c400,"\x41\x57\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53\x48\x83\xec\x78",16},
            {0x583b20,"\x57\x56\x53\x48\x83\xec\x20\x48\x8b\x05\xd2\x74\x0c\x01",14},
            {0x5816c0,"\x41\x57\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53\x48\x81\xec\xc8",16},
            {0x20e8d0,"\x48\x8b\x05\xc9\x10\x1d\x01\x48\x85\xc0\x74\x25\xf3\x0f\x10\x40",16},
        };
        for (const auto& g:guards) {std::array<char,16> actual{};if(!bytes(config_.image+g.rva,actual.data(),g.count,e))return false;
            if(std::memcmp(actual.data(),g.bytes,g.count))return fail(e,"hud_time_prologue_mismatch_rva="+std::to_string(g.rva));}
        ready_=true;e.clear();return true;
    } catch(const std::exception& ex){return fail(e,std::string("hud_time_initialize_exception: ")+ex.what());}
      catch(...){return fail(e,"hud_time_initialize_unknown_exception");}
}
bool Reader::once(State& out,Witness& witness,std::string& e) {
    uintptr_t game=0,mode=0,active=0,world=0,hud=0,panel=0,clock=0,options=0,owned_hud=0;
    int32_t mode_state=0;bool quitting=false;
    if(!at(config_.image,0x13df8f0,game,e)||!vtable(game,0x1743740,"game",e)||
       !at(config_.image,0x13df320,mode,e)||!vtable(mode,0x16e6730,"mode",e)||!at(game,0x28,active,e))return false;
    if(active!=mode)return fail(e,"hud_time_mode_not_active");
    if(!at(mode,0x2c4,mode_state,e)||!boolean(mode,0x304,quitting,"mode_quitting",e)||!at(mode,0x58,world,e))return false;
    if(mode_state!=4||quitting||!world)return fail(e,"hud_time_world_not_active");
    if(!at(config_.image,0x13df8b0,hud,e)||!vtable(hud,0x1742750,"hud",e)||!at(mode,0x128,owned_hud,e))return false;
    if(hud!=owned_hud)return fail(e,"hud_time_mode_hud_mismatch");
    if(!at(hud,0x60,panel,e)||!vtable(panel,0x16e4140,"clock_panel",e)||!name(panel,0x1b0,"clockpanel",e)||
       !boolean(panel,0x45f,out.clock_panel_visible,"clock_panel_visible",e))return false;
    if(!at(config_.image,0x13df9a0,clock,e)||!vtable(clock,0x17499d0,"clock",e)||
       !at(clock,0x58,out.native_selected_scale,e)||!at(clock,0x5c,out.native_pause_scale,e)||
       !at(clock,0x44,out.flat_day,e)||!at(clock,0x48,out.seconds,e)||!boolean(clock,0x50,out.native_pause_indicator,"pause_indicator",e))return false;
    if(!std::isfinite(out.native_selected_scale)||!std::isfinite(out.native_pause_scale)||out.seconds<0||out.seconds>86400)
        return fail(e,"hud_time_clock_fields_invalid");
    uintptr_t pause_window=0;
    if(!at(clock,0x68,pause_window,e))return false;
    out.pause_window_present=pause_window!=0;
    if(pause_window && (!vtable(pause_window,0x17883d0,"pause_window",e)||!name(pause_window,0x1b0,"pause_hud",e)||
       !boolean(pause_window,0x45f,out.pause_window_visible,"pause_window_visible",e)))return false;
    if(!at(config_.image,0x13df1e0,options,e)||!vtable(options,0x16c0db0,"options",e)||!boolean(options,0x191,out.pause_option_enabled,"pause_option_enabled",e))return false;
    witness.roots={game,mode,world,hud,panel,clock,options,pause_window};
    std::vector<uintptr_t> children;
    for(size_t offset:{size_t(0x2c8),size_t(0x2f8)}) {
        const uintptr_t table=offset==0x2c8?0x16ef880:0x1748bf0;
        uintptr_t data=0;int32_t count=0,capacity=0;
        if(!vtable(panel+offset,table,"panel_children",e)||!at(panel,offset+8,data,e)||!at(panel,offset+0x10,count,e)||!at(panel,offset+0x14,capacity,e))return false;
        if(count<0||capacity<count||capacity>256||(count&&!data))return fail(e,"hud_time_panel_children_bound");
        for(int32_t i=0;i<count;++i){uintptr_t child=0;if(!at(data,size_t(i)*8,child,e))return false;
            if(!child||std::find(children.begin(),children.end(),child)!=children.end())return fail(e,"hud_time_duplicate_or_null_child");children.push_back(child);}
    }
    out.panel_children=static_cast<uint32_t>(children.size());
    for(uintptr_t slot:children) {
        uintptr_t table=0,owner=0,parent=0;
        if(!at(slot,0,table,e)||!at(slot,0x280,owner,e)||!at(slot,0x278,parent,e))return false;
        if(owner!=panel||parent!=panel)return fail(e,"hud_time_child_owner_mismatch");
        if(table==config_.image+0x16dc680)continue;
        if(table!=config_.image+0x16db1c0)return fail(e,"hud_time_unexpected_panel_child_type");
        uintptr_t button=0,item=0,weak_item=0,proxy=0,item_proxy=0,live=0,type=0,type_proxy=0,type_live=0;
        if(!at(slot,0x368,button,e)||!vtable(button,0x16e5b90,"time_button",e)||!at(slot,0x360,item,e)||
           !vtable(item,0x17437f0,"hud_item",e)||!at(button,0x350,parent,e))return false;
        if(parent!=slot)return fail(e,"hud_time_button_dock_mismatch");
        if(!at(button,0x278,parent,e)||!at(button,0x280,owner,e))return false;
        if(parent!=slot||owner!=panel)return fail(e,"hud_time_button_owner_mismatch");
        if(!at(button,0x2d0,weak_item,e)||!at(button,0x2d8,proxy,e)||!at(item,0x28,item_proxy,e)||!at(proxy,0,live,e))return false;
        if(weak_item!=item||proxy!=item_proxy||live!=item+0x20)return fail(e,"hud_time_button_item_weak_mismatch");
        uintptr_t type_owned_proxy=0;
        if(!at(item,0x68,type,e)||!vtable(type,0x1723f50,"hud_item_type",e)||!at(item,0x70,type_proxy,e)||
           !at(type_proxy,0,type_live,e)||!at(type,0x40,type_owned_proxy,e))return false;
        if(type_live!=type+0x38||type_owned_proxy!=type_proxy)return fail(e,"hud_time_item_type_weak_mismatch");
        float scale=0;int32_t state=0;
        if(!at(type,0x15c,scale,e)||!at(item,0x38,state,e))return false;
        const auto found=std::find(scales.begin(),scales.end(),scale);
        if(found==scales.end()||(state!=0&&state!=1))return fail(e,"hud_time_item_scale_or_state_invalid");
        const size_t index=static_cast<size_t>(found-scales.begin());
        if(witness.items[index])return fail(e,"hud_time_duplicate_scale");
        if(!name(type,0x48,names[index],e)||!name(slot,0x318,names[index],e))return false;
        witness.items[index]=item;witness.types[index]=type;witness.slots[index]=slot;witness.buttons[index]=button;witness.proxies[index]=proxy;
        witness.type_proxies[index]=type_proxy;
        out.items[index]={scale,state,state==0};
        if(!boolean(button,0x380,out.items[index].button_cached_active,"button_cached_active",e)||
           !boolean(button,0x381,out.items[index].button_enabled,"button_enabled",e)||!at(item,0x30,owner,e))return false;
        out.items[index].actor_owner_present=owner!=0;
        ++out.time_items;
    }
    if(out.time_items!=4)return fail(e,"hud_time_missing_time_item");
    out.complete=true;return true;
}
bool Reader::read(State& out,std::string& e) {
    Witness witness{};return read_bound(out,witness,e);
}
void Reader::activation_binding(const State& state,const Witness& witness,size_t index,ActivationBinding& out) {
    out={true,state.clock_panel_visible,state.items[index].button_enabled,witness.roots[2],witness.roots[4],
        witness.slots[index],witness.buttons[index],witness.items[index],witness.types[index],
        witness.proxies[index],witness.type_proxies[index],state.items[index].native_state,
        state.items[index].scale,state.native_selected_scale};
}
bool Reader::classify_activation(uintptr_t type,uintptr_t item,ActivationBinding& out,std::string& e) {
    out={};State state;Witness witness;
    if(!read_bound(state,witness,e))return false;
    for(size_t index=0;index<witness.items.size();++index)if(witness.items[index]==item&&witness.types[index]==type){
        activation_binding(state,witness,index,out);return true;
    }
    e.clear();return true;
}
bool Reader::button_binding(float scale,ActivationBinding& out,std::string& e) {
    out={};const auto found=std::find(scales.begin(),scales.end(),scale);
    if(found==scales.end())return fail(e,"hud_time_button_scale_invalid");
    State state;Witness witness;if(!read_bound(state,witness,e))return false;
    activation_binding(state,witness,static_cast<size_t>(found-scales.begin()),out);return true;
}
bool Reader::read_bound(State& out,Witness& witness,std::string& e) {
    witness={};
    if(busy_)return fail(e,"hud_time_reentrant");
    Busy lock(busy_);out={};
    try {
        if(!ready_)return fail(e,"hud_time_not_initialized");
        if(io_.thread_id(io_.user)!=config_.game_thread)return fail(e,"hud_time_wrong_thread");
        Witness before{},after{};State second{};
        if(!once(out,before,e)){out.complete=false;return false;}
        if(!once(second,after,e)){out.complete=false;return false;}
        if(!(before==after)){out.complete=false;return fail(e,"hud_time_owner_changed_during_read");}
        out=second;witness=after;e.clear();return true;
    } catch(const std::exception& ex){out.complete=false;return fail(e,std::string("hud_time_read_exception: ")+ex.what());}
      catch(...){out.complete=false;return fail(e,"hud_time_read_unknown_exception");}
}
}
