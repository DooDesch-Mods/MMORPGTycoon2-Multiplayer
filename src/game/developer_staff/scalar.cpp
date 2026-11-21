#include "scalar.h"
#include "mt2mp/core/hash.h"
#include <array>
#include <vector>
#include <exception>
namespace mt2mp::game::developer_staff::scalar {
const char* state_name(int32_t state)noexcept{
    constexpr const char* names[]={"Idle","GoingToJob","DoingJob","TransportJob","GoingHome","TechGoingHome","TechDevelop","TechDebug"};
    return state>=0&&state<8?names[state]:nullptr;
}
namespace {
bool fail(std::string& e,const std::string& why){e=why;return false;}
template<class T>bool at(const Read& read,uintptr_t owner,size_t offset,T& out,std::string& e){
    if(!owner||owner>UINTPTR_MAX-offset||owner+offset>UINTPTR_MAX-sizeof(T))return fail(e,"Developer scalar local address overflow");
    return read(owner+offset,&out,sizeof(T),e);
}
struct Body {uintptr_t rva;size_t size;const char* sha;};
constexpr std::array<Body,6> bodies={{{0x970590,416,"708dc9f67619c74b41ea34856237e98209b298b0cc72613a4cf3b729999a5976"},
    {0x9708c0,160,"182327238cd9e4972b16d4d21ec53c523fce5ef393eda38318e12cc9eb727642"},
    {0x111f720,272,"646ae2ceb1adbb4b88d9e72b93f1c5cb83b4eda2a7e81c23c411c2aec65ee3d2"},
    {0xd30ff0,32,"cfecd549f2e6fc6c689856fae8276d52e3e1b19f3f7eca1a5cb61d01400d9f6f"},
    {0x8aad90,128,"9e2324bb66601dcf36f4a52631bb4e83b6af61890ab9aca15f971d2f0d246370"},
    {0xd1b450,672,"86e506aa0a620c7a5235a38b0d338129da6efd581fdc853e6adf5153ab5d7858"}}};
}
bool apply(const Read& read,uintptr_t image,uintptr_t actor,Field field,int32_t value,Receipt& out,std::string& e){
    out={};e.clear();
    if(!read||!image||image>UINTPTR_MAX-0x1800000||!actor||
       (field!=Field::State&&field!=Field::Active)||(field==Field::State&&!state_name(value))||(field==Field::Active&&(value<0||value>1)))
        return fail(e,"Developer scalar binding/field/value invalid");
    const auto property=image+(field==Field::State?0x13d4fc0:0x13d4f40);
    const auto expected_ap=image+(field==Field::State?0x16cfde0:0x16d87b0);
    const uintptr_t expected_offset=field==Field::State?0x4c4:0x4d9;
    uintptr_t ap=0,property_ap=0,offset=0,job=0,carried=0;
    if(!at(read,actor,0,ap,e)||!at(read,actor,0x4c8,job,e)||!at(read,actor,0x4d0,carried,e)||!at(read,property,0,property_ap,e)||!at(read,property,0x38,offset,e))return false;
    if(ap!=image+0x1748680||property_ap!=expected_ap||offset!=expected_offset||job||carried)
        return fail(e,"Developer scalar exact actor/property/no-job/no-carried binding differs");
    try{
        for(const auto& body:bodies){std::vector<unsigned char> bytes(body.size);if(!read(image+body.rva,bytes.data(),bytes.size(),e))return false;
            if(mt2mp::sha256(bytes)!=body.sha)return fail(e,"Developer scalar native full body differs RVA="+std::to_string(body.rva));}
        out.exception=invoke(image,actor,field,value,&out);
        if(out.exception||out.cleanup_exception||!out.record_confirmed||!out.returned||!out.accepted||!out.cleanup_confirmed)
            return fail(e,"Developer scalar original call/cleanup unconfirmed; SEH="+std::to_string(out.exception)+" cleanup="+std::to_string(out.cleanup_exception)+"; no replay");
        int32_t actual=0;if(field==Field::State){if(!at(read,actor,expected_offset,actual,e))return false;}
        else {uint8_t byte=0;if(!at(read,actor,expected_offset,byte,e))return false;actual=byte;}
        out.readback=actual==value;
        return out.readback||fail(e,"Developer scalar original property readback differs; no replay");
    }catch(const std::exception& ex){return fail(e,std::string("Developer scalar exception; no replay: ")+ex.what());}
    catch(...){return fail(e,"Developer scalar unknown exception; no replay");}
}
}
