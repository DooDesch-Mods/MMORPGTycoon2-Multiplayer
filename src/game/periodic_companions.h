#pragma once
#include "selected_snapshot.h"

namespace mt2mp::game::periodic_companions {
namespace fs=std::filesystem;
inline void require(bool value,const std::string& cause){if(!value)throw std::runtime_error("Periodic companion rotation: "+cause);}
inline fs::path sidecar(const fs::path& native){return fs::path(native.wstring()+L".monsters.json");}
struct File {
    bool present=false;
    std::string sha;
    uint64_t bytes=0;
    bool operator==(const File&)const=default;
};
inline File read_file(const fs::path& path){
    File value;value.present=selected_snapshot::regular(path,false);if(!value.present)return value;
    std::string error;require(net::hash_snapshot_file(path,value.sha,value.bytes,error),error);return value;
}
struct Pair {File native,companion;std::string text;};
struct Rotation {
    fs::path directory;
    std::array<Pair,3> before;
    bool consumed=false;
};
inline fs::path native_path(const fs::path& directory,size_t index){
    return directory/(index==0?L"periodic.vrz":index==1?L"periodic+1.vrz":L"periodic+2.vrz");
}
inline Rotation prepare(const fs::path& target){
    require(target.is_absolute()&&target.filename()==L"periodic.vrz","exact absolute periodic target required");
    private_profile::DirectoryPins pins;pins.verify(target.parent_path());
    Rotation value;value.directory=target.parent_path();
    for(size_t index=0;index<value.before.size();++index){
        const auto path=native_path(value.directory,index);auto& pair=value.before[index];
        net::SnapshotFileHandle native_pin,companion_pin;std::string error;
        pair.native=read_file(path);pair.companion=read_file(sidecar(path));
        if(!pair.companion.present)continue;
        require(pair.native.present,"companion has no native file index="+std::to_string(index));
        require(pair.companion.bytes<=monster_snapshot_companion::maximum_bytes,"companion exceeds byte budget");
        require(native_pin.open(path,GENERIC_READ,OPEN_EXISTING,error),error);
        require(companion_pin.open(sidecar(path),GENERIC_READ,OPEN_EXISTING,error),error);
        require(private_profile::equal(private_profile::final_path(native_pin.get()),path)&&
            private_profile::equal(private_profile::final_path(companion_pin.get()),sidecar(path)),"pinned path differs");
        pair.text.resize(size_t(pair.companion.bytes));DWORD count=0;
        require(ReadFile(companion_pin.get(),pair.text.data(),DWORD(pair.text.size()),&count,nullptr)!=0&&count==pair.text.size(),
            "exact companion read failed Win32="+std::to_string(GetLastError()));
        monster_snapshot_companion::Manifest manifest;
        require(monster_snapshot_companion::decode(pair.text,pair.native.sha,pair.native.bytes,manifest,error),error);
        require(read_file(path)==pair.native&&read_file(sidecar(path))==pair.companion,"source changed during preparation");
        require(native_pin.close(&error),error);require(companion_pin.close(&error),error);
    }
    return value;
}
inline nlohmann::json finish(Rotation& value){
    require(!value.consumed&&!value.directory.empty(),"receipt was already consumed or absent");value.consumed=true;
    private_profile::DirectoryPins pins;pins.verify(value.directory);
    // Original PeriodicAutosave already rotated the native files. Require the
    // exact old bytes at both destinations before changing either companion.
    for(size_t index=1;index<3;++index){
        const auto path=native_path(value.directory,index);
        require(read_file(path)==value.before[index-1].native,"original native rotation differs index="+std::to_string(index));
        require(read_file(sidecar(path))==value.before[index].companion,"destination companion changed index="+std::to_string(index));
    }
    const auto primary=sidecar(native_path(value.directory,0));
    require(read_file(primary)==value.before[0].companion,"primary companion changed before rotation");
    auto receipt=nlohmann::json::array();
    // Preserve the old backup before moving the primary to its new native pair.
    // Fresh primary publication remains exclusive through SnapshotBuffer.
    for(size_t index=2;index>0;--index){
        const auto path=sidecar(native_path(value.directory,index));const auto& source=value.before[index-1];
        if(index==1&&source.companion.present){
            require(read_file(primary)==source.companion&&read_file(path)==value.before[index].companion,
                "primary move source or destination changed");
            if(!MoveFileExW(primary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
                require(false,"primary companion move failed Win32="+std::to_string(GetLastError()));
            require(!read_file(primary).present,"primary companion still exists after original-pair rotation");
        }else if(source.companion.present){
            auto temporary=path;temporary+=L".pending-"+std::filesystem::path(private_profile::new_run_id()).wstring();
            net::SnapshotFileHandle output;std::string error;
            require(output.open(temporary,GENERIC_WRITE,CREATE_NEW,error),error);DWORD count=0;
            require(WriteFile(output.get(),source.text.data(),DWORD(source.text.size()),&count,nullptr)!=0&&count==source.text.size(),
                "companion write failed Win32="+std::to_string(GetLastError()));
            require(FlushFileBuffers(output.get())!=0,"companion flush failed Win32="+std::to_string(GetLastError()));
            require(output.close(&error),error);
            require(read_file(temporary)==source.companion,"temporary companion readback differs");
            if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
                require(false,"companion replace failed Win32="+std::to_string(GetLastError()));
        }else if(value.before[index].companion.present){
            require(DeleteFileW(path.c_str())!=0,"stale destination companion removal failed Win32="+std::to_string(GetLastError()));
        }
        require(read_file(path)==source.companion,"companion rotation acknowledgement differs index="+std::to_string(index));
        receipt[index-1]={{"index",index},{"native_sha256",source.native.sha},{"native_bytes",source.native.bytes},
            {"companion_present",source.companion.present},{"companion_sha256",source.companion.sha}};
    }
    return receipt;
}
}
