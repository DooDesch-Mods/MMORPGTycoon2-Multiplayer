static std::filesystem::path checked_dropin_proof_root(const std::filesystem::path& game){
    std::array<wchar_t,32768> value{};
    const DWORD n=GetEnvironmentVariableW(L"MT2MP_DROPIN_PROOF_ROOT",value.data(),DWORD(value.size()));
    if(!n)return {};
    if(n>=value.size())throw std::runtime_error("drop-in proof root is truncated");
    const std::filesystem::path proof(value.data());
    if(!proof.is_absolute()||proof.filename()!=L"dropin-proof"||proof.parent_path().filename()!=L"_MPTest")
        throw std::runtime_error("drop-in diagnostics require explicit _MPTest/dropin-proof root");
    for(const auto& part:proof.relative_path())if(part==L"."||part==L"..")throw std::runtime_error("drop-in proof parent/alias component refused");
    if(!mt2mp::private_profile::equal(mt2mp::private_profile::canonical_file(proof),proof)||
       !mt2mp::private_profile::equal(game,proof/L"game"))
        throw std::runtime_error("drop-in proof does not own exact canonical executable directory");
    const DWORD local_n=GetEnvironmentVariableW(L"LOCALAPPDATA",value.data(),DWORD(value.size()));
    if(!local_n||local_n>=value.size()||!mt2mp::private_profile::equal(std::filesystem::path(value.data()),proof/L"appdata"))
        throw std::runtime_error("drop-in proof requires LOCALAPPDATA inside its own appdata directory");
    return proof;
}
