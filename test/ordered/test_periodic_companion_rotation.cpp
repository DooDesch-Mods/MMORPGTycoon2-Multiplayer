#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "selected_snapshot.h"
#include <iostream>
namespace fs=std::filesystem;
fs::path failed_move_destination;
BOOL guarded_test_move(LPCWSTR from,LPCWSTR to,DWORD flags){
    if(!failed_move_destination.empty()&&fs::path(to)==failed_move_destination){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
    return ::MoveFileExW(from,to,flags);
}
#define MoveFileExW guarded_test_move
#include "periodic_companions.h"
#undef MoveFileExW
namespace pc=mt2mp::game::periodic_companions;
namespace net=mt2mp::net;
namespace companion=mt2mp::game::monster_snapshot_companion;
int checks=0;
void require(bool condition,const std::string& cause){++checks;if(!condition)throw std::runtime_error(cause);}
void write(const fs::path& path,const std::string& data){std::ofstream out(path,std::ios::binary);out<<data;out.close();require(bool(out),"fixture write failed");}
std::string encoded(const fs::path& native){
    const auto identity=pc::read_file(native);companion::Manifest manifest;
    manifest.native_sha256=identity.sha;manifest.native_bytes=identity.bytes;manifest.region_rng.present=true;
    manifest.region_rng.rows.push_back({7,{UINT64_MAX,0x8000000000000001ull,3,4},INT32_MIN});
    std::string text,error;require(companion::encode(manifest,text,error),error);return text;
}
void make_pair(const fs::path& dir,size_t index,bool with_companion=true){
    auto path=pc::native_path(dir,index);write(path,"native-"+std::to_string(index));
    if(with_companion)write(pc::sidecar(path),encoded(path));
}
void rotate_native(const fs::path& dir){
    auto oldest=pc::native_path(dir,2);if(fs::exists(oldest))require(fs::remove(oldest),"fixture original native oldest removal failed");
    for(size_t i=2;i>0;--i){auto source=pc::native_path(dir,i-1);if(fs::exists(source))fs::rename(source,pc::native_path(dir,i));}
}
bool publish(const fs::path& native,std::string& error){
    const auto text=encoded(native);net::SnapshotSha256 hasher;
    hasher.add(reinterpret_cast<const uint8_t*>(text.data()),text.size());
    auto target=pc::sidecar(native),staging=target;staging+=L".incoming";
    net::SnapshotBuffer output;const auto count=uint32_t((text.size()+net::snapshot_chunk_bytes-1)/net::snapshot_chunk_bytes);
    if(!output.begin(text.size(),count,staging,target,hasher.finish(),error))return false;
    for(uint32_t i=0;i<count;++i){size_t offset=size_t(i)*net::snapshot_chunk_bytes;
        if(!output.add(i,reinterpret_cast<const uint8_t*>(text.data()+offset),uint32_t(std::min<size_t>(net::snapshot_chunk_bytes,text.size()-offset)),error))return false;}
    return output.commit(error);
}
void valid_pair(const fs::path& native){
    auto file=pc::read_file(native),sidecar=pc::read_file(pc::sidecar(native));require(file.present&&sidecar.present,"complete pair absent");
    std::ifstream in(pc::sidecar(native),std::ios::binary);std::string text((std::istreambuf_iterator<char>(in)),{}),error;
    companion::Manifest manifest;require(companion::decode(text,file.sha,file.bytes,manifest,error),error);
    require(manifest.region_rng.present&&manifest.region_rng.rows.size()==1,"complete Region proof missing");
}
int main(int argc,char** argv)try{
    require(argc==2||argc==3,"owned fixture root required");auto root=fs::absolute(argv[1]);fs::create_directories(root);
    const auto triple=root/mt2mp::private_profile::new_run_id();fs::create_directory(triple);
    for(size_t i=0;i<3;++i)make_pair(triple,i,false);
    for(int save=1;save<=3;++save){
        auto prepared=pc::prepare(pc::native_path(triple,0));rotate_native(triple);write(pc::native_path(triple,0),"fresh-native-save-"+std::to_string(save));
        const auto receipt=pc::finish(prepared);
        for(size_t i=1;i<3;++i){require(pc::read_file(pc::sidecar(pc::native_path(triple,i)))==prepared.before[i-1].companion,"backup bytes changed");
            require(receipt.at(i-1).at("index")==i,"ascending receipt contract changed");}
        std::string error;const bool published=publish(pc::native_path(triple,0),error);
        if(argc==3&&save==2){require(!published&&error=="snapshot target already exists; refusing overwrite","old source did not reproduce Ord188");
            std::cout<<"Retained Ord188 second-save failure reproduced without overwriting primary\n";return 0;}
        require(published,"fresh primary publication failed: "+error);valid_pair(pc::native_path(triple,0));
        const auto committed=pc::read_file(pc::sidecar(pc::native_path(triple,0)));
        require(!publish(pc::native_path(triple,0),error)&&error.find("refusing overwrite")!=std::string::npos,"ordinary SnapshotBuffer overwrite guard changed");
        require(pc::read_file(pc::sidecar(pc::native_path(triple,0)))==committed,"rejected repeat publication changed bytes");
    }
    for(size_t i=0;i<3;++i)valid_pair(pc::native_path(triple,i));
    {
        const auto dir=root/mt2mp::private_profile::new_run_id();fs::create_directory(dir);
        for(size_t i=0;i<3;++i)make_pair(dir,i);
        auto value=pc::prepare(pc::native_path(dir,0));rotate_native(dir);write(pc::native_path(dir,0),"new-native");
        pc::finish(value);auto incoming=pc::sidecar(pc::native_path(dir,0));incoming+=L".incoming";write(incoming,"foreign partial upload");
        const auto retained=pc::read_file(incoming);std::string error;
        require(!publish(pc::native_path(dir,0),error),"occupied staging accepted");
        require(pc::read_file(incoming)==retained,"failed fresh publication changed occupied staging");
        require(!pc::read_file(pc::sidecar(pc::native_path(dir,0))).present,"failed fresh publication invented primary success");
        for(size_t i=1;i<3;++i){
            require(pc::read_file(pc::sidecar(pc::native_path(dir,i)))==value.before[i-1].companion,"failed publication lost exact old backup");
            valid_pair(pc::native_path(dir,i));
        }
    }
    // Fail the oldest backup or primary move. Retain recoverable original pairs.
    for(size_t destination:{size_t{2},size_t{1}}){
        const auto dir=root/mt2mp::private_profile::new_run_id();fs::create_directory(dir);
        for(size_t i=0;i<3;++i)make_pair(dir,i);
        auto value=pc::prepare(pc::native_path(dir,0));rotate_native(dir);write(pc::native_path(dir,0),"new-native");
        failed_move_destination=pc::sidecar(pc::native_path(dir,destination));bool rejected=false;
        try{pc::finish(value);}catch(const std::exception& e){rejected=std::string(e.what()).find("Win32=5")!=std::string::npos;}
        failed_move_destination.clear();require(rejected&&value.consumed,"injected partial filesystem failure hidden");
        require(pc::read_file(pc::sidecar(pc::native_path(dir,0)))==value.before[0].companion,"partial failure lost old primary companion");
        require(pc::read_file(pc::sidecar(pc::native_path(dir,1)))==value.before[1].companion,"failed move changed old backup1");
        require(pc::read_file(pc::sidecar(pc::native_path(dir,2)))==(destination==2?value.before[2].companion:value.before[1].companion),"partial backup2 outcome differs");
        bool replay=false;try{pc::finish(value);}catch(const std::exception&){replay=true;}require(replay,"failed rotation was replayed");
    }
    for(int foreign=0;foreign<4;++foreign){
        const auto dir=root/mt2mp::private_profile::new_run_id();fs::create_directory(dir);
        for(size_t i=0;i<3;++i)make_pair(dir,i);
        auto value=pc::prepare(pc::native_path(dir,0));rotate_native(dir);
        if(foreign<3)write(pc::sidecar(pc::native_path(dir,size_t(foreign))),"foreign companion");
        else write(pc::native_path(dir,2),"foreign native");
        std::array<pc::File,3> before{};for(size_t i=0;i<3;++i)before[i]=pc::read_file(pc::sidecar(pc::native_path(dir,i)));
        bool rejected=false;try{pc::finish(value);}catch(const std::exception&){rejected=true;}require(rejected,"changed rotation owner admitted");
        for(size_t i=0;i<3;++i)require(pc::read_file(pc::sidecar(pc::native_path(dir,i)))==before[i],"failed preflight changed companion");
    }
    bool wrong_target=false;try{pc::prepare(triple/L"autosave.vrz");}catch(const std::exception&){wrong_target=true;}
    require(wrong_target,"arbitrary target acquired periodic publication authority");
    std::cout<<checks<<" actual periodic filesystem and exclusive publication checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
