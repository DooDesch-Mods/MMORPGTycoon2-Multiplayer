#include "mt2mp/core/private_profile_paths.h"
#include <iostream>
#include "../src/core/dropin_proof_policy.inc"
static void environment(const wchar_t* name,const wchar_t* value){
    if(!SetEnvironmentVariableW(name,value))throw std::runtime_error("fixture environment update failed");
}
int wmain(int argc,wchar_t** argv){
    if(argc>2){std::cerr<<"usage: test_policy [new-empty-owned-directory]\n";return 2;}
    try{
        const auto owner=argc==2?std::filesystem::absolute(argv[1]):std::filesystem::current_path()/(L"policy_fixture_"+std::to_wstring(GetCurrentProcessId())+L"_"+std::to_wstring(GetTickCount64()));
        if(argc==1&&!std::filesystem::create_directory(owner))throw std::runtime_error("fresh owned fixture directory was not created");
        if(!std::filesystem::is_directory(owner)||!std::filesystem::is_empty(owner))throw std::runtime_error("owned fixture must exist and be empty");
        const auto proof=owner/L"_MPTest/dropin-proof",game=proof/L"game";
        std::filesystem::create_directories(game);std::filesystem::create_directories(proof/L"appdata");
        environment(L"MT2MP_DROPIN_PROOF_ROOT",nullptr);
        if(!checked_dropin_proof_root(game).empty())throw std::runtime_error("normal startup enabled diagnostics");
        environment(L"MT2MP_DROPIN_PROOF_ROOT",proof.c_str());environment(L"LOCALAPPDATA",(proof/L"appdata").c_str());
        if(!mt2mp::private_profile::equal(checked_dropin_proof_root(game),proof))throw std::runtime_error("owned proof rejected");
        bool rejected=false;
        try{checked_dropin_proof_root(owner);}catch(const std::exception& error){rejected=std::string(error.what()).find("executable directory")!=std::string::npos;}
        if(!rejected)throw std::runtime_error("foreign executable accepted");
        environment(L"LOCALAPPDATA",owner.c_str());rejected=false;
        try{checked_dropin_proof_root(game);}catch(const std::exception& error){rejected=std::string(error.what()).find("LOCALAPPDATA")!=std::string::npos;}
        if(!rejected)throw std::runtime_error("normal AppData accepted");
        std::cout<<"PASS: exact staged policy body; normal-off, owned opt-in, foreign executable rejection, outside AppData rejection\n";return 0;
    }catch(const std::exception& error){std::cerr<<"error: proof policy fixture: "<<error.what()<<'\n';return 1;}
}
