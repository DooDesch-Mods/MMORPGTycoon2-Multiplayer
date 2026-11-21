#include "terrain_settlement.h"
#include <stdexcept>
namespace mt2mp::game::terrain_settlement {
namespace region=ui_region_runtime;
using Json=nlohmann::json;
using Published=void(*)(void*,const void*,float);
Published original_published=nullptr;
bool installed=false,installation_attempted=false;
uintptr_t native_image=0;
struct PublicationBox {uint8_t initialized=0;std::array<uint32_t,6> coordinates{};uint32_t exception=0;};
struct Scope {uintptr_t map=0;uint64_t publications=0;std::vector<uint8_t> animation_bytes;std::vector<PublicationBox> boxes;};
thread_local Scope* active=nullptr;
Json last_receipt=Json::object();
void require(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
uint32_t publication_box(const void* box,PublicationBox& out){
    __try{std::memcpy(&out.initialized,box,1);std::memcpy(out.coordinates.data(),static_cast<const unsigned char*>(box)+4,24);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
Json publication_boxes(const Scope& scope){
    Json rows=Json::array();for(const auto& box:scope.boxes)rows.push_back({{"initialized",box.initialized},{"coordinate_bits",box.coordinates},{"read_exception",box.exception}});
    return rows;
}
void published_call(void* map,const void* box,float duration,uintptr_t caller){
    const bool scoped=active&&uintptr_t(map)==active->map&&caller==native_image+0x33da51;
    if(scoped&&active->boxes.size()<16){PublicationBox row;row.exception=publication_box(box,row);active->boxes.push_back(row);}
    original_published(map,box,scoped?0.0f:duration);
    if(scoped)++active->publications;
}
__declspec(noinline) void published_entry(void* map,const void* box,float duration){published_call(map,box,duration,uintptr_t(_ReturnAddress()));}
template<class T>T read(region::Runtime& r,uintptr_t address){T value{};std::string error;require(region::at(r,address,0,value,error),"Terrain settlement read: "+error);return value;}
uint32_t native_call(uintptr_t image,unsigned call,uintptr_t receiver,uintptr_t argument=0,uint8_t flag=0){
    __try{
        switch(call){
        case 0:reinterpret_cast<void(*)(void*,const void*)>(image+0x33d800)(reinterpret_cast<void*>(receiver),reinterpret_cast<void*>(argument));break;
        case 1:reinterpret_cast<void(*)(void*,uint8_t)>(image+0x33b160)(reinterpret_cast<void*>(receiver),flag);break;
        case 2:reinterpret_cast<void(*)(void*,float)>(image+0x33b070)(reinterpret_cast<void*>(receiver),0.0f);break;
        case 3:reinterpret_cast<void(*)(void*,bool)>(image+0x4472a0)(reinterpret_cast<void*>(receiver),flag);break;
        case 4:reinterpret_cast<void(*)(void*,const void*)>(image+0x9fb520)(reinterpret_cast<void*>(receiver),reinterpret_cast<void*>(argument));break;
        }
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
void call(region::Runtime& r,unsigned operation,uintptr_t receiver,uintptr_t argument=0,uint8_t flag=0){
    const auto code=native_call(r.image,operation,receiver,argument,flag);require(!code,"Terrain settlement original call="+std::to_string(operation)+" SEH="+std::to_string(code));
}
uint32_t consume_native(uintptr_t map,bool finished=false){
    __try{if(finished)*reinterpret_cast<uint8_t*>(map+0x3d0)=0;else *reinterpret_cast<int32_t*>(map+0x3c8)=0;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
using Context=void*(WINAPI*)();
struct NativePort {
    region::Runtime& r;uintptr_t map=0,data=0,gpu=0,tasks=0,game=0;Context context=nullptr;void* graphics=nullptr;
    std::vector<uintptr_t> roads;
    std::map<uintptr_t,uint64_t> road_uids;
    uint64_t started=GetTickCount64();
    NativePort(region::Runtime& owner):r(owner){
        const auto gl=GetModuleHandleW(L"opengl32.dll");context=gl?reinterpret_cast<Context>(GetProcAddress(gl,"wglGetCurrentContext")):nullptr;
        require(context&&(graphics=context()),"Terrain settlement requires current native GL context");
        map=read<uintptr_t>(r,r.image+0x13eb8c0);require(map&&read<uintptr_t>(r,map)==r.image+0x1748c00,"Terrain settlement Map owner differs");
        require(read<int32_t>(r,map+0x3d4)==7,"Terrain settlement Map is not fully loaded");
        data=read<uintptr_t>(r,map+0x1a8);gpu=read<uintptr_t>(r,data+8);require(data&&gpu,"Terrain settlement MapData/GPU owner missing");
        tasks=read<uintptr_t>(r,r.image+0x13eb340);game=read<uintptr_t>(r,r.image+0x13eb8f0);held();
        region_saved_identity::Index index;std::string error;
        auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& why){return region::at(r,object,offset,value,why);};
        const bool captured=region_saved_identity::capture(r.image,map,map+0x1c0,index,at,error);require(captured,"Terrain settlement complete Region census: "+error);
        std::set<uintptr_t> unique;
        for(const auto& [id,object]:index.saved){
            require(GetTickCount64()-started<5000,"Terrain settlement Region census exceeded 5000ms");
            (void)id;const auto manager=read<uintptr_t>(r,object+0x9a0);
            require(manager&&read<uintptr_t>(r,manager+0x6a0)==object,"Terrain settlement Path manager owner differs");
            for(unsigned type=0;type<11;++type){const auto offset=size_t(type)*0x30;
                const auto count=read<int32_t>(r,manager+0x30+offset),capacity=read<int32_t>(r,manager+0x34+offset);
                const auto array=read<uintptr_t>(r,manager+0x28+offset);
                require(count>=0&&capacity>=count&&capacity<=1000000&&(!count||array),"Terrain settlement path array invalid");
                for(int32_t i=0;i<count;++i){const auto road=read<uintptr_t>(r,array+size_t(i)*8);
                    require(road&&read<uintptr_t>(r,road)==r.image+0x174a2b0&&read<int32_t>(r,road+0x21c)==int32_t(type),"Terrain settlement Path type/address point differs");
                    if(unique.insert(road).second){const auto uid=read<uint64_t>(r,road+0x18);require(uid!=0,"Terrain settlement Path UID missing");roads.push_back(road);road_uids.emplace(road,uid);}
                }
            }
        }
    }
    uint64_t now(){return GetTickCount64();}
    void held(){
        require(action_session::tick_commands::authority::active()&&GetCurrentThreadId()==r.thread&&
            reinterpret_cast<bool(*)()>(r.image+0xa168e0)(),"Terrain settlement requires exact ordered native main thread");
        require(context&&context()==graphics,"Terrain settlement native GL context changed");
        require(read<uintptr_t>(r,r.image+0x13eb8c0)==map&&read<uintptr_t>(r,map+0x1a8)==data&&read<uintptr_t>(r,data+8)==gpu,"Terrain settlement World/GPU changed");
        require(read<uintptr_t>(r,r.image+0x13eb8f0)==game&&read<uintptr_t>(r,r.image+0x13eb340)==tasks&&game&&read<uintptr_t>(r,game)==r.image+0x1749ae0&&tasks&&read<uintptr_t>(r,game+0x68)==tasks&&
            read<uintptr_t>(r,tasks)==r.image+0x1703e90,"Terrain settlement Game/Task owner changed");
        const auto paused=read<uint8_t>(r,tasks+0xf8);const auto one=read<int32_t>(r,tasks+0x90),two=read<int32_t>(r,tasks+0xa8);
        require(paused==1&&one==0&&two==0,"Terrain settlement scheduler not held: paused="+std::to_string(paused)+" running_type1="+std::to_string(one)+" running_type2="+std::to_string(two));
        require(!reinterpret_cast<bool(*)()>(r.image+0x268170)(),"Terrain settlement original EditManager remains locked");
    }
    State state(){
        State s;s.boxes=read<int32_t>(r,map+0x3c8);s.gpu=read<int32_t>(r,gpu+0xf48);const auto queued=read<uint8_t>(r,gpu+0xf40);
        require(s.boxes>=0&&s.boxes<=1000000&&s.gpu>=2&&s.gpu<=5&&queued<=1,"Terrain settlement invalid native pending state");s.queued=queued!=0;
        for(auto road:roads){require(read<uintptr_t>(r,road)==r.image+0x174a2b0&&read<uint64_t>(r,road+0x18)==road_uids.at(road),"Terrain settlement Path lifetime changed");const auto flag=read<uint8_t>(r,road+0x474);require(flag<=1,"Terrain settlement Path Plot flag invalid");s.plots=s.plots||flag;}
        return s;
    }
    void consume_boxes(){
        const auto count=read<int32_t>(r,map+0x3c8),capacity=read<int32_t>(r,map+0x3cc);const auto array=read<uintptr_t>(r,map+0x3c0);
        require(count>0&&count<=capacity&&capacity<=1000000&&array,"Terrain settlement dirty box array invalid");
        std::array<unsigned char,28> box{};
        for(int32_t i=0;i<count;++i){const auto next=array+size_t(i)*28;
            require(read<uint8_t>(r,next)<=1,"Terrain settlement dirty box initialization invalid");
            const auto coordinates=read<std::array<float,6>>(r,next+4);
            require(std::all_of(coordinates.begin(),coordinates.end(),[](float value){return std::isfinite(value);}),"Terrain settlement nonfinite dirty box");
            call(r,4,uintptr_t(box.data()),next);
        }
        const auto exception=consume_native(map);require(!exception,"Terrain settlement dirty queue consume SEH="+std::to_string(exception));
        call(r,0,data,uintptr_t(box.data()));
        // The native Map forwards this byte without normalizing it to C++ bool.
        const auto animate=read<uint8_t>(r,map+0x3d0);active->animation_bytes.push_back(animate);
        call(r,1,data,0,animate);
        const auto finished=consume_native(map,true);require(!finished,"Terrain settlement dirty animation clear SEH="+std::to_string(finished));
    }
    uint64_t plot_queued(){uint64_t count=0;for(auto road:roads)if(read<uint8_t>(r,road+0x474)){
        const auto flag=read<uint8_t>(r,road+0x478);require(flag<=1,"Terrain settlement Plot argument invalid");call(r,3,road,0,flag!=0);++count;
    }return count;}
    void pump(){call(r,2,data);}
    void yield(){Sleep(0);}
};
bool enable(region::Runtime& r,std::string& error);
bool run(region::Runtime& r,const std::string& action,bool ordered,Json& output,std::string& error){
    if(!ordered)return true;
    const auto start=GetTickCount64();Scope scope;output={{"attempted",true},{"confirmed",false},{"action_uuid",action}};
    struct Record{Json& value;Scope& scope;~Record(){value["publication_boxes"]=publication_boxes(scope);value["publication_boxes_truncated"]=scope.publications>scope.boxes.size();last_receipt=value;}} record{output,scope};
    try{
        require(installed&&!active,"Terrain settlement hook unavailable or reentered");NativePort port(r);scope.map=port.map;active=&scope;
        struct Clear{~Clear(){active=nullptr;}} clear;
        const auto receipt=drain(port);port.held();require(!receipt.batches||scope.publications,"Terrain settlement lacks exact original SetData publication acknowledgement");
        output={{"attempted",true},{"confirmed",true},{"action_uuid",action},{"elapsed_ms",GetTickCount64()-start},{"pumps",receipt.pumps},{"dirty_batches",receipt.batches},{"plots",receipt.plots},{"publications",scope.publications},{"animation_bytes",scope.animation_bytes}};
        return true;
    }catch(const std::exception& cause){active=nullptr;output["elapsed_ms"]=GetTickCount64()-start;output["publications"]=scope.publications;output["animation_bytes"]=scope.animation_bytes;error=std::string("Terrain settlement failed; no replay: ")+cause.what();output["cause"]=error;return false;}
    catch(...){active=nullptr;output["elapsed_ms"]=GetTickCount64()-start;error="Terrain settlement raised unknown exception; no replay";output["cause"]=error;return false;}
}
bool enable(region::Runtime& r,std::string& error){
    if(installed)return true;
    if(installation_attempted){error="Terrain settlement hook installation already attempted";return false;}installation_attempted=true;
    struct Pin{uintptr_t rva;size_t bytes;const char* sha;};
    const Pin pins[]{
        {0x50e4e0,96,"e17612808d32cdd246b9c9b83d29458be5197a1fed9fe72e2503dd07b527dc3b"},
        {0x50e480,96,"514b36baee0c03ad84a8e99ab694ea32df35f6dbf4da37c6860cea4c1fc5a4ea"},
        {0x32ec40,160,"a06c79c73ee2d7f9883d1f7b7b59222774b09272c785875140011ca338096091"},
        {0x33b540,1296,"d73e70838dc5a74df8d90e3f42eba81d2ca048bd557624d65de581ecc8844db0"},
        {0x33ba50,1536,"05db612e83b734488fdccfcfffbd0ddddadffcdbb7d41fa281a54e43838d3d2c"},
        {0xa312c0,160,"a22f5616015cda23588a12f1f46854586d5e9470cad39510ba70a07f601bdd66"},
        {0x9fff30,32,"3b236f08ecce59ddb62437851c30d3532b289235872bf311144b00ce55576b6b"},
        {0x330270,384,"cd9f571d142fbcafdeacc12b1b90709b396352067d70eeab47dc6c7ed5d31a07"},
        {0x33da10,128,"f7e7b6aeff408201f53453b92f7b24cae7cf12cf7c81fc6dad6d525ad26af0cb"},
        {0x354160,240,"93c64b4cb1edc8f86404a50ad868dc6bbcc78a1e0a5ebe4c2581c04976670ec7"},
        {0x2cf830,336,"8b8d6e262356cde238a346cd87188476b4917492616968fa83fbf2aa153bf582"},
        {0x44e340,16,"357bdcb217be20ac5e07d9c64de9d6e5b5ac373656a9fc335b395e2c2a92a241"},
        {0x44e040,768,"4b1d1cd11993f132a102249b9e633b1d5cc2d2c2896776d953293812c52e54dc"},
        {0x332920,5264,"55ca106d58de5c32fdc1fc661706a1ecf3662ef4144040141475375e8dbb43c4"},
        {0x33d800,16,"56ae262b65339db3b9606816764be050477f096a3828534675e93a7ae80b1106"},
        {0x33b160,432,"eabe73ea6dd3df96545283a9bdeaadb75ace698175c2349cc430961d293c24b0"},
        {0x33b070,240,"79f6e32d4023be3dab2da003ef361a4ce7262b7b08708429584bf96446992e1a"},
        {0x340790,1168,"4ad5ed7c82a5b9abc997ca92c81b8e43ffe204ed8aa7ae9f33d5ade39c6ee994"},
        {0x4472a0,4768,"80d4c10e59967e32422d1d72db3eaa3beac14642f23e4fd0a78a8579a1a91f50"},
        {0x9fb520,224,"b287cec39ac2c9a40cc398570a528bd8cb70396e488d24c6f157b8908bd410d7"},
        {0x9d66f0,624,"c29f1673139a0a548b109f2db896cac49ad2589d437a5be1ba0adf40ddb82a34"},
        {0x9d6960,688,"878d4b55fce6a9c2b37e028a04189ca9104655b37fb398ea151be4b55c9b3d20"},
    };
    for(const auto& pin:pins){std::string bytes(pin.bytes,'\0'),hash;
        if(!r.memory.copy(r.image+pin.rva,bytes.data(),bytes.size(),error)||!region::sha256(bytes,hash,error))return false;
        if(hash!=pin.sha){error="Terrain settlement pinned native body differs RVA="+std::to_string(pin.rva);return false;}
    }
    native_image=r.image;const auto result=MH_CreateHook(reinterpret_cast<void*>(r.image+0x330270),reinterpret_cast<void*>(&published_entry),reinterpret_cast<void**>(&original_published));
    if(result!=MH_OK||!original_published){error="Terrain settlement Map publication hook failed status="+std::to_string(result);return false;}
    struct Route{uintptr_t rva;};const Route routes[]{{0x330270}};
    if(!mt2mp::hooks::enable_prepared_routes(r.image,routes,"Terrain settlement publication",error))return false;
    installed=true;return true;
}
}
