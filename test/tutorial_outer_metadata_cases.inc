#include <set>
#include <optional>
#include <vector>
namespace staff_metadata {
struct Developer {uintptr_t address=0;int32_t id=0,job=0,row=-1;uint8_t employed=0,on_map=0,wanted=0;bool operator==(const Developer&)const=default;};
struct Source {uintptr_t address=0;int32_t job=0;};
struct Snapshot {
    struct {void* state=nullptr;} origin;
    struct Row {bool project_present=true,purchased=true,deployed=false;};
    struct Creation {std::vector<Source> developers;};
    std::optional<Row> row;std::optional<Creation> creation;
    uintptr_t project=1,lounge=0;
};
namespace tutorial_development {using StartingpointSnapshot=Snapshot;}
struct Owners {Snapshot snapshot;std::vector<Developer> developers;int32_t count=0;};
uintptr_t image=0x140000000;uint64_t reads=0;uintptr_t blocked=0;
template<class T>bool at(uintptr_t p,size_t off,T& out,std::string& e){
    ++reads;if(p+off==blocked)return fail(e,"original Staff read failure");return memory.copy(p+off,&out,sizeof out,e);
}
bool type(uintptr_t p,uintptr_t rva,std::string& e){uintptr_t v=0;return at(p,0,v,e)&&(v==image+rva||fail(e,"original Staff class differs"));}
#include "staff_owner_reader.inc"
}
void outer_metadata_cases(){
    using namespace staff_metadata;
    auto* owned=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x20000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));need(owned!=nullptr,"owned fixed50 fixture");
    struct Release {void* p;~Release(){VirtualFree(p,0,MEM_RELEASE);}} release{owned};
    const auto base=reinterpret_cast<uintptr_t>(owned);
    auto put=[]<class T>(uintptr_t p,T value){std::memcpy(reinterpret_cast<void*>(p),&value,sizeof value);};
    Snapshot snapshot;snapshot.origin.state=owned;snapshot.lounge=base+0x800;snapshot.row=Snapshot::Row{};snapshot.creation=Snapshot::Creation{};
    put(base+0x1c0,base+0x400);put(base+0x44c,int32_t(1));put(snapshot.lounge+0x310,int32_t(1));
    for(size_t i=0;i<50;++i){const auto p=base+0x1000+i*0x600;put(p,image+0x17422e0);put(p+0x4c0,int32_t(i));put(p+0x4d8,uint8_t(i==0));put(p+0x4d9,uint8_t(i==0));put(p+0x368,uint8_t(0));snapshot.creation->developers.push_back({p,0});}
    std::string e;nlohmann::json result;development::StartingpointSnapshot dev;
    auto chain=[&](bool share,Owners& out){return development::read_startingpoint(dev,result,e,true,share)&&development::read_research("bridges",dev,result,e,true,share)&&read_owners(snapshot,out,e,share);};
    Owners expected,actual;uint64_t plain=0,shared=0;
    {timing::PageReadScope outer;auto before=queries();need(chain(false,expected),"default adjacent readers");plain=queries()-before;}
    const auto before_reads=reads;
    {timing::PageReadScope outer;auto before=queries();SetLastError(123);need(chain(true,actual),"opt-in adjacent readers");shared=queries()-before;need(GetLastError()==123,"opt-in LastError unchanged");}
    need(actual.developers==expected.developers&&actual.count==expected.count,"exact fixed50 values preserved");
    need(reads-before_reads==253,"all253 actual Staff reads preserved");need(shared<plain,"shared pure chain reduces real OS queries");
    std::cout<<"adjacent Development/Staff metadata OS queries "<<plain<<" -> "<<shared<<", Staff live reads253\n";
    {timing::PageReadScope outer;need(chain(true,actual),"prime shared chain");put(base+0x1000+0x368,uint8_t(1));need(chain(true,actual)&&actual.developers[0].on_map==1,"live changed Staff byte observed within shared metadata");put(base+0x1000+0x368,uint8_t(0));
        blocked=base+0x1000+0x4d8;need(!chain(true,actual)&&e=="original Staff read failure","original Staff cause preserved");blocked=0;}
    need(timing::page_read_scope==nullptr,"shared scope closes after failure");
    {auto before=queries();need(chain(true,actual),"opt-in without parent falls back to independent scopes");need(queries()-before==plain,"no parent does not retain metadata");}
    {timing::PageReadScope outer;need(chain(true,actual),"prime before original boundary");
        {timing::PageReadScope original(timing::PageBoundary::OriginalCall);DWORD old=0;need(VirtualProtect(owned,0x20000,PAGE_NOACCESS,&old)!=0,"owned page made unreadable at original boundary");}
        need(!read_owners(snapshot,actual,e,true)&&e=="error: native placement UI page unreadable","original boundary invalidates shared metadata");DWORD old=0;need(VirtualProtect(owned,0x20000,PAGE_READWRITE,&old)!=0,"owned page restored");}
    {timing::PageReadScope outer;injected_exception=true;bool caught=false;try{chain(true,actual);}catch(const std::runtime_error& x){caught=std::string(x.what())=="original reader exception";}injected_exception=false;need(caught,"shared reader preserves original exception");}
    need(timing::page_read_scope==nullptr,"no retained metadata after shared exception");
}
