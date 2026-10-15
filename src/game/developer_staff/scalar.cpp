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
constexpr std::array<Body,6> bodies={{{0x967370,416,"2a6e4111f91e2c3cecb2aa35b99ed8d71f22a75f8300694f7d9ba6b086c01daa"},
    {0x9676a0,160,"33000088d5ae9e353738b283b76f13a641243aa996c7952714c57d6a07cc170f"},
    {0x1113d80,272,"dee54a7232d3aef27b73436213be00e802ba3cb997d4fb09a5f41f38468fafcc"},
    {0xd258b0,32,"92284a2bddcc708ff1be3237cb4371f99dcd33fb3697874d819255b81af22ab0"},
    {0x8a1c70,128,"f803a22293cf408d76ed09a49c80764c6872b2ff069af07ea247a4c66365ce75"},
    {0xd0fd10,672,"04893cac0714f4ce3a4f95f882fd1851e214348a695754ed67021050809cacfc"}}};
}
bool apply(const Read& read,uintptr_t image,uintptr_t actor,Field field,int32_t value,Receipt& out,std::string& e){
    out={};e.clear();
    if(!read||!image||image>UINTPTR_MAX-0x1800000||!actor||
       (field!=Field::State&&field!=Field::Active)||(field==Field::State&&!state_name(value))||(field==Field::Active&&(value<0||value>1)))
        return fail(e,"Developer scalar binding/field/value invalid");
    const auto property=image+(field==Field::State?0x13c8fc0:0x13c8f40);
    const auto expected_ap=image+(field==Field::State?0x16c9a40:0x16d2410);
    const uintptr_t expected_offset=field==Field::State?0x4c4:0x4d9;
    uintptr_t ap=0,property_ap=0,offset=0,job=0,carried=0;
    if(!at(read,actor,0,ap,e)||!at(read,actor,0x4c8,job,e)||!at(read,actor,0x4d0,carried,e)||!at(read,property,0,property_ap,e)||!at(read,property,0x38,offset,e))return false;
    if(ap!=image+0x17422e0||property_ap!=expected_ap||offset!=expected_offset||job||carried)
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
