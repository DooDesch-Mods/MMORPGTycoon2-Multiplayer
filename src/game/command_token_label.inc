namespace command_token_label {
inline constexpr uintptr_t entry_rva=0x96e1d0;
inline constexpr size_t entry_bytes=80;
inline constexpr const char* entry_sha256="8804bc96497105c4fc82d693ffc88533c3e6c1b9e70b92e3d310cdad72a0f2cc";
using Equals=bool(*)(const void*,const quest_list::QuestTitleView&);
static_assert(sizeof(quest_list::QuestTitleView)==32);
bool matches(Equals equals,const void* record,const char* wanted){
    const quest_list::QuestTitleView text{wanted,std::char_traits<char>::length(wanted)};
    return equals(record,text);
}
uint32_t native_read(Equals equals,const void* record,const char* wanted,bool& result)noexcept{
    __try{result=matches(equals,record,wanted);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool read(Memory& memory,const void* record,const char* wanted,bool& result,std::string& error){
    result=false;std::string bytes(entry_bytes,'\0'),hash;
    if(!memory.copy(state.image+entry_rva,bytes.data(),bytes.size(),error)||!quest_bridge::sha256(bytes,hash,error))return false;
    if(hash!=entry_sha256)return fail(error,"Native token-label helper body differs required="+std::string(entry_sha256)+" actual="+hash);
    // The original token comparison ignores record arguments; payload readers validate them separately.
    const auto seh=native_read(reinterpret_cast<Equals>(state.image+entry_rva),record,wanted,result);
    if(seh)return fail(error,"Native token-label comparison SEH="+std::to_string(seh)+" label="+wanted);
    return true;
}
}
