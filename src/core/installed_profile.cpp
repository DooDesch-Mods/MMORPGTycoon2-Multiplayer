#include "mt2mp/core/installed_profile.h"
#include "mt2mp/core/private_profile_paths.h"
#include <memory>

namespace mt2mp::installed_profile {
namespace {std::unique_ptr<Verified> profile;private_profile::DirectoryPins pins;bool attempted=false;std::filesystem::path proof_root;}
#include "dropin_proof_policy.inc"

bool accept(const Receipt& receipt,std::string& error){
    if(attempted){error="installed profile acceptance already attempted";return false;}attempted=true;
    try{
        if(receipt.size!=sizeof(Receipt)||receipt.version!=1||receipt.process_id!=GetCurrentProcessId()||receipt.isolation_flags!=3||
           !receipt.loader||!receipt.game_dir||!receipt.runtime_root||!receipt.user_data_dir||!receipt.run_id)
            throw std::runtime_error("installed profile bootstrap receipt differs");
        for(const wchar_t* variable:{L"MT2MP_TEST_ROOT",L"MT2MP_RUNTIME_ROOT",L"MT2MP_RUN_ID"})
            if(GetEnvironmentVariableW(variable,nullptr,0))throw std::runtime_error("installed profile cannot mix launcher test variables");
        auto candidate=std::make_unique<Verified>();
        candidate->game_dir=private_profile::module_file(nullptr).parent_path();
        candidate->runtime_root=private_profile::runtime_root(candidate->game_dir);
        candidate->user_data_dir=candidate->runtime_root/L"data";
        if(!private_profile::equal(candidate->game_dir,receipt.game_dir)||!private_profile::equal(candidate->runtime_root,receipt.runtime_root)||
           !private_profile::equal(candidate->user_data_dir,receipt.user_data_dir)||
           !private_profile::equal(private_profile::module_file(receipt.loader),candidate->game_dir/L"WSOCK32.dll"))
            throw std::runtime_error("installed profile exact module/root path differs");
        const size_t n=strnlen_s(receipt.run_id,37);
        if(n!=36||std::string(receipt.run_id,n).find_first_not_of("0123456789abcdef-")!=std::string::npos)
            throw std::runtime_error("installed profile run ID invalid");
        candidate->run_id.assign(receipt.run_id,n);
        const auto confirmed_proof=checked_dropin_proof_root(candidate->game_dir);
        pins.verify(candidate->runtime_root);pins.verify(candidate->user_data_dir);pins.verify(candidate->runtime_root/L"diagnostics");
        HMODULE pinned=nullptr;const auto loader=candidate->game_dir/L"WSOCK32.dll";
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN,loader.c_str(),&pinned)||pinned!=receipt.loader)
            throw std::runtime_error("installed profile loader pin differs Win32="+std::to_string(GetLastError()));
        proof_root=confirmed_proof;profile=std::move(candidate);error.clear();return true;
    }catch(const std::exception& ex){error=ex.what();OutputDebugStringA(("error: installed profile refused: "+error+"\n").c_str());return false;}
}
const Verified* get(){return profile.get();}
const std::filesystem::path& diagnostic_root(){return proof_root;}
}
