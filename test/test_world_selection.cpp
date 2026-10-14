#ifdef NDEBUG
#undef NDEBUG
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <cstdio>
#include "src/game/world_selection.h"
namespace world_selection=mt2mp::game::world_selection;
std::vector<std::string> saved_worlds;
std::map<std::string,std::vector<std::string>> listing;
std::set<std::string> folders;
std::string fail_path;
int freed=0,allocated=0;bool exports=true;
char** fake_enum(const char* path){
 if(fail_path==path)return nullptr;
 const auto& rows=listing[path];auto** result=new char*[rows.size()+1];
 for(size_t i=0;i<rows.size();++i){result[i]=new char[rows[i].size()+1];std::memcpy(result[i],rows[i].c_str(),rows[i].size()+1);}
 result[rows.size()]=nullptr;++allocated;return result;
}
void fake_free(void* p){auto** values=static_cast<char**>(p);for(auto** v=values;*v;++v)delete[] *v;delete[] values;++freed;}
int fake_dir(const char* path){return folders.contains(path);}
HMODULE fake_module(const wchar_t*){return reinterpret_cast<HMODULE>(1);}
FARPROC fake_proc(HMODULE,const char* name){
 if(!exports)return nullptr;
 if(!std::strcmp(name,"PHYSFS_enumerateFiles"))return reinterpret_cast<FARPROC>(&fake_enum);
 if(!std::strcmp(name,"PHYSFS_freeList"))return reinterpret_cast<FARPROC>(&fake_free);
 if(!std::strcmp(name,"PHYSFS_isDirectory"))return reinterpret_cast<FARPROC>(&fake_dir);
 return nullptr;
}
#define GetModuleHandleW fake_module
#define GetProcAddress fake_proc
#include "world_selection_body.inc"
int main(){
 using namespace world_selection;
 assert(valid_name("Guardian of Tales"));assert(valid_name("Wüste"));
 for(auto name:{"", ".", "..", "a/b", "a\\b", "a\nb", "a\"b"})assert(!valid_name(name));
 assert(!valid_name(std::string(201,'x')));assert(save_file("autosave.vrz"));
 assert(!save_file("autosave.meta.vrz"));assert(!save_file("empty.vrz/"));
 std::string error;assert(refresh_saved_worlds(error)&&error.empty()&&saved_worlds.empty());
 folders={"user/save","user/save/Zeta","user/save/Alpha","user/save/Meta"};
 listing["user/save"]={"Zeta","Alpha","Meta","../escape","a-file"};
 listing["user/save/Zeta"]={"autosave.meta.vrz","autosave.vrz"};
 listing["user/save/Alpha"]={"initial_state.vrz"};listing["user/save/Meta"]={"autosave.meta.vrz"};
 assert(refresh_saved_worlds(error)&&saved_worlds==std::vector<std::string>({"Alpha","Zeta"}));assert(freed==allocated);
 fail_path="user/save/Zeta";assert(!refresh_saved_worlds(error)&&error.find("world=Zeta")!=std::string::npos);assert(freed==allocated);
 fail_path="user/save";assert(!refresh_saved_worlds(error)&&error.find("directory enumeration")!=std::string::npos);assert(freed==allocated);
 fail_path.clear();exports=false;assert(!refresh_saved_worlds(error)&&error.find("exports unavailable")!=std::string::npos);
 std::puts("PASS actual save enumeration body: empty, sorting, multiple saves, metadata-only, invalid names, cleanup and distinct failures");
}
