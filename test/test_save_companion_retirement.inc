namespace quit_fixture {
using fixture::need;
fs::path write_root,archive_root,native_file,companion_file;
json receipt;
int serial=0,quit_calls=0,archive_calls=0;
bool thread_ok=true,active=true,save_read_ok=true,quit_flag=true,desktop=false;
int32_t saves_value=0;
DWORD native_exception=0;
std::string original_companion;
constexpr uintptr_t mode_slot=1;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
bool check_thread(std::string& error){if(!thread_ok)return fail(error,"original wrong thread");return true;}
bool in_game(){return active;}
bool read_saves_in_progress(int32_t& count,std::string& error){count=saves_value;if(!save_read_ok)return fail(error,"original save status failure");return true;}
void* singleton(uintptr_t slot){need(slot==mode_slot,"Quit reads original Mode singleton");return &serial;}
bool read_at(void* mode,size_t offset,unsigned char& value){
    need(mode==&serial,"Quit readback uses same original Mode");
    if(offset==0x304)value=quit_flag?1:0;else if(offset==0x305)value=desktop?1:0;else throw std::runtime_error("unexpected Quit field");
    return true;
}
bool retire_autosave_companion(std::string& error){
    ++archive_calls;
    try{receipt=game::save_companion_retirement::archive(write_root,"World",archive_root);error.clear();return true;}
    catch(const std::exception& problem){return fail(error,std::string("return to title companion retirement failed before native autosave: ")+problem.what());}
}
DWORD call_string(uintptr_t address,void* mode,const char* command){
    ++quit_calls;
    need(address==0x4a2f90&&mode==&serial&&std::string(command)=="Quit","original Quit ABI/receiver/command unchanged");
    need(!receipt.is_null()&&!fs::exists(companion_file),"native Quit runs only after companion retirement or confirmed absence");
    if(native_exception)return native_exception;
    fixture::write(native_file,"new bytes from original native autosave");
    return 0;
}
#include "selected_snapshot_quit_actual.inc"
void reset(bool companion=true,bool meta=true){
    ++serial;write_root=fixture::root/("quit-"+std::to_string(serial));archive_root=write_root/"owned-archive";
    native_file=write_root/"save/World/autosave.vrz";companion_file=fs::path(native_file.wstring()+L".monsters.json");
    fixture::write(native_file,"old exact native save");
    if(meta)fixture::write(write_root/"save/World/autosave.meta.vrz","old exact native metadata");
    std::string sha,error;uint64_t bytes=0;need(net::hash_snapshot_file(native_file,sha,bytes,error),error);
    game::monster_snapshot_companion::Manifest manifest{sha,bytes,{{67,sha+":world:monster:37","action:11111111-1111-4111-8111-111111111111"}}};
    need(game::monster_snapshot_companion::encode(manifest,original_companion,error),error);
    if(companion)fixture::write(companion_file,original_companion);
    receipt=nullptr;quit_calls=archive_calls=0;thread_ok=active=save_read_ok=quit_flag=true;desktop=false;saves_value=0;native_exception=0;
}
void check_archive(bool meta){
    const auto destination=net::snapshot_world_path::utf8_path(receipt.at("archive").get<std::string>());
    need(receipt.at("state")=="archived_companion_before_native_autosave","archive acknowledgement remains explicit");
    need(fixture::read(destination/"autosave.vrz")=="old exact native save"&&
         fixture::read(destination/"autosave.vrz.monsters.json")==original_companion,"archive preserves exact original native and companion bytes");
    need(fs::exists(destination/"autosave.meta.vrz")==meta,"archive preserves exact metadata presence");
    if(meta)need(fixture::read(destination/"autosave.meta.vrz")=="old exact native metadata","archive preserves exact metadata bytes");
    const auto persisted=json::parse(fixture::read(destination/"receipt.json"));need(persisted==receipt,"archive receipt is persisted exactly");
    for(const auto& file:receipt.at("files"))if(file.at("present").get<bool>()){
        std::string sha,error;uint64_t bytes=0;
        need(net::hash_snapshot_file(destination/net::snapshot_world_path::utf8_path(file.at("name").get<std::string>()),sha,bytes,error),error);
        need(sha==file.at("sha256").get<std::string>()&&bytes==file.at("bytes").get<uint64_t>(),"archive receipt SHA/length matches real retained bytes");
    }
}
void cases(){
    for(const bool meta:{false,true}){
        reset(true,meta);std::string error;
        need(request_return_to_title(error)&&error.empty()&&quit_calls==1&&archive_calls==1,"original Quit is called once after complete archival");
        check_archive(meta);
        need(!fs::exists(companion_file)&&fixture::read(native_file)=="new bytes from original native autosave","native autosave writes normally without stale companion");
        auto copied=game::selected_snapshot::copy(write_root,"save/World/autosave.vrz",fixture::uuid(100+unsigned(serial)),net::snapshot_max_bytes,net::hash_snapshot_file);
        need(copied.at("selected_monster_companion_present")==false,"next exact selected save enters supported companion-absent path");
    }
    {
        reset(false);std::string error;
        need(request_return_to_title(error)&&quit_calls==1&&archive_calls==1,"absent companion leaves original Quit intact");
        need(receipt.at("state")=="no_companion"&&!fs::exists(archive_root),"absent companion creates no archive or fabricated alias record");
    }
    for(int bad=0;bad<3;++bad){
        reset();auto altered=json::parse(original_companion);
        if(bad==0)altered["native_sha256"]=std::string(64,'b');
        if(bad==1)altered["native_bytes"]=1;
        fixture::write(companion_file,bad==2?"malformed original companion":altered.dump());
        const auto before=fixture::read(companion_file);std::string error;
        need(!request_return_to_title(error)&&error.find("retirement refused")!=std::string::npos,"invalid companion remains a visible fail-closed rejection");
        need(quit_calls==0&&archive_calls==1&&!fs::exists(archive_root)&&fixture::read(companion_file)==before&&
             fixture::read(native_file)=="old exact native save","corrupt/mismatched pair is neither moved, rewritten nor overwritten");
    }
    {
        reset();net::SnapshotFileHandle held;std::string error;need(held.open(companion_file,GENERIC_READ,OPEN_EXISTING,error),error);
        need(!request_return_to_title(error)&&error.find("snapshot file open")!=std::string::npos&&quit_calls==0,
             "competing pinned reader prevents retirement rather than reopening or ignoring the companion");
        need(fs::exists(companion_file)&&!fs::exists(archive_root),"failed exclusive identity pin leaves original pair untouched");
        need(held.close(&error),error);
    }
    {
        reset();fixture::write(archive_root,"archive root is a file");std::string error;
        need(!request_return_to_title(error)&&!error.empty()&&quit_calls==0&&fs::exists(companion_file),"unavailable owned archive blocks original overwrite");
    }
    for(int guard=0;guard<4;++guard){
        reset();if(guard==0)thread_ok=false;if(guard==1)active=false;if(guard==2)save_read_ok=false;if(guard==3)saves_value=1;
        std::string error;need(!request_return_to_title(error)&&!error.empty()&&quit_calls==0&&archive_calls==0&&fs::exists(companion_file),
                               "existing thread/world/save guards precede any archival effect");
    }
    for(int failure=0;failure<3;++failure){
        reset();if(failure==0)native_exception=5;if(failure==1)quit_flag=false;if(failure==2)desktop=true;
        std::string error;need(!request_return_to_title(error)&&error.find(failure==0?"native call failed":"acknowledgement failed")!=std::string::npos&&quit_calls==1,
                               "original native exception/acknowledgement failure remains visible with no retry");
        check_archive(true);
    }
}
}
