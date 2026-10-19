#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace fixture {
constexpr uintptr_t fixture_image=0x10000000;
struct {uintptr_t image=fixture_image;}state;
struct MT2FW_MT2_RouteReceipt {uint32_t size;unsigned char current[32]{},pristine[32]{};};
namespace dungeon_config_codes {
struct Pin {uintptr_t rva;size_t size;const char* sha;};
constexpr std::array<Pin,2> helpers{{{0x54d80,113,"original-add-expense"},{0x75610,47,"original-unhooked-helper"}}};
}
struct Memory {
    std::map<uintptr_t,std::string> code;bool fail_read=false;
    bool copy(uintptr_t address,void* target,size_t size,std::string& error){
        if(fail_read){error="original helper memory read failed";return false;}
        auto row=code.find(address);if(row==code.end()||row->second.size()<size){error="original helper memory span missing";return false;}
        std::memcpy(target,row->second.data(),size);return true;
    }
}fixture_memory;
std::map<std::string,std::string> original_hashes;
bool sha_failure=false,route_failure=false,verify_failure=false;
unsigned route_calls=0,verify_calls=0;size_t verified_size=0;
MT2FW_MT2_RouteReceipt route{sizeof route};std::string expected_original,verify_cause;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
namespace quest_bridge {
bool sha256(const std::string& bytes,std::string& hash,std::string& error){
    if(sha_failure)return fail(error,"original helper SHA operation failed");
    auto row=original_hashes.find(bytes);hash=row==original_hashes.end()?"foreign-full-body":row->second;return true;
}
}
namespace quest_list {
bool read_route(uintptr_t address,MT2FW_MT2_RouteReceipt& out,std::string& error){
    ++route_calls;if(route_failure)return fail(error,"original Backend route read failed");
    if(address!=fixture_image+0x54d80)return fail(error,"unexpected helper route query");out=route;return true;
}
bool verify_code(uintptr_t address,const void* bytes,size_t size,std::string& error){
    ++verify_calls;verified_size=size;
    if(verify_failure)return fail(error,verify_cause);
    if(address!=fixture_image+0x54d80||size!=expected_original.size()||std::memcmp(bytes,expected_original.data(),size))return fail(error,"original Backend pristine full-body verification differs");
    return true;
}
}
#include "dungeon_config_helpers_actual.inc"
size_t checks=0;
void need(bool okay,const std::string& cause){++checks;if(!okay)throw std::runtime_error(cause+" comparison="+std::to_string(checks));}
void setup(bool hooked=true){
    fixture_memory={};original_hashes.clear();sha_failure=route_failure=verify_failure=false;route_calls=verify_calls=0;verified_size=0;verify_cause.clear();route={sizeof route};
    for(const auto& pin:dungeon_config_codes::helpers){std::string body(pin.size,'\0');for(size_t i=0;i<body.size();++i)body[i]=char((i*17+pin.rva)%251);
        original_hashes.emplace(body,pin.sha);fixture_memory.code.emplace(fixture_image+pin.rva,body);if(pin.rva==0x54d80)expected_original=body;}
    std::memcpy(route.pristine,expected_original.data(),32);std::memcpy(route.current,route.pristine,32);
    if(hooked){route.current[0]=0xff;route.current[1]=0x25;for(size_t i=2;i<14;++i)route.current[i]=static_cast<unsigned char>(i+71);}
    std::memcpy(fixture_memory.code.at(fixture_image+0x54d80).data(),route.current,32);
}
void tests(){std::string error;
    setup();const auto live=fixture_memory.code;
    need(check_helpers(fixture_memory,error),error);
    need(route_calls==1&&verify_calls==1&&verified_size==113&&fixture_memory.code==live,"Owned AddExpense guard omitted full verification or wrote into executable memory");
    setup(false);need(check_helpers(fixture_memory,error)&&route_calls==1&&verify_calls==1&&verified_size==113,error);
    setup();fixture_memory.code.at(fixture_image+0x54d80)[0]^=1;need(!check_helpers(fixture_memory,error)&&error.find("current route bytes")!=std::string::npos&&verify_calls==0,"Foreign live prefix was reconstructed without matching the actual receipt");
    setup();route.current[14]^=1;need(!check_helpers(fixture_memory,error)&&error.find("current route bytes")!=std::string::npos,"Mismatched unchanged part of current32 was ignored");
    setup();route.pristine[17]^=1;need(!check_helpers(fixture_memory,error)&&error.find("helper body differs")!=std::string::npos&&verify_calls==0,"Foreign pristine receipt passed the original full-body SHA pin");
    setup();fixture_memory.code.at(fixture_image+0x54d80)[112]^=1;need(!check_helpers(fixture_memory,error)&&error.find("helper body differs")!=std::string::npos&&verify_calls==0,"Tampered helper tail beyond stolen/current32 bytes was ignored");
    setup();verify_failure=true;verify_cause="original Backend route has foreign owner";need(!check_helpers(fixture_memory,error)&&error==verify_cause&&verify_calls==1&&verified_size==113,"Foreign owner verification was bypassed or original cause was replaced");
    setup();verify_failure=true;verify_cause="original Backend route generation is stale";need(!check_helpers(fixture_memory,error)&&error==verify_cause&&verify_calls==1,"Stale owned receipt was accepted from matching prefix alone");
    setup();route_failure=true;need(!check_helpers(fixture_memory,error)&&error=="original Backend route read failed"&&verify_calls==0,"Original route read failure was replaced");
    setup();fixture_memory.fail_read=true;need(!check_helpers(fixture_memory,error)&&error=="original helper memory read failed"&&route_calls==0,"Original native read failure was replaced");
    setup();sha_failure=true;need(!check_helpers(fixture_memory,error)&&error=="original helper SHA operation failed"&&verify_calls==0,"Original SHA failure was replaced");
    setup();fixture_memory.code.at(fixture_image+0x75610)[40]^=1;need(!check_helpers(fixture_memory,error)&&error.find("RVA="+std::to_string(0x75610))!=std::string::npos&&route_calls==1,"Other helper guard was weakened or queried as an owned economic route");
    setup();need(check_helpers(fixture_memory,error)&&route_calls==1&&verify_calls==1,error);
}
}
int main()try{fixture::tests();std::cout<<fixture::checks<<" actual Dungeon config helper pristine/owned/foreign/stale/full-body route comparisons passed\n";return 0;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
