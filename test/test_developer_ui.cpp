#include "developer_staff/ui_capture.h"
#include "developer_staff/owned_ui_entry.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "mt2mp/core/hash.h"
#include <iostream>
#include <map>
#include <stdexcept>
namespace staff=mt2mp::game::developer_staff;
namespace {
unsigned checks=0;
void check(bool ok,const char* cause){++checks;if(!ok)throw std::runtime_error(cause);}
struct Calls {
    unsigned originals=0,admissions=0,ids=0,retentions=0;bool allowed=true,retained=true,throws=false;
    bool original(){++originals;return false;}
    bool admit(staff::UiIntent,std::string& e){++admissions;if(throws)throw std::runtime_error("original native owner read failed");if(!allowed)e="actual native owner changed";return allowed;}
    bool uuid(std::string& id,std::string&){++ids;id="11111111-1111-4111-8111-111111111111";return true;}
    bool retain(staff::UiIntent,const std::string&,std::string& e){++retentions;if(!retained)e="Controller already has one pending action";return retained;}
};
void intents(){staff::IntentReceipt receipt;
    for(auto intent:{staff::UiIntent::Hire,staff::UiIntent::Fire}){Calls c;check(staff::capture_ui(true,true,intent,c,receipt)&&receipt.suppressed&&receipt.captured&&c.originals==0&&c.admissions==1&&c.ids==1&&c.retentions==1,"active actual Hire/Fire UI was not captured before original cash");}
    {Calls c;check(!staff::capture_ui(false,true,staff::UiIntent::Hire,c,receipt)&&c.originals==1&&!c.admissions&&!receipt.suppressed,"offline original UI did not run once");}
    {Calls c;check(!staff::capture_ui(true,false,staff::UiIntent::Hire,c,receipt)&&c.originals==1&&!c.admissions&&!receipt.suppressed,"foreign worker original UI did not run once");}
    {Calls c;check(!staff::capture_ui(true,true,staff::UiIntent::Other,c,receipt)&&c.originals==1&&!c.admissions&&!receipt.suppressed,"Research/Commit/other original UI was swallowed");}
    {Calls c;c.allowed=false;check(staff::capture_ui(true,true,staff::UiIntent::Hire,c,receipt)&&receipt.suppressed&&!receipt.captured&&receipt.cause=="actual native owner changed"&&!c.originals&&!c.ids,"failed owner guard ran Client native Hire or lost cause");}
    {Calls c;c.retained=false;check(staff::capture_ui(true,true,staff::UiIntent::Fire,c,receipt)&&!receipt.captured&&!c.originals&&receipt.cause.find("pending")!=std::string::npos,"pending intent fell through into local Fire");}
    {Calls c;c.throws=true;check(staff::capture_ui(true,true,staff::UiIntent::Hire,c,receipt)&&!receipt.captured&&!c.originals&&receipt.cause.find("original native owner read failed")!=std::string::npos,"C++ owner exception escaped into native UI or became success");}
    {Calls c;check(staff::capture_ui(true,true,staff::UiIntent::Unreadable,c,receipt)&&receipt.suppressed&&!receipt.captured&&!c.originals&&!c.admissions&&!receipt.cause.empty(),"unreadable cooperative UI record ran native original");}
}
struct Code {
    static constexpr uintptr_t target=0x10000,trampoline=0x20000,detour=0x30000;
    staff::UiEntryOwner owner{target,detour,trampoline,true,true};
    std::map<uintptr_t,unsigned char> bytes;std::vector<unsigned char> original;std::string digest;uintptr_t unreadable=0;
    template<class T>void put(uintptr_t p,T value){const auto* data=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[p+i]=data[i];}
    Code():original(48){for(size_t i=0;i<original.size();++i)original[i]=static_cast<unsigned char>((i*11+7)&255);std::copy(staff::ui_prefix.begin(),staff::ui_prefix.end(),original.begin());digest=mt2mp::sha256(original);
        for(size_t i=0;i<original.size();++i)bytes[target+i]=original[i];bytes[target]=0xe9;put(target+1,int32_t(trampoline+20-(target+5)));
        for(size_t i=0;i<6;++i)bytes[trampoline+i]=staff::ui_prefix[i];const unsigned char jump[]={0xff,0x25,0,0,0,0};for(size_t i=0;i<6;++i){bytes[trampoline+6+i]=jump[i];bytes[trampoline+20+i]=jump[i];}put(trampoline+12,target+6);put(trampoline+26,detour);
    }
    bool copy(uintptr_t p,void* out,size_t n,std::string& e){if(p==unreadable){e="original physical hook read failed";return false;}auto* data=static_cast<unsigned char*>(out);for(size_t i=0;i<n;++i){if(!bytes.contains(p+i)){e="synthetic owned code extent absent";return false;}data[i]=bytes.at(p+i);}return true;}
    bool verify(std::string& e){return staff::verify_owned_ui(owner,target,[&](auto p,auto out,auto n,auto& error){return copy(p,out,n,error);},[](const auto& b){return mt2mp::sha256(b);},e,original.size(),digest);}
};
void owners(){std::string e;
    {Code c;check(c.verify(e),"exact registered physical UI route rejected");}
    {Code c;c.owner.enabled=false;check(!c.verify(e),"unenabled UI owner accepted");}
    {Code c;c.owner.original_verified=false;check(!c.verify(e),"unverified original body accepted");}
    {Code c;c.owner.target++;check(!c.verify(e),"different physical owner accepted");}
    {Code c;c.bytes[Code::target+5]^=1;check(!c.verify(e),"sixth original entry byte ignored");}
    {Code c;c.bytes[Code::target+47]^=1;check(!c.verify(e)&&e.find("fingerprint")!=std::string::npos,"full-body suffix mutation accepted");}
    {Code c;c.bytes[Code::target+12]^=1;check(!c.verify(e),"middle body mutation accepted");}
    {Code c;c.put(Code::target+1,int32_t(0));check(!c.verify(e),"arbitrary patched entry accepted");}
    {Code c;c.bytes[Code::trampoline+2]^=1;check(!c.verify(e),"trampoline original instruction differs");}
    {Code c;c.put(Code::trampoline+12,Code::target+5);check(!c.verify(e),"incorrect original continuation accepted");}
    {Code c;c.put(Code::trampoline+26,Code::detour+1);check(!c.verify(e),"foreign relay detour accepted");}
    {Code c;c.unreadable=Code::trampoline;check(!c.verify(e)&&e=="original physical hook read failed","physical read cause lost");}
    {Code c;check(!staff::verify_owned_ui(c.owner,Code::target,[&](auto p,auto out,auto n,auto& error){return c.copy(p,out,n,error);},[](const auto&)->std::string{throw std::runtime_error("original hash provider failure");},e,c.original.size(),c.digest)&&e.find("original hash provider failure")!=std::string::npos,"hash failure became owned-hook success");}
    {Code c;check(!staff::verify_owned_ui(c.owner,Code::target,[&](auto p,auto out,auto n,auto& error){return c.copy(p,out,n,error);},[](const auto& b){return mt2mp::sha256(b);},e,c.original.size()),"synthetic body accepted production fingerprint");}
}
}
int main(){try{intents();owners();std::cout<<checks<<" actual UI capture/physical-owner cases passed\n";return 0;}catch(const std::exception& ex){std::cerr<<"error: "<<ex.what()<<'\n';return 1;}}
