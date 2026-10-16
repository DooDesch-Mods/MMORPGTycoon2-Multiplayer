// Reuse the original-record test doubles while compiling the production tuple body.
#define main weapon_record_primitive_cases
#include "test_toon_weapon_records.cpp"
#undef main
#include "subscriber_lifecycle/append_calls.h"
#include "subscriber_quest_state.h"
#include <array>
#include <functional>
#include <memory>

namespace fixture {
using Toon=mt2mp::subscriber_quest_state::Toon;
decode::DecodeApi record_api;
unsigned stores=0, fail_store=0;
bool fail_cleanup=false,fail_record_api=false;
struct Copy {
    bool copy(uintptr_t address,void* out,size_t size,std::string&) {std::memcpy(out,reinterpret_cast<void*>(address),size);return true;}
} memory;
struct Pin {uintptr_t image=0;};
constexpr size_t max_frame_payload=65536;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
template<class T>bool at(uintptr_t object,size_t offset,T& out,std::string& error){return memory.copy(object+offset,&out,sizeof out,error);}
bool text(uintptr_t object,size_t offset,size_t bound,std::string& out,std::string& error){
    const auto& value=*reinterpret_cast<String*>(object+offset);
    if(value.length>bound)return fail(error,"fixture string exceeds native bound");
    out.assign(value.data,value.length);return true;
}
void checked_release(void* object){if(fail_cleanup)RaiseException(0xc0000005,0,0,nullptr);release(object);}
}
namespace mt2mp::subscriber_lifecycle::first_append {
bool resolve_record_api(Memory&,uintptr_t,DecodeApi& out,std::string& error){
    out={};if(fixture::fail_record_api){error="original Record API code read failed";return false;}
    out=fixture::record_api;return true;
}
}
namespace mt2mp::subscriber_lifecycle {
bool mock_weapon_load(PropertyLoad,const void* property,void* object,void* record,CallReceipt& receipt,std::string& error){
    receipt={};receipt.attempted=true;++fixture::stores;
    if(fixture::stores==fixture::fail_store){receipt.seh=0xc0000005;error="original scalar exception; effects unknown, do not replay";return false;}
    const auto offset=*reinterpret_cast<const uintptr_t*>(static_cast<const char*>(property)+0x38);
    const auto& token=*static_cast<const ::Token*>(static_cast<const ::Record*>(record)->tokens.data);
    if(offset==0x4b8){auto* target=static_cast<char*>(object)+offset;destroy_string(target);construct_string(target,token.text.data,token.text.length,nullptr);}
    else std::memcpy(static_cast<char*>(object)+offset,&token.bits,4);
    receipt.returned=true;return true;
}
}
namespace fixture {
#define load_scalar_property mock_weapon_load
#include "live_host_state/day_rollover/economy/native_weapon_state.inc"
#undef load_scalar_property
struct World {
    std::unique_ptr<unsigned char[]> image=std::make_unique<unsigned char[]>(0x1740000);
    alignas(8) std::array<unsigned char,0x900> toon{};
    Pin pin{reinterpret_cast<uintptr_t>(image.get())};
    uintptr_t owner=reinterpret_cast<uintptr_t>(toon.data());
    World(){
        const auto property=[&](size_t rva,size_t ap,size_t load,size_t offset){
            const uintptr_t address=pin.image+ap,function=pin.image+load;
            std::memcpy(image.get()+rva,&address,8);std::memcpy(image.get()+ap+0x10,&function,8);std::memcpy(image.get()+rva+0x38,&offset,8);
        };
        property(0x13c8b40,0x16d64d0,0xd2c7b0,0x4b0);property(0x13c8b00,0x16cb410,0xd198b0,0x4b8);property(0x13c8ac0,0x16d64d0,0xd2c7b0,0x4d8);
        const std::array<unsigned char,16> int_load={0x48,0x89,0xc8,0x4c,0x89,0xc1,0x4d,0x89,0xc8,0x48,0x03,0x50,0x38,0xe9,0x3e,0x34};
        const std::array<unsigned char,16> string_load={0x48,0x89,0xc8,0x4c,0x89,0xc1,0x4d,0x89,0xc8,0x48,0x03,0x50,0x38,0xe9,0xde,0x8c};
        const std::array<unsigned char,16> leaf={0x53,0x48,0x83,0xec,0x20,0x31,0xc0,0x48,0x89,0xd3,0x8b,0x51,0x40,0x85,0xd2,0x7f};
        std::memcpy(image.get()+0xd2c7b0,int_load.data(),16);std::memcpy(image.get()+0xd198b0,string_load.data(),16);
        std::memcpy(image.get()+0x89fc00,leaf.data(),16);std::memcpy(image.get()+0x8a25a0,leaf.data(),16);
        construct_string(toon.data()+0x4b8,"",0,nullptr);
    }
    ~World(){destroy_string(toon.data()+0x4b8);}
};
}
int main(){try{
    fixture::record_api.allocate=allocate;fixture::record_api.release=fixture::checked_release;
    fixture::record_api.record_construct=construct;fixture::record_api.record_destroy=destroy;
    fixture::record_api.string_destroy=destroy_string;fixture::record_api.set_label=label;
    fixture::record_api.set_string=text;fixture::record_api.token_count=count;
    fixture::record_api.set_integer=integer;fixture::record_api.string_construct=construct_string;
    fixture::World world;fixture::Toon prior,target;target.weapon_ilvl=13;target.weapon_category="swords";target.weapon_variant_id=INT32_MIN;
    std::string error;unsigned fences=0;const auto fence=[&](std::string&){++fences;return true;};
    const auto api_allocations_before=allocations;
    fixture::fail_record_api=true;
    need(!fixture::store_toon_weapon(world.pin,world.owner,prior,target,fence,error)&&fixture::stores==0&&
        allocations==api_allocations_before&&fences==0&&error=="original Record API code read failed","Record API failure allocated or stored before admission");
    fixture::fail_record_api=false;
    const bool adopted=fixture::store_toon_weapon(world.pin,world.owner,prior,target,fence,error);
    need(adopted,error.c_str());fixture::Toon actual;
    need(fixture::read_toon_weapon(world.owner,actual,error)&&fixture::same_weapon(actual,target),"native complete chosen tuple differs");
    need(fixture::stores==3&&allocations==releases&&fences==5,"native tuple calls/cleanup/fences differ");
    const auto allocations_before=allocations,fences_before=fences;
    need(fixture::store_toon_weapon(world.pin,world.owner,target,target,fence,error)&&fixture::stores==3&&allocations==allocations_before&&fences==fences_before,"duplicate tuple performed effects");
    need(!fixture::store_toon_weapon(world.pin,world.owner,prior,target,fence,error)&&fixture::stores==3&&allocations==releases&&error.find("prior tuple changed")!=std::string::npos,"native prior tuple barrier lost");
    world.image[0xd198b0]^=1;
    need(!fixture::store_toon_weapon(world.pin,world.owner,target,prior,fence,error)&&allocations==releases&&error.find("original scalar body differs")!=std::string::npos,"native signature mismatch admitted");
    world.image[0xd198b0]^=1;
    auto changed=target;changed.weapon_ilvl=14;changed.weapon_category="bows";changed.weapon_variant_id=17;
    fixture::fail_store=fixture::stores+2;
    need(!fixture::store_toon_weapon(world.pin,world.owner,target,changed,fence,error)&&fixture::stores==5&&error.find("original scalar exception")!=std::string::npos&&error.find("native owner retained field=1")!=std::string::npos,"partial tuple exception lost cause or continued stores");
    need(fixture::read_toon_weapon(world.owner,actual,error)&&actual.weapon_ilvl==14&&actual.weapon_category=="swords"&&actual.weapon_variant_id==INT32_MIN,"partial native effects falsely reported complete");
    fixture::fail_store=0;fixture::fail_cleanup=true;changed=actual;changed.weapon_variant_id=19;
    need(!fixture::store_toon_weapon(world.pin,world.owner,actual,changed,fence,error)&&error.find("Record cleanup")!=std::string::npos,"native cleanup failure became success");
    fixture::fail_cleanup=false;
    std::cout<<"PASS "<<checks<<" actual native tuple orchestration cases\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
