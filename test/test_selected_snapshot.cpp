#include "selected_snapshot.h"
BOOL WINAPI snapshot_fixture_set_information(HANDLE,FILE_INFO_BY_HANDLE_CLASS,LPVOID,DWORD);
#define SetFileInformationByHandle snapshot_fixture_set_information
#include "save_companion_retirement.h"
#undef SetFileInformationByHandle
#include "mt2mp/game/native_engine.h"
#include <iostream>
#include <random>
#include <set>
using json=nlohmann::json;
namespace fs=std::filesystem;
namespace mt2mp {
struct Config {bool is_host=true;static Config& instance(){static Config c;return c;}};
namespace net {inline void invalidate_snapshot_world(const std::string&);}
}
using namespace mt2mp;
namespace fixture {
std::set<size_t> rename_residues;
fs::path root;std::vector<std::string> calls;bool title=true,title_pending=false,reject_prepare=false,reject_begin=false,reject_request=false;
unsigned checks=0;
void need(bool b,const std::string& e){++checks;if(!b)throw std::runtime_error(e);}
void write(const fs::path& p,const std::string& s){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f<<s;f.close();need(bool(f),"fixture write failed");}
std::string read(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
std::string uuid(unsigned n){auto s=std::string("00000000-0000-4000-8000-000000000000");s.replace(s.size()-std::to_string(n).size(),std::to_string(n).size(),std::to_string(n));return s;}
}
BOOL WINAPI snapshot_fixture_set_information(HANDLE handle,FILE_INFO_BY_HANDLE_CLASS kind,LPVOID data,DWORD length){
    if(kind==FileRenameInfo){
        fixture::need(data&&length>=offsetof(FILE_RENAME_INFO,FileName),"rename information header missing");
        const auto* info=static_cast<const FILE_RENAME_INFO*>(data);
        fixture::need(info->FileNameLength%sizeof(wchar_t)==0,"rename filename has partial UTF-16 code unit");
        fixture::need(size_t(length)>=offsetof(FILE_RENAME_INFO,FileName)+info->FileNameLength+sizeof(wchar_t),
                      "rename buffer does not reserve the filename terminator");
        const auto characters=info->FileNameLength/sizeof(wchar_t);
        fixture::need(info->FileName[characters]==wchar_t{},"rename filename is not NUL terminated");
        fixture::rename_residues.insert(characters%4);
    }
    return ::SetFileInformationByHandle(handle,kind,data,length);
}
namespace mt2mp::game {
namespace native {
static_assert(WorldStatus{}.saves_in_progress==-1);
bool read_world_status(WorldStatus& w,std::string&){w={};w.active_mode_index=fixture::title?2:1;w.requested_mode_index=2;
    w.title_transition_pending=fixture::title_pending;fixture::need(w.saves_in_progress==-1&&!w.world_present,"actual Title defaults changed");return true;}
bool request_load(const std::string& p,std::string& e){fixture::calls.push_back("request:"+p);if(fixture::reject_request){e="original request rejected";return false;}return true;}
}
namespace native_open_runtime {
bool prepare(const fs::path&,const std::string& id,const std::string& sha,uint64_t bytes,uint64_t& generation,std::string& e,const std::string& world){
    fixture::calls.push_back("prepare");generation=17;
    if(fixture::reject_prepare){e="original native pin rejected";return false;}
    const auto path=net::snapshot_world_path::physical(fixture::root,world,id);std::string actual;uint64_t length=0;
    fixture::need(net::hash_snapshot_file(path,actual,length,e)&&actual==sha&&length==bytes,"prepared wrong native bytes");
    const auto companion=fs::path(path.wstring()+L".monsters.json");
    if(fs::exists(companion)){monster_snapshot_companion::Manifest m;fixture::need(monster_snapshot_companion::decode(fixture::read(companion),sha,bytes,m,e),e);
        fixture::need(m.rows.at(0).key=="action:11111111-1111-4111-8111-111111111111","original action key lost before prepare");}
    return true;
}
bool retire_unstarted(uint64_t n,std::string&){fixture::need(n==17,"retired foreign generation");fixture::calls.push_back("retire");return true;}
bool before_request(uint64_t n,std::string&){fixture::need(n==17,"requested foreign generation");fixture::calls.push_back("before_request");return true;}
}
namespace normal_load_identity {
bool begin(const std::string&,const std::string&,std::string& e){fixture::calls.push_back("begin");if(fixture::reject_begin){e="original begin failed";return false;}return true;}
void cancel(const std::string&){fixture::calls.push_back("cancel");}
}
namespace network_identity {void invalidate_world(const std::string&){fixture::calls.push_back("network_invalidate");}}
}
namespace mt2mp::net {inline void invalidate_snapshot_world(const std::string&){fixture::calls.push_back("snapshot_invalidate");}}
json status(){return {{"write_dir",net::snapshot_world_path::utf8(fixture::root)}};}
bool suspend_time(const std::string&,std::string&){fixture::calls.push_back("suspend");return true;}
void write_json(const fs::path& p,const json& j){fixture::write(p,j.dump());}
struct Watch {void begin(int){fixture::calls.push_back("pending");}}load_watch;
json pending_load;std::string last_request;
#define LOG_ERROR(...) ((void)0)
#define LOG_INFO(...) ((void)0)
#include "selected_snapshot_lobby_actual.inc"
void execute(const json& args,const std::string& id,bool internal_request=true){
    const std::string operation="load_snapshot";json request={{"args",args}},response;
    const auto dir=fixture::root/"diagnostics";const auto runtime_root=fixture::root;const int now=0;
    #include "selected_snapshot_load_actual.inc"
    if(response.contains("error"))throw std::runtime_error(response["error"]["message"].get<std::string>());
}
#include "test_save_companion_retirement.inc"
int main(int argc,char** argv)try{
    fixture::need(argc==2,"fixture root required");fixture::root=fs::absolute(argv[1])/(std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));fixture::need(!fs::exists(fixture::root),"fresh fixture directory required");
    const auto source=fixture::root/"save"/"Selected"/"chosen.vrz";fixture::write(source,"exact original world bytes");
    fixture::write(fixture::root/"save"/"Selected"/"chosen.meta.vrz","exact original metadata");
    std::string sha,e,text;uint64_t bytes=0;fixture::need(net::hash_snapshot_file(source,sha,bytes,e),e);
    game::monster_snapshot_companion::Manifest manifest{sha,bytes,{{67,sha+":world:monster:37","action:11111111-1111-4111-8111-111111111111"}}};
    fixture::need(game::monster_snapshot_companion::encode(manifest,text,e),e);const auto companion=fs::path(source.wstring()+L".monsters.json");fixture::write(companion,text);
    const json selected={{"selected_file","save/Selected/chosen.vrz"}};
    game::lobby_operations::configure("owned-run",[](){return true;});std::string operation;
    fixture::need(game::lobby_operations::submit("load_snapshot",selected,operation,e),e);json queued;
    fixture::need(game::lobby_operations::take_request(queued)&&queued.at("args")==selected,"actual lobby lost selected source");
    fixture::need(!game::lobby_operations::submit("load_snapshot",selected,operation,e)&&e.find("already pending")!=std::string::npos,"actual lobby replayed pending operation");
    fixture::need(game::lobby_operations::receive({{"id",queued.at("id")},{"ok",true}})&&game::lobby_operations::take_response(queued.at("id").get<std::string>(),queued),"actual lobby completion failed");
    fixture::need(!game::lobby_operations::submit("load_snapshot",{{"selected_file","save/Selected/chosen.vrz"},{"snapshot_id",fixture::uuid(1)}},operation,e),"actual lobby admitted ambiguous source");
    execute(selected,fixture::uuid(1));
    fixture::need(fixture::calls==std::vector<std::string>{"prepare","suspend","network_invalidate","snapshot_invalidate","begin","before_request","request:save/Selected/snapshot-"+fixture::uuid(1)+".vrz","pending"},"actual selected owned transaction order differs");
    const auto out=net::snapshot_world_path::physical(fixture::root,"Selected",fixture::uuid(1));
    fixture::need(fixture::read(out)==fixture::read(source)&&fixture::read(fs::path(out.wstring()+L".monsters.json"))==text,"copy changed native/companion");
    auto meta=out;meta.replace_extension(".meta.vrz");fixture::need(fixture::read(meta)=="exact original metadata","original metadata lost");
    auto rejected=[&](auto call,const std::string& cause){fixture::calls.clear();bool failed=false;try{call();}catch(const std::exception& x){failed=std::string(x.what()).find(cause)!=std::string::npos;}fixture::need(failed,"missing original failure: "+cause);};
    rejected([&]{execute(selected,fixture::uuid(1));},"already exists");fixture::need(fixture::calls.empty(),"collision reached native");
    rejected([&]{execute(selected,fixture::uuid(2),false);},"internal Host");
    Config::instance().is_host=false;rejected([&]{execute(selected,fixture::uuid(2));},"internal Host");Config::instance().is_host=true;
    fixture::title=false;rejected([&]{execute(selected,fixture::uuid(2));},"requires Title active_mode=1 required=2");fixture::title=true;
    fixture::need(fixture::calls.empty()&&!fs::exists(net::snapshot_world_path::physical(fixture::root,"Selected",fixture::uuid(2))),"wrong mode copied or requested load");
    fixture::title_pending=true;rejected([&]{execute(selected,fixture::uuid(2));},"requires idle Title title_transition_pending=1 required=0");fixture::title_pending=false;
    fixture::need(fixture::calls.empty()&&!fs::exists(net::snapshot_world_path::physical(fixture::root,"Selected",fixture::uuid(2))),"pending Title copied or requested load");
    for(const auto path:{"save/../chosen.vrz","save/Selected/../chosen.vrz","save/Selected/chosen.meta.vrz","C:/chosen.vrz"})
        rejected([&]{execute({{"selected_file",path}},fixture::uuid(2));},"selected snapshot");
    auto malformed=manifest;malformed.native_sha256=std::string(64,'b');malformed.rows.clear();fixture::need(game::monster_snapshot_companion::encode(malformed,text,e),e);fixture::write(companion,text);
    rejected([&]{execute(selected,fixture::uuid(2));},"native file SHA/length differs");fixture::need(fixture::calls.empty()&&!fs::exists(net::snapshot_world_path::physical(fixture::root,"Selected",fixture::uuid(2))),"bad companion copied or reached native");
    fixture::need(game::monster_snapshot_companion::encode(manifest,text,e),e);fixture::write(companion,text);
    fixture::reject_prepare=true;rejected([&]{execute(selected,fixture::uuid(3));},"original native pin rejected");fixture::reject_prepare=false;fixture::need(fixture::calls==std::vector<std::string>{"prepare"},"rejected prepare attempted load");
    fixture::reject_begin=true;rejected([&]{execute(selected,fixture::uuid(4));},"original begin failed");fixture::reject_begin=false;fixture::need(fixture::calls.back()=="retire","failed binding did not retire unstarted generation");
    fixture::reject_request=true;rejected([&]{execute(selected,fixture::uuid(7));},"original request rejected");fixture::reject_request=false;
    fixture::need(fixture::calls.back()=="cancel"&&std::count(fixture::calls.begin(),fixture::calls.end(),"pending")==0,"original load rejection hidden or retried");
    fixture::calls.clear();execute({{"snapshot_id",fixture::uuid(1)}},fixture::uuid(5));fixture::need(fixture::calls.front()=="prepare"&&fixture::calls.back()=="pending","existing pinned UUID route changed");
    fixture::write(fixture::root/"save"/"Legacy"/"bare.vrz","bare native world");fixture::calls.clear();execute({{"selected_file","save/Legacy/bare.vrz"}},fixture::uuid(6));fixture::need(fixture::calls.back()=="pending","legacy absent companion rejected");
    fixture::need(fixture::read(source)=="exact original world bytes"&&fixture::read(companion)==text,"selected originals were modified");
    quit_fixture::cases();
    std::cout<<fixture::checks<<" selected snapshot actual copy/load transaction checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}
