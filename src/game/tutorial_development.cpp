#include "live_host_state/memory_query_timing.h"
#include "tutorial_development.h"
#include "native_open_runtime.h"
#include "placement_ui_probe/ui_probe.h"
#include <windows.h>
#include "mt2mp/core/hash.h"

namespace mt2mp::game::tutorial_development {
namespace {
DWORD research_call(uintptr_t image,uintptr_t project,std::array<bool,4>& out){
    using Getter=bool(*)(const void*);
    __try{
        const auto self=reinterpret_cast<const void*>(project);
        out[0]=reinterpret_cast<Getter>(image+0x266310)(self);
        out[1]=reinterpret_cast<Getter>(image+0x265cf0)(self);
        out[2]=reinterpret_cast<Getter>(image+0x265e30)(self);
        out[3]=reinterpret_cast<Getter>(image+0x266110)(self);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
class NativePort final:public detail::Port {
    placement_ui_probe::ProcessMemory memory_;
    bool research_guarded_=false;
public:
    bool current(detail::Origin& out,std::string& error)override{return native_open_runtime::read_current_world(out,error);}
    bool image(uintptr_t& out,std::string& error)override{
        out=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if(out)return true;error="development native image unavailable";return false;
    }
    bool research_flags(uintptr_t image,uintptr_t project,std::array<bool,4>& out,std::string& error)override{
        if(!research_guarded_){
            struct Body{uintptr_t rva;size_t size;const char* sha;};
            constexpr Body bodies[]={
                {0x266310,336,"6afb8ed85e9b72c77a87deb21d4c76e4e0ce105158787ed1931f86b9a05ce3f8"},
                {0x265cf0,144,"d9bb747b0adf33a3bb91bc7e5a3a2aa21029031b8f6d75a625ccb1166b4ddeac"},
                {0x265e30,256,"3b2392e9b71807c2d099194862ae4390030fb1d993ad69a9cae554e6c9b4971c"},
                {0x266110,512,"83b2964a45832ecc8cf9b35dcfb344a4d79d4cd4fcfb9f13bf2fc1b33cd48617"},
                {0x534b00,16,"ec3ba4db05df5007f385f7c2a48aeaa6277f02df8ef9d648ab4f80d07891c55d"},
                {0x534b90,112,"f3df4791df2c186a0961c6b4c5b943e2a0b3c81679e3bf7341d66fb6840a22aa"},
                {0x55200,32,"f743600a807eb2caf8a0e9f7bed2dae3fa941212661a1bd6505b313b2300c040"}};
            for(const auto& body:bodies){std::vector<unsigned char> bytes(body.size);
                if(!memory_.copy(image+body.rva,bytes.data(),bytes.size(),error))return false;
                if(mt2mp::sha256(bytes)!=body.sha){error="development research original getter body differs RVA="+std::to_string(body.rva);return false;}}
            research_guarded_=true;
        }
        live_host_state::apply_timing::PageReadScope getter_scope(live_host_state::apply_timing::PageBoundary::OriginalCall);
        const auto fault=research_call(image,project,out);
        if(fault){error="development research original getter exception="+std::to_string(fault);return false;}return true;
    }
    bool copy(uintptr_t address,void* out,size_t count,std::string& error)override{return memory_.copy(address,out,count,error);}
};
}
bool read(nlohmann::json& result,std::string& error){live_host_state::apply_timing::PageReadScope pages;NativePort port;StartingpointSnapshot snapshot;return detail::observe(port,result,error,&snapshot,false,true);}
bool read_startingpoint(StartingpointSnapshot& snapshot,nlohmann::json& result,std::string& error,bool creation,bool share_metadata){live_host_state::apply_timing::PageReadScope pages(!share_metadata||!live_host_state::apply_timing::page_read_scope);NativePort port;return detail::observe(port,result,error,&snapshot,creation);}
bool read_research(const std::string& key,StartingpointSnapshot& snapshot,nlohmann::json& result,std::string& error,bool creation,bool share_metadata){
    live_host_state::apply_timing::PageReadScope pages(!share_metadata||!live_host_state::apply_timing::page_read_scope);NativePort port;return detail::observe(port,result,error,&snapshot,creation,false,key);
}

}
