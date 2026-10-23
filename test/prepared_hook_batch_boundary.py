"""Reverse only the prepared Dungeon cell enable phase for historical guards."""

OLD = '''    for(const auto rva:{dungeon_cells_codes::drag.rva,dungeon_cells_codes::release.rva,dungeon_cells_codes::create.rva}){status=MH_EnableHook(reinterpret_cast<void*>(state.image+rva));if(status!=MH_OK)return fail(error,"Dungeon cell UI enable failed RVA="+std::to_string(rva)+" status="+std::to_string(status));}
'''
NEW = '''    const uintptr_t targets[]{state.image+dungeon_cells_codes::drag.rva,state.image+dungeon_cells_codes::release.rva,state.image+dungeon_cells_codes::create.rva};
    if(!mt2mp::hooks::enable_prepared_targets(targets,"Dungeon cell UI",error))return false;
'''


def normalize_cells_enable(text):
    text = text.replace('\r\n', '\n')
    assert text.count(NEW) == 1, 'Exact prepared Dungeon cell enable phase changed'
    before, after = text.split(NEW)
    assert before.rindex('MH_CreateHook(') < before.rindex('if(status!=MH_OK||!original_create)')
    assert after.startswith('    MT2FW_MT2_RouteReceipt drag{sizeof drag},release{sizeof release},create{sizeof create};\n')
    return text.replace(NEW, OLD, 1)
