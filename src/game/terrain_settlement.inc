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
    const bool scoped=active&&uintptr_t(map)==active->map&&caller==native_image+0x339951;
    if(scoped&&active->boxes.size()<16){PublicationBox row;row.exception=publication_box(box,row);active->boxes.push_back(row);}
    original_published(map,box,scoped?0.0f:duration);
    if(scoped)++active->publications;
}
__declspec(noinline) void published_entry(void* map,const void* box,float duration){published_call(map,box,duration,uintptr_t(_ReturnAddress()));}
template<class T>T read(region::Runtime& r,uintptr_t address){T value{};std::string error;require(region::at(r,address,0,value,error),"Terrain settlement read: "+error);return value;}
uint32_t native_call(uintptr_t image,unsigned call,uintptr_t receiver,uintptr_t argument=0,uint8_t flag=0){
    __try{
        switch(call){
        case 0:reinterpret_cast<void(*)(void*,const void*)>(image+0x339700)(reinterpret_cast<void*>(receiver),reinterpret_cast<void*>(argument));break;
        case 1:reinterpret_cast<void(*)(void*,uint8_t)>(image+0x337060)(reinterpret_cast<void*>(receiver),flag);break;
        case 2:reinterpret_cast<void(*)(void*,float)>(image+0x336f70)(reinterpret_cast<void*>(receiver),0.0f);break;
        case 3:reinterpret_cast<void(*)(void*,bool)>(image+0x4424c0)(reinterpret_cast<void*>(receiver),flag);break;
        case 4:reinterpret_cast<void(*)(void*,const void*)>(image+0x9f10d0)(reinterpret_cast<void*>(receiver),reinterpret_cast<void*>(argument));break;
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
        map=read<uintptr_t>(r,r.image+0x13df8c0);require(map&&read<uintptr_t>(r,map)==r.image+0x1742860,"Terrain settlement Map owner differs");
        require(read<int32_t>(r,map+0x3d4)==7,"Terrain settlement Map is not fully loaded");
        data=read<uintptr_t>(r,map+0x1a8);gpu=read<uintptr_t>(r,data+8);require(data&&gpu,"Terrain settlement MapData/GPU owner missing");
        tasks=read<uintptr_t>(r,r.image+0x13df340);game=read<uintptr_t>(r,r.image+0x13df8f0);held();
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
                    require(road&&read<uintptr_t>(r,road)==r.image+0x1743f10&&read<int32_t>(r,road+0x21c)==int32_t(type),"Terrain settlement Path type/address point differs");
                    if(unique.insert(road).second){const auto uid=read<uint64_t>(r,road+0x18);require(uid!=0,"Terrain settlement Path UID missing");roads.push_back(road);road_uids.emplace(road,uid);}
                }
            }
        }
    }
    uint64_t now(){return GetTickCount64();}
    void held(){
        require(action_session::tick_commands::authority::active()&&GetCurrentThreadId()==r.thread&&
            reinterpret_cast<bool(*)()>(r.image+0xa0bc80)(),"Terrain settlement requires exact ordered native main thread");
        require(context&&context()==graphics,"Terrain settlement native GL context changed");
        require(read<uintptr_t>(r,r.image+0x13df8c0)==map&&read<uintptr_t>(r,map+0x1a8)==data&&read<uintptr_t>(r,data+8)==gpu,"Terrain settlement World/GPU changed");
        require(read<uintptr_t>(r,r.image+0x13df8f0)==game&&read<uintptr_t>(r,r.image+0x13df340)==tasks&&game&&read<uintptr_t>(r,game)==r.image+0x1743740&&tasks&&read<uintptr_t>(r,game+0x68)==tasks&&
            read<uintptr_t>(r,tasks)==r.image+0x16fdaf0,"Terrain settlement Game/Task owner changed");
        const auto paused=read<uint8_t>(r,tasks+0xf8);const auto one=read<int32_t>(r,tasks+0x90),two=read<int32_t>(r,tasks+0xa8);
        require(paused==1&&one==0&&two==0,"Terrain settlement scheduler not held: paused="+std::to_string(paused)+" running_type1="+std::to_string(one)+" running_type2="+std::to_string(two));
        require(!reinterpret_cast<bool(*)()>(r.image+0x265070)(),"Terrain settlement original EditManager remains locked");
    }
    State state(){
        State s;s.boxes=read<int32_t>(r,map+0x3c8);s.gpu=read<int32_t>(r,gpu+0xf48);const auto queued=read<uint8_t>(r,gpu+0xf40);
        require(s.boxes>=0&&s.boxes<=1000000&&s.gpu>=2&&s.gpu<=5&&queued<=1,"Terrain settlement invalid native pending state");s.queued=queued!=0;
        for(auto road:roads){require(read<uintptr_t>(r,road)==r.image+0x1743f10&&read<uint64_t>(r,road+0x18)==road_uids.at(road),"Terrain settlement Path lifetime changed");const auto flag=read<uint8_t>(r,road+0x474);require(flag<=1,"Terrain settlement Path Plot flag invalid");s.plots=s.plots||flag;}
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
        {0x508390,96,"2c3e6f9a181f6df74d573a59ca5915fc7776a2c7e73e854ab0ad4a0003224192"},
        {0x508330,96,"f855ee1f7b4fb3ecc3666535c7966e661cbe84b3e3277471044292d27d3641bb"},
        {0x32abb0,160,"fdf20902f36cb864884b2356018ee6e84103b8120ecd4fb1ddb2f1a8278b7dfa"},
        {0x337440,1296,"8eb056729dd4d48406f3e00660e9bd56d50bb78a79b235100871124a203ae352"},
        {0x337950,1536,"6bb4d6b44e46697f6ba864997719542fbc0c0981c2d737ba29802f90da6b0b77"},
        {0xa26210,160,"2112e7bb3e51e9c9a77cd566245e37b67302625afdd9105ef593b6f08183882f"},
        {0x9f5ae0,32,"14777700b3499ab6f56bb8ac6471c19c2a0d92033df8a1cd4de9f3822ecdc07f"},
        {0x32c1e0,384,"df7056ebd3d2e00588047aa9a7535a0207c3823520786c5c48e0aff66d3ffe0d"},
        {0x339910,128,"1dc61d947733227a5f22bdec430e30f90ef014483df52955c0c18c1acba7b32c"},
        {0x34fa60,240,"ea872ac173da524fccedb9f8c4c9deeffe3d3a8f69905b3cf909cfaf9894a545"},
        {0x2cc120,336,"6d3ff7213c387fb0203aae02c16ae7356d41860ea9c24ca86a0ac5eb56805e9c"},
        {0x449330,16,"357bdcb217be20ac5e07d9c64de9d6e5b5ac373656a9fc335b395e2c2a92a241"},
        {0x449030,768,"a9fe8e6334060cee8f09bad0734be2cf41a9e2a5465cf901b53c66a81dd57bcb"},
        {0x32e890,5264,"5e6f092c80fc48bbb0fedc56b386af43f47040ccf3829989c25d9e97a2852db7"},
        {0x339700,16,"36ff4fddc4b61d935d61c31a840e5c660e87b76002c227647171ebd6c7c90914"},
        {0x337060,432,"9339e27827ee8c3cfd6c60c7401d6b37251bcbf9410b740e2a61762eb67e838e"},
        {0x336f70,240,"e5d2ab70e87d8560cb9377eff2f4aa90f304e9a7964711d9f80485643cc37b89"},
        {0x33c690,1168,"8e16898e542a4079d6c0f4a9979086d2d6c18b844e3798e88e9010f9a86e7035"},
        {0x4424c0,4768,"201a69303e149be7f9a524d0504d2b4851fb8d4d6b3cc9db87a729dec19ba112"},
        {0x9f10d0,224,"b287cec39ac2c9a40cc398570a528bd8cb70396e488d24c6f157b8908bd410d7"},
        {0x9ccaf0,624,"94b30f55f7d0c628acbbd8d81f5a14a913dab3d1839ccc3dbafe46d1727c8ce4"},
        {0x9ccd60,688,"643bb1f2b620ab1c7f7f17ea4fc42d3a5e127f3fb5753a1ef710fa9be1f7ed86"},
    };
    for(const auto& pin:pins){std::string bytes(pin.bytes,'\0'),hash;
        if(!r.memory.copy(r.image+pin.rva,bytes.data(),bytes.size(),error)||!region::sha256(bytes,hash,error))return false;
        if(hash!=pin.sha){error="Terrain settlement pinned native body differs RVA="+std::to_string(pin.rva);return false;}
    }
    native_image=r.image;const auto result=MH_CreateHook(reinterpret_cast<void*>(r.image+0x32c1e0),reinterpret_cast<void*>(&published_entry),reinterpret_cast<void**>(&original_published));
    if(result!=MH_OK||!original_published){error="Terrain settlement Map publication hook failed status="+std::to_string(result);return false;}
    struct Route{uintptr_t rva;};const Route routes[]{{0x32c1e0}};
    if(!mt2mp::hooks::enable_prepared_routes(r.image,routes,"Terrain settlement publication",error))return false;
    installed=true;return true;
}
}
