namespace dungeon_cells {
namespace {
bool original_result(uint32_t seh,const char* operation,std::string& error){return !seh||fail(error,std::string("Dungeon cell original ")+operation+" SEH="+std::to_string(seh));}
struct Port {
    Memory& memory;const Sample& before;const a::DungeonCellStroke& stroke;
    calls::EditStorage storage;calls::Start started;calls::Commit committed;
    uintptr_t owner()const{return before.palette.dungeon.dungeon.dungeon.local.dungeon;}
    bool layout(dungeon_layout_reader::Record& out,std::string& error){
        auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& e){return field(memory,base,offset,value,e);};
        auto text=[&](uintptr_t base,size_t offset,std::string& value,std::string& e){return npc_quest_records::read_string(memory,base,offset,"Dungeon live cell Theme",value,e);};
        dungeon_layout_reader::Local local;if(!dungeon_layout_reader::read(at,text,state.image,owner(),out,local,error))return false;
        return local==before.palette.local||fail(error,"Dungeon cell original layout storage identity changed during native execution");
    }
    bool eligible(const dungeon_layout_reader::Record& current,int32_t index,bool& result,std::string& error){
        result=false;if(index<0||size_t(index)>=current.cells.size()||current.palette.size()<4)return fail(error,"Dungeon live cell index/palette differs");
        const auto& cell=current.cells[size_t(index)];bool blocked=false;
        if(stroke.mode==a::DungeonCellMode::Close){if(!cell.open)return true;
            if(!original_result(calls::obstructed(state.image,owner(),index,false,blocked),"Gizmo obstruction",error))return false;
            result=!blocked;return true;
        }
        if(!cell.open)return original_result(calls::can_open(state.image,owner(),index,result),"CanCellOpen",error);
        std::array<uint32_t,4> palette{};std::copy_n(current.palette.begin(),4,palette.begin());
        result=cell.theme!=stroke.theme||cell.colors!=palette;return true;
    }
    bool ready(const std::vector<dungeon_cells_native::Cell>& cells,std::string& error){
        dungeon_layout_reader::Record current;if(!layout(current,error))return false;
        bool any=false;for(const auto& cell:cells){bool allowed=false;if(!eligible(current,cell.index,allowed,error))return false;any=any||allowed;}
        return any||fail(error,"Dungeon cell stroke has no currently eligible changed cells");
    }
    bool start(uintptr_t& graph,bool& opened,std::string& error){
        const auto seh=calls::start(state.image,owner(),storage,started);graph=started.graph;opened=started.opened;
        return (original_result(seh,"_StartEdit",error)&&started.returned&&started.marker&&graph)||
            fail(error,error.empty()?"Dungeon original _StartEdit lacks graph/marker/return receipt":error);
    }
    bool active(uintptr_t graph,std::string& error){return original_result(calls::active(state.image,graph),"SetActive",error);}
    bool cell(const dungeon_cells_native::Cell& cell,bool& changed,std::string& error){
        changed=false;dungeon_layout_reader::Record current;if(!layout(current,error))return false;bool allowed=false;
        if(!eligible(current,cell.index,allowed,error))return false;if(!allowed)return true;
        if(stroke.mode==a::DungeonCellMode::Close){return original_result(calls::closed(state.image,owner(),cell.index,changed),"CloseCell",error)&&
            (changed||fail(error,"Dungeon original CloseCell did not confirm the previously open cell"));}
        const calls::Text theme{stroke.theme.data(),uint64_t(stroke.theme.size())};
        if(!current.cells[size_t(cell.index)].open){bool opened=false;
            if(!original_result(calls::opened(state.image,owner(),cell.index,theme,opened),"OpenCell",error)||!opened)return fail(error,error.empty()?"Dungeon original OpenCell did not confirm eligible cell":error);
            changed=true;if(!layout(current,error))return false;
        }
        if(current.cells[size_t(cell.index)].theme!=stroke.theme){if(!original_result(calls::theme(state.image,owner(),cell.index,theme),"SetCellTheme",error))return false;changed=true;}
        // SetCellTheme also copies the palette, so reread before deciding on
        // the separate original color-only operation.
        if(!layout(current,error))return false;std::array<uint32_t,4> palette{};std::copy_n(current.palette.begin(),4,palette.begin());
        if(current.cells[size_t(cell.index)].colors!=palette){if(!original_result(calls::colors(state.image,owner(),cell.index),"SetCellData",error))return false;changed=true;}
        return true;
    }
    bool drain(uintptr_t graph,std::string& error){return original_result(calls::drain(state.image,graph),"queued deletion drain",error);}
    bool finish(std::string& error){return original_result(calls::finished(state.image,owner()),"FinishedEditing",error);}
    bool end(std::string& error){return original_result(calls::end(state.image),"EndEdit",error);}
    bool release_marker(std::string& error){return original_result(calls::release_marker(state.image,started),"edit marker destruction",error);}
    bool commit(uintptr_t graph,std::string& error){
        const auto seh=calls::commit(state.image,before.palette.dungeon.dungeon.dungeon.undo.receiver,graph,committed);
        if(seh||committed.arguments_cleanup||committed.string_cleanup||!committed.returned)
            return fail(error,"Dungeon original Undo::AddDo/label lifetime failed SEH="+std::to_string(seh)+" arguments_cleanup="+
                std::to_string(committed.arguments_cleanup)+" string_cleanup="+std::to_string(committed.string_cleanup));
        return true;
    }
};
}
}
