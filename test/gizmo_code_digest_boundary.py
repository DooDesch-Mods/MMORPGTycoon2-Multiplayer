import hashlib

def normalize_gizmo_code(text):
    changes = [
        ('    live_host_state::apply_timing::PageReadScope pages(!live_host_state::apply_timing::PageReadScope::reader_active());\n', ''),
        ('    static thread_local std::array<hooks::framework::CodeBodyDigest,1+std::size(dungeon_gizmo_codes::helpers)+std::size(dungeon_gizmo_live_codes::values)> digests;\n    size_t slot=0;\n', ''),
        ('    const auto digest=[&](uintptr_t entry,const char* expected){return digests[slot++].read(entry,bytes.data(),bytes.size(),expected,hash,error,\n        [&](std::string& value,std::string& cause){return quest_bridge::sha256(bytes,value,cause);});};\n', ''),
        ('!digest(state.image+ui_code.rva,ui_code.sha)', '!quest_bridge::sha256(bytes,hash,error)'),
        ('!digest(state.image+pin.rva,pin.sha)', '!quest_bridge::sha256(bytes,hash,error)'),
        ('pin.sha,error,&digests[slot++])', 'pin.sha,error)'),
    ]
    for after, before in changes:
        assert text.count(after) == 1, 'Gizmo exact digest-only source boundary changed'
        text = text.replace(after, before, 1)
    body = text[text.index('bool code('):text.index('bool protected_digest(')]
    assert hashlib.sha256(body.encode()).hexdigest() == '3c890e207c84801b59a6d77738cb769e3967dc92ecc05f4ca573dc32a3d82555', 'Gizmo code proof changed beyond digest memoization'
    return text
