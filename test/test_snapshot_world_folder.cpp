#include "mt2mp/net/snapshot_world_path.h"
#include <iostream>
#include <stdexcept>

namespace mt2mp::game::native {
bool thread_ok=true,world=true,string_ok=true;
std::string native_folder="World of Stories";
int string_reads=0;
constexpr int game_slot=3;
bool check_thread(std::string& error){if(thread_ok)return true;error="wrong native thread";return false;}
bool in_game(){return world;}
void* singleton(int slot){if(slot!=game_slot)throw std::runtime_error("unexpected native singleton");return reinterpret_cast<void*>(1);}
bool read_game_string(const void* owner,size_t offset,std::string& out){
    if(owner!=reinterpret_cast<void*>(1)||offset!=0x470)throw std::runtime_error("native game-folder field changed");
    ++string_reads;out=native_folder;return string_ok;
}
bool fail(std::string& error,const char* reason){error=reason;return false;}
#include "snapshot_world_folder_body.inc"
}
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main()try{
    using namespace mt2mp::game::native;
    std::string folder="stale",error="old";
    check(read_active_world_folder(folder,error)&&folder==native_folder&&error.empty()&&string_reads==1,"active folder not read exactly from native world");
    thread_ok=false;
    check(!read_active_world_folder(folder,error)&&folder.empty()&&error=="wrong native thread"&&string_reads==1,"wrong-thread read changed output or accessed native folder");
    thread_ok=true;world=false;
    check(!read_active_world_folder(folder,error)&&folder.empty()&&string_reads==1,"Title mode read stale game folder");
    world=true;string_ok=false;
    check(!read_active_world_folder(folder,error)&&folder.empty(),"unreadable folder accepted");
    string_ok=true;native_folder="../private";
    check(!read_active_world_folder(folder,error)&&folder.empty(),"unsafe native folder accepted");
    native_folder="World of Stories";
    check(read_active_world_folder(folder,error)&&folder==native_folder&&error.empty(),"recovered native folder failed");
    std::cout<<"native world-folder body: passed active, Title, thread, unreadable and invalid cases\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
