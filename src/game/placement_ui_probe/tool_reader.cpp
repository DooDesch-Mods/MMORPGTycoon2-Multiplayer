#include "ui_probe.h"
#include <cstring>
#include <limits>
#include <cmath>
namespace placement_ui_probe {
namespace {
bool fail(std::string& e,const char* text){if(e.empty())e=text;else e=std::string(text)+"; cause: "+e;return false;}
class BoundedMemory final:public Memory{Memory& source_;std::size_t reads_{};public:explicit BoundedMemory(Memory& source):source_(source){}bool copy(std::uintptr_t p,void* out,std::size_t n,std::string& e)override{if(++reads_>65536)return fail(e,"error: native UI diagnostic read budget exceeded, required<=65536");return source_.copy(p,out,n,e);}};
template<class T> bool at(Memory& m,std::uintptr_t p,std::size_t o,T& v,std::string& e){if(!p||p>UINTPTR_MAX-o)return fail(e,"error: native UI read address overflow");return m.copy(p+o,&v,sizeof(v),e);}
bool ap(Memory& m,const Context& c,std::uintptr_t p,std::uintptr_t expected,std::string& e){std::uintptr_t v=0;return at(m,p,0,v,e)&&(v==c.image+expected||fail(e,"error: native UI class differs"));}
bool text(Memory& m,std::uintptr_t p,std::string& out,std::string& e){std::uintptr_t data=0;std::uint64_t length=0;if(!at(m,p,0,data,e)||!at(m,p,8,length,e))return false;if(!data||!length||length>256)return fail(e,"error: native UI string span invalid");char bytes[256]{};if(!m.copy(data,bytes,std::size_t(length),e))return false;out.assign(bytes,std::size_t(length));return out.find('\0')==std::string::npos||fail(e,"error: native UI string contains NUL");}
bool array(Memory& m,std::uintptr_t p,std::uintptr_t& data,std::int32_t& count,std::size_t stride,std::string& e){std::int32_t capacity=0;if(!at(m,p,8,data,e)||!at(m,p,16,count,e)||!at(m,p,20,capacity,e))return false;if(count<0||count>4096||capacity<count||(count&&!data)||data>UINTPTR_MAX-std::size_t(count)*stride){e="error: native diagnostic array bound actual="+std::to_string(count)+" required=0..4096";return false;}return true;}
bool identifier(Memory& m,std::uintptr_t behaviour,const Request& r,int& index,std::string& e,std::size_t header=0x1a0){std::uintptr_t data=0;std::int32_t count=0;if(!array(m,behaviour+header,data,count,64,e))return false;index=-1;for(int i=0;i<count;++i){std::string type,variant;if(!text(m,data+std::size_t(i)*64,type,e)||!text(m,data+std::size_t(i)*64+32,variant,e))return false;if(type==r.type&&variant==r.variant){if(index>=0)return fail(e,"error: native building palette identifier is duplicated");index=i;}}return index>=0;}
bool native_child(Memory& m,std::uintptr_t view,std::uintptr_t wanted,bool& found,std::string& e){
    found=false;unsigned matches=0;
    for(std::size_t header:{std::size_t(0x2c8),std::size_t(0x2f8)}){
        std::uintptr_t data=0;std::int32_t count=0;if(!array(m,view+header,data,count,8,e))return false;
        for(std::int32_t i=0;i<count;++i){std::uintptr_t child=0;if(!at(m,data,std::size_t(i)*8,child,e))return false;if(child==wanted)++matches;}
    }
    found=matches==1;return true;
}
bool weak_type(Memory& m,std::uintptr_t item,std::uintptr_t type,std::uintptr_t& proxy,std::string& e){std::uintptr_t target=0,owned=0;if(!at(m,item,0x70,proxy,e)||!at(m,proxy,0,target,e)||!at(m,type,0x40,owned,e))return false;return target==type+0x38&&owned==proxy||fail(e,"error: native item/type Weak target differs");}
bool scene_directory(Memory& m,std::uintptr_t behaviour,const Request& r,std::string& e){
    std::uintptr_t data=0;std::int32_t count=0;if(!array(m,behaviour+0x158,data,count,32,e))return false;
    if(!count)return true;
    unsigned matches=0;for(int i=0;i<count;++i){std::string name;if(!text(m,data+std::size_t(i)*32,name,e))return false;if(name==r.type)++matches;}
    if(matches>1)return fail(e,"error: native Scenery directory filter duplicated");return matches==1;
}
bool scene_index(Memory& m,const Binding& b,int& index,std::string& e){
    std::uintptr_t data=0,apvalue=0;std::int32_t count=0;
    if(!at(m,b.behaviour,0x410,apvalue,e)||apvalue!=b.context.image+0x1747480||!array(m,b.behaviour+0x410,data,count,8,e))return fail(e,"error: native Scenery ListData array differs");
    index=-1;for(int i=0;i<count;++i){std::array<std::int32_t,2> pair{};if(!at(m,data,std::size_t(i)*8,pair,e))return false;
        if(pair==std::array<std::int32_t,2>{b.category,b.variant_hash}){if(index>=0)return fail(e,"error: native Scenery palette category/hash duplicated");index=i;}}
    return index>=0||fail(e,"error: native Scenery variant absent from unlocked palette");
}
bool scene_single(Memory& m,const Binding& b,std::string& e){
    std::int32_t mode=-1;std::uintptr_t options=0;std::string name;
    if(!at(m,b.behaviour,0x174,mode,e)||!at(m,b.behaviour,0xb68,options,e)||!ap(m,b.context,options,0x1715390,e)||!text(m,options+0x88,name,e))return false;
    return mode==0&&name=="single"||fail(e,"error: actual Scenery tool is not single mode");
}
bool normal_definition(Memory& m,const Context& c,const Request& r,std::string& e){
    std::uintptr_t manager=0,definitions=0,selected=0;std::int32_t count=0;
    if(!at(m,c.image,0x13df780,manager,e))return false;
    if(!manager)return fail(e,"error: actual BuildingModelManager cached singleton missing");
    if(!array(m,manager+0x30,definitions,count,8,e))return false;
    for(std::int32_t i=0;i<count;++i){
        std::uintptr_t value=0;std::string type;
        if(!at(m,definitions,std::size_t(i)*8,value,e)||!ap(m,c,value,0x172ae50,e)||!text(m,value+0x18,type,e))return false;
        if(type==r.type){if(selected)return fail(e,"error: actual BuildingDefinition type is ambiguous");selected=value;}
    }
    if(!selected)return fail(e,"error: requested type absent from actual BuildingDefinition catalog");
    if(r.type=="Scenery")return fail(e,"error: actual BuildingDefinition is the Scenery definition");
    std::uint32_t flags=0;if(!at(m,selected,0x16c,flags,e))return false;
    if(flags&(1u<<13))return fail(e,"error: actual BuildingDefinition is Scenery");
    if(flags&((1u<<16)|(1u<<17)))return fail(e,"error: actual BuildingDefinition uses a dungeon branch");
    if(((flags&(1u<<2))!=0)!=(r.type=="StartingPoint"))return fail(e,"error: actual StartingPoint definition type/feature differs");
    std::uintptr_t variants=0;std::int32_t variant_count=0;unsigned matches=0;
    if(!array(m,selected+0x170,variants,variant_count,8,e))return false;
    for(std::int32_t i=0;i<variant_count;++i){
        std::uintptr_t value=0;std::string name;
        if(!at(m,variants,std::size_t(i)*8,value,e)||!ap(m,c,value,0x1718550,e)||!text(m,value+0x18,name,e))return false;
        if(name==r.variant)++matches;
    }
    if(matches>1)return fail(e,"error: actual BuildingDefinition variant is ambiguous");
    return matches==1||fail(e,"error: requested variant absent from actual BuildingDefinition catalog");
}
bool scene_current_bar(Memory& m,const Context& c,std::uintptr_t& bar,std::string& e){
    constexpr unsigned char original[]={0x48,0x8b,0x05,0x81,0x55,0x4c,0x01,0xc3,0x0f,0x1f,0x84,0x00,0x00,0x00,0x00,0x00};
    unsigned char actual[sizeof(original)]{};
    if(!m.copy(c.image+0x575940,actual,sizeof(actual),e))return false;
    if(std::memcmp(actual,original,sizeof(original)))return fail(e,"error: native Scenery ActionBar.Current body differs");
    if(!at(m,c.image,0x1a3aec8,bar,e))return false;
    return bar&&ap(m,c,bar,0x16dcec0,e)||fail(e,"error: native Scenery current ActionBar is absent or foreign");
}
bool network_type(Memory& m,const Context& c,std::uintptr_t behaviour,const Request& r,std::string& e){
    std::string type,key,definition_type,definition_key;std::uintptr_t definition=0;
    if(!text(m,behaviour+0x148,type,e)||!text(m,behaviour+0x50,key,e)||!at(m,behaviour,0x168,definition,e)||!ap(m,c,definition,0x1738010,e)||
       !text(m,definition+0x18,definition_type,e)||!text(m,definition+0x38,definition_key,e))return false;
    return type==r.type&&key==r.variant&&type==definition_type&&key==definition_key;
}
bool network_pipe_type(Memory& m,const Context& c,std::uintptr_t behaviour,const Request& r,std::string& e){
    std::string type,key,node,definition_type,definition_key;std::uintptr_t definition=0;
    if(!text(m,behaviour+0x148,type,e)||!text(m,behaviour+0x168,node,e)||!text(m,behaviour+0x50,key,e)||!at(m,behaviour,0x190,definition,e)||!ap(m,c,definition,0x17380d0,e)||
       !text(m,definition+0x18,definition_type,e)||!text(m,definition+0x38,definition_key,e))return false;
    return type==r.type&&key==r.variant&&type==definition_type&&key==definition_key&&node=="Junction";
}
bool path_type(Memory& m,std::uintptr_t behaviour,const Request& r,std::string& e){std::string type,node;if(!text(m,behaviour+0x148,type,e)||!text(m,behaviour+0x168,node,e))return false;return type==r.type&&node==r.variant;}
bool terrain_style(Memory& m,std::uintptr_t behaviour,const Request& r,std::string& e){
    std::int32_t style=-1;return at(m,behaviour,0x150,style,e)&&style==r.terrain_style;
}
bool terrain_height_up(Memory& m,std::uintptr_t behaviour,const Request& r,std::string& e){
    unsigned char up=2;return at(m,behaviour,0x148,up,e)&&up<=1&&up==r.terrain_style;
}
bool npc_type(Memory& m,std::uintptr_t behaviour,const Request& r,std::string& e){
    std::string name;std::int32_t type=-1;
    if(!text(m,behaviour+0x148,name,e)||!at(m,behaviour,0x170,type,e))return false;
    return name==r.type&&type==(r.type=="QuestGiver"?0:r.type=="Guard"?1:2);
}
bool scan_once(Memory& m,const Context& c,const Request& r,Binding& b,std::string& e){
    b={};b.context=c;b.family=r.family;b.category=r.category;b.variant_hash=r.variant_hash;b.terrain_style=r.terrain_style;std::uintptr_t game=0,mode=0,active=0,world=0;int state=0;unsigned char quitting=1;
    if(!at(m,c.image,0x13df8f0,game,e)||!ap(m,c,game,0x1743740,e)||!at(m,c.image,0x13df320,mode,e)||!ap(m,c,mode,0x16e6730,e)||!at(m,game,0x28,active,e)||!at(m,mode,0x58,world,e)||!at(m,mode,0x2c4,state,e)||!at(m,mode,0x304,quitting,e))return false;
    if(active!=mode||world!=c.world||state!=4||quitting)return fail(e,"error: native placement UI world is not current mode state4");
    if(r.family==Family::Building){if(!normal_definition(m,c,r,e))return false;}
    else if(r.family==Family::RegionLevel){}
    else if(r.family==Family::NPC){if(r.type!="QuestGiver"&&r.type!="Guard"&&r.type!="ClassTrainer")return fail(e,"error: NPC native UI requires QuestGiver, Guard or ClassTrainer");}
    else if(r.family==Family::TerrainHeight){if(r.terrain_style<0||r.terrain_style>1)return fail(e,"error: native Height up outside 0..1");}
    else if(r.family==Family::Terrain){if(r.terrain_style<0||r.terrain_style>3)return fail(e,"error: native Terrain style outside 0..3");}
    else if(r.family==Family::NetworkPipe){if(r.type!="Cable"||r.variant!="item_cable")return fail(e,"error: NetworkPipe requires canonical Cable/item_cable");}
    else if(r.family==Family::NetworkNode){if(r.type.empty()||r.variant.empty())return fail(e,"error: NetworkNode requires canonical type/key");}
    else if(r.family==Family::Path){if(r.type!="Road"||r.variant!="RoadNode")return fail(e,"error: first path action requires canonical Road/RoadNode");}
    else if(r.family!=Family::Scenery||r.category<0||r.category>=47)return fail(e,"error: native Scenery catalog binding invalid");
    if(!at(m,c.image,0x13df9f0,b.player,e)||!ap(m,c,b.player,0x17873b0,e)||!at(m,b.player,0x1d0,b.inventory,e)||!ap(m,c,b.inventory,0x16df0f0,e)||!at(m,c.image,0x13df9d0,b.cursor,e)||!ap(m,c,b.cursor,0x1786dd0,e))return false;
    std::uintptr_t items=0;std::int32_t count=0;
    // The inventory is itself the vsArray pointer header (no independent capacity field).
    if(!at(m,b.inventory,8,items,e)||!at(m,b.inventory,16,count,e))return false;
    if(!items||count<=0||count>4096)return fail(e,"error: actual native inventory count outside diagnostic bound");
    for(int i=0;i<count;++i){std::uintptr_t item=0,type=0,table=0,behaviour=0,proxy=0;if(!at(m,items,std::size_t(i)*8,item,e))return false;if(!item)continue;if(!ap(m,c,item,0x17437f0,e)||!at(m,item,0x68,type,e)||!weak_type(m,item,type,proxy,e)||!at(m,type,0,table,e))return false;if(table!=c.image+0x173b7b0)continue;if(!at(m,type,0x160,behaviour,e)||!behaviour)continue;if(!at(m,behaviour,0,table,e))return false;if(table!=c.image+(r.family==Family::NPC?0x172b450:r.family==Family::RegionLevel?0x1741210:r.family==Family::Scenery?0x17396b0:(r.family==Family::NetworkPipe?0x173ed80:r.family==Family::NetworkNode?0x173ebd0:r.family==Family::Path?0x172f570:(r.family==Family::TerrainHeight?0x1740010:(r.family==Family::Terrain?0x173f580:0x17327d0)))))continue;int index=-1;std::string cause;if(!(r.family==Family::NPC?npc_type(m,behaviour,r,cause):r.family==Family::RegionLevel?true:r.family==Family::Scenery?scene_directory(m,behaviour,r,cause):(r.family==Family::NetworkPipe?network_pipe_type(m,c,behaviour,r,cause):r.family==Family::NetworkNode?network_type(m,c,behaviour,r,cause):r.family==Family::Path?path_type(m,behaviour,r,cause):(r.family==Family::TerrainHeight?terrain_height_up(m,behaviour,r,cause):(r.family==Family::Terrain?terrain_style(m,behaviour,r,cause):identifier(m,behaviour,r,index,cause,0x188)))))){if(!cause.empty()){e=cause;return false;}continue;}if(b.type&&b.type!=type)return fail(e,"error: actual placement tool type is ambiguous");b.inventory_item=item;b.type=type;b.behaviour=behaviour;}
    if(!b.type)return fail(e,"error: actual inventory has no matching enabled placement tool");
    std::uintptr_t bars=0;std::int32_t bar_count=0;
    if(!at(m,c.image,0x1a3af18,bars,e)||!at(m,c.image,0x1a3af20,bar_count,e))return false;
    if(!bars||bar_count<=0||bar_count>4096)return fail(e,"error: actual actionbar registry count outside diagnostic bound");
    std::uintptr_t current_bar=0;
    if(r.family!=Family::Building){
        if(!scene_current_bar(m,c,current_bar,e))return false;
        unsigned matches=0;
        for(int i=0;i<bar_count;++i){std::uintptr_t bar=0;if(!at(m,bars,std::size_t(i)*8,bar,e))return false;if(bar==current_bar)++matches;}
        if(matches!=1){e="error: native Scenery current ActionBar registry membership actual="+std::to_string(matches)+" required=1";return false;}
    }
    for(int bi=0;bi<bar_count;++bi){std::uintptr_t bar=0,contents=0;std::int32_t content_count=0;if(!at(m,bars,std::size_t(bi)*8,bar,e))return false;
        // Other action bars retain their docked items while another UI mode is active.
        if(r.family!=Family::Building&&bar!=current_bar)continue;
        if(!ap(m,c,bar,0x16dcec0,e)||!array(m,bar+0x4e8,contents,content_count,8,e))return false;
        for(int ci=0;ci<content_count;++ci){std::uintptr_t content=0,slots=0;std::int32_t slot_count=0;if(!at(m,contents,std::size_t(ci)*8,content,e)||!ap(m,c,content,0x171ccf0,e)||!array(m,content+0x348,slots,slot_count,8,e))return false;
            for(int si=0;si<slot_count;++si){std::uintptr_t slot=0,button=0,item=0,type=0,proxy=0,target=0,owned=0,owner=0,parent=0;if(!at(m,slots,std::size_t(si)*8,slot,e)||!slot)continue;if(!ap(m,c,slot,0x16db1c0,e)||!at(m,slot,0x368,button,e))return false;if(!button)continue;if(!ap(m,c,button,0x16e5b90,e)||!at(m,button,0x2d0,item,e)||!at(m,item,0x68,type,e))return false;if(type!=b.type)continue;
                if(!ap(m,c,item,0x17437f0,e)||!at(m,button,0x2d8,proxy,e)||!at(m,proxy,0,target,e)||!at(m,item,0x28,owned,e)||!at(m,button,0x350,parent,e)||!at(m,button,0x280,owner,e))return false;
                std::uintptr_t slot_parent=0,slot_owner=0,button_parent=0;
                if(!at(m,slot,0x278,slot_parent,e)||!at(m,slot,0x280,slot_owner,e)||!at(m,button,0x278,button_parent,e))return false;
                bool content_child=false,slot_child=false;
                if(!native_child(m,content,slot,content_child,e)||!native_child(m,slot,button,slot_child,e))return fail(e,"error: actual normal building native Layout child-array read failed");
                const bool constructor_context=!slot_parent&&!slot_owner&&!owner;
                const std::array<bool,8> checks{target==item+0x20,proxy==owned,parent==slot,button_parent==slot,slot_parent==content||constructor_context,owner==slot_owner,content_child,slot_child};
                bool match=true;for(bool value:checks)match=match&&value;
                if(!match){
                    constexpr const char* names[]{"weak_target_item20","weak_proxy_owned","button350_slot","button_layout_view_slot","slot_layout_view_content_or_ctor_null","window_context_equal","content_native_child_slot","slot_native_child_button"};
                    std::string report="error: actual normal building button weak/owner differs";
                    for(std::size_t i=0;i<checks.size();++i)report+="; "+std::string(names[i])+" actual="+(checks[i]?"1":"0")+" required=1";
                    report+="; observed_button280_nonnull="+std::string(owner?"1":"0")+"; observed_slot278_content="+(slot_parent==content?"1":"0")+"; observed_ctor_null_context="+(constructor_context?"1":"0");
                    return fail(e,report.c_str());
                }std::uintptr_t type_proxy=0;if(!weak_type(m,item,type,type_proxy,e))return false;
                unsigned char enabled=0;int item_state=-1;if(!at(m,button,0x381,enabled,e)||!at(m,item,0x38,item_state,e))return false;if(enabled!=1||item_state!=1)continue;
                if(b.button&&b.button!=button)return fail(e,r.family==Family::Scenery?"error: multiple enabled inactive Scenery buttons in current ActionBar are ambiguous":"error: multiple enabled inactive normal building buttons are ambiguous");b.bar=bar;b.content=content;b.slot=slot;b.button=button;b.item=item;b.item_proxy=proxy;b.type_proxy=type_proxy;
            }
        }
    }
    return b.button!=0||fail(e,"error: actual normal building UI button is missing or not enabled/inactive");
}
}
bool read_existing_tool(Memory& m,const Context& c,const Request& r,Binding& b,std::string& e){BoundedMemory bounded(m);Binding a,again;if(!scan_once(bounded,c,r,a,e)||!scan_once(bounded,c,r,again,e))return false;if(a!=again)return fail(e,"error: actual inventory/actionbar binding changed between reads");b=a;return true;}
bool read_scenery_mode_window(Memory& m,const Binding& b,std::uintptr_t& window,std::string& e){
    window=0;std::uintptr_t active=0,options=0;unsigned char visible=0,painting=0;
    std::array<unsigned char,3> flags{};std::uintptr_t graph=0,edit=0;
    if(b.family!=Family::Scenery||!at(m,b.cursor,0x190,active,e)||active!=b.behaviour||
       !ap(m,b.context,b.behaviour,0x17396b0,e)||!at(m,b.behaviour,0x408,window,e)||!ap(m,b.context,window,0x171ae90,e)||
       !at(m,window,0x45f,visible,e)||visible!=1||!at(m,b.behaviour,0xb68,options,e)||!ap(m,b.context,options,0x1715390,e)||
       !at(m,b.behaviour,0x6c0,flags,e)||!at(m,b.behaviour,0xb48,painting,e)||!at(m,b.behaviour,0xc50,graph,e)||!at(m,b.behaviour,0x360,edit,e))return fail(e,"error: Scenery mode selector active window/owner differs");
    if(flags!=std::array<unsigned char,3>{}||painting||graph||edit)return fail(e,"error: Scenery mode selector requires idle original tool");
    return true;
}
bool read_scenery_mode_button(Memory& m,const Binding& b,std::uintptr_t window,std::uintptr_t button,std::string& e){
    std::uintptr_t current=0,options=0,value=0;std::string id,mode,property;
    if(!read_scenery_mode_window(m,b,current,e)||current!=window)return fail(e,"error: Scenery mode selector window changed during lookup");
    if(!button||!ap(m,b.context,button,0x1713de0,e)||!text(m,button+0x1b0,id,e)||id!="mode_single")return fail(e,"error: original Scenery mode_single lookup returned wrong pane");
    if(!at(m,b.behaviour,0xb68,options,e)||!text(m,button+0x5a8,mode,e)||!text(m,button+0x5e0,property,e)||!at(m,button,0x600,value,e))return false;
    return mode=="single"&&property=="mode"&&value==options+0x88||fail(e,"error: Scenery mode_single native property binding differs");
}

bool read_palette_index(Memory& m,const Binding& b,const Request& r,std::int32_t& index,std::uintptr_t& list,std::string& e){
    if(b.family==Family::Scenery){std::uintptr_t active=0,window=0;std::int32_t count=0;
        if(!at(m,b.cursor,0x190,active,e)||active!=b.behaviour||!scene_single(m,b,e)||!scene_index(m,b,index,e)||
           !at(m,b.behaviour,0x408,window,e)||!ap(m,b.context,window,0x171ae90,e)||!at(m,window,0x4e8,list,e)||!ap(m,b.context,list,0x16dfc30,e)||!at(m,list,0x350,count,e))return fail(e,"error: actual Scenery active owner/palette differs");
        return index>=0&&index<count||fail(e,"error: native Scenery selector index differs from ListData");}
std::uintptr_t active=0,window=0;unsigned char palette=0;if(!at(m,b.cursor,0x190,active,e)||active!=b.behaviour)return fail(e,"error: actual active UI cursor owner differs after activation");if(!at(m,b.behaviour,0x6d0,palette,e)||!identifier(m,b.behaviour,r,index,e))return fail(e,"error: actual normal building palette identifier disappeared");list=0;if(palette){if(palette!=1||!at(m,b.behaviour,0x3a8,window,e)||!ap(m,b.context,window,0x171ae90,e)||!at(m,window,0x4e8,list,e)||!ap(m,b.context,list,0x16dfc30,e))return false;std::int32_t selectors=0;if(!at(m,list,0x350,selectors,e)||index<0||index>=selectors)return fail(e,"error: actual palette selector index differs from identifier index");}return true;}
bool read_live_pose_and_pending(Memory& m,const Binding& b,const Request& r,bool pending,std::string& e){
    if(b.family==Family::Scenery){std::uintptr_t active=0,ghost=0,blueprint=0,owned=0;std::array<unsigned char,3> flags{};std::int32_t selected=-1,index=-1,category=-1,hash=0;
        if(!at(m,b.cursor,0x190,active,e)||active!=b.behaviour||!ap(m,b.context,b.behaviour,0x17396b0,e)||!scene_single(m,b,e)||!scene_index(m,b,index,e)||
           !at(m,b.behaviour,0x368,selected,e)||selected!=index||!at(m,b.behaviour,0x178,category,e)||!at(m,b.behaviour,0x17c,hash,e)||category!=b.category||hash!=b.variant_hash||
           !at(m,b.behaviour,0xb58,ghost,e)||!at(m,b.behaviour,0xb50,blueprint,e)||!ap(m,b.context,ghost,0x1705230,e)||!ap(m,b.context,blueprint,0x16dd870,e)||
           !at(m,ghost,0x210,owned,e)||owned!=blueprint||!at(m,b.behaviour,0x6c0,flags,e))return fail(e,"error: native Scenery active palette/owned ghost differs");
        if(flags!=std::array<unsigned char,3>{static_cast<unsigned char>(pending?0:1),static_cast<unsigned char>(pending?1:0),0})return fail(e,"error: native Scenery pending flags differ");
        std::array<float,3> pos{},scale{};std::array<float,4> quat{};
        if(!at(m,ghost,0x110,pos,e)||!at(m,ghost,0x100,quat,e)||!at(m,ghost,0x11c,scale,e))return false;
        for(float v:pos)if(!std::isfinite(v))return fail(e,"error: native Scenery position is not finite");
        for(float v:quat)if(!std::isfinite(v))return fail(e,"error: native Scenery rotation is not finite");
        for(float v:scale)if(!std::isfinite(v))return fail(e,"error: native Scenery scale is not finite");
        return true;}
std::uintptr_t active=0,prop=0,blueprint=0;unsigned char flags[3]{};if(!at(m,b.cursor,0x190,active,e)||active!=b.behaviour||!at(m,b.behaviour,0x6b8,prop,e)||!at(m,b.behaviour,0x6c0,blueprint,e)||!ap(m,b.context,prop,0x1705230,e)||!ap(m,b.context,blueprint,0x16dd870,e)||!m.copy(b.behaviour+0x660,flags,3,e))return fail(e,"error: actual live ghost/owner unavailable");if(flags[0]!=(pending?0:1)||flags[1]!=(pending?1:0)||flags[2])return fail(e,"error: actual native placement flags differ");unsigned char palette=0;std::int32_t selected=0;int expected=-1;if(!at(m,b.behaviour,0x6d0,palette,e)||palette>1||(palette&&!at(m,b.behaviour,0x6cc,selected,e))||!identifier(m,b.behaviour,r,expected,e)||selected!=expected)return fail(e,"error: actual selected native type/variant differs");std::array<float,3> pos{},scale{};std::array<float,4> quat{};if(!m.copy(prop+0x110,pos.data(),12,e)||!m.copy(prop+0x100,quat.data(),16,e)||!m.copy(prop+0x11c,scale.data(),12,e))return false;const std::array<float,4> identity{0,0,0,1};const std::array<float,3> one{1,1,1};if(pos!=r.position||quat!=identity||scale!=one)return fail(e,"error: native UI snapping/pose differs from requested normal placement");return true;}
bool read_scenery_release_pose(Memory& m,const Binding& b,const Request& r,NativePose& out,std::string& e){
    out={};if(b.family!=Family::Scenery||!read_live_pose_and_pending(m,b,r,true,e))return fail(e,"error: Scenery release requires actual pending tool");
    auto read=[&](NativePose& pose){std::uintptr_t ghost=0;if(!at(m,b.behaviour,0xb58,ghost,e)||!ap(m,b.context,ghost,0x1705230,e))return false;
        return at(m,ghost,0x110,pose.position,e)&&at(m,ghost,0x100,pose.rotation,e)&&at(m,ghost,0x11c,pose.scale,e);};
    NativePose a,before;if(!read(a)||!read(before))return false;
    if(a!=before||!read_live_pose_and_pending(m,b,r,true,e))return fail(e,"error: native Scenery release pose changed during read");
    out=a;return true;
}
}

