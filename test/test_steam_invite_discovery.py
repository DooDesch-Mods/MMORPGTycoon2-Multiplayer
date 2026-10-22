"""Exercise the production invite-poll prefix and lobby-search function with fake Steam calls."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import re
import os


ROOT = Path(__file__).resolve().parents[1]


def function(text, signature):
    start = text.index(signature)
    opening = text.index("{", start)
    cursor, depth = opening + 1, 1
    while depth:
        depth += (text[cursor] == "{") - (text[cursor] == "}")
        cursor += 1
    return text[start:cursor]


class SteamInviteDiscoveryTests(unittest.TestCase):
    def test_production_invite_and_search_behavior(self):
        compiler = shutil.which(os.environ.get("MT2_STEAM_TEST_CXX", "clang++")) or shutil.which("g++")
        if not compiler:
            self.skipTest("A C++17 compiler is required for the extracted production behavior test")
        source = (ROOT / "src/net/steam_matchmaking.cpp").read_text(encoding="utf-8")
        poll = source[source.index("void SteamMatchmaking::poll() {"):source.index("    // Poll async lobby CREATION result")]
        poll += "}\n"
        search = function(source, "void SteamMatchmaking::request_lobby_list(")
        setter = function(source, "void SteamMatchmaking::set_invite_handler(")
        callback_tables = "\n".join(re.search(r"static void\* g_lobby_" + name + r"_vtable_storage\[5\] = \{.*?\};", source, re.S).group(0) for name in ["join", "chat"])
        domain = (ROOT / "src/game/framework_lobby_domain.inc").read_text(encoding="utf-8")
        admission = function(domain, "void process_steam_invite(")
        fixture = r'''
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#define LOG_STEAM_INFO(...) ((void)0)
#define LOG_STEAM_ERROR(...) (++errors)
#define LOG_STEAM_WARN(...) (++errors)
#define MOD_VERSION "fixture-version"
using LobbyListCallback = std::function<void(bool,const std::vector<int>&)>;
std::atomic<uint64_t> g_pending_invite_lobby{0};
std::chrono::steady_clock::time_point g_next_invite_init{}, g_list_started{};
uint64_t g_pending_list_call=0;
LobbyListCallback g_pending_list_cb;
int errors=0,init_calls=0,requests=0,distance=-1;
bool init_result=true;
std::vector<std::pair<std::string,std::string>> filters;
uint64_t request(void*) { ++requests; return 42; }
void add_filter(void*,const char* key,const char* value,int compare) {
    assert(compare==0);filters.emplace_back(key,value);
}
void add_distance(void*,int value) { distance=value; }
auto fn_RequestLobbyList=&request;
auto fn_AddStringFilter=&add_filter;
auto fn_AddDistanceFilter=&add_distance;
struct SteamMatchmaking {
    void* steam_matchmaking_=nullptr;
    std::function<void(uint64_t)> invite_handler_;
    bool init() { ++init_calls;if(init_result)steam_matchmaking_=this;return init_result; }
    void poll();
    void request_lobby_list(LobbyListCallback);
    void set_invite_handler(std::function<void(uint64_t)>);
};
uint64_t pending_steam_invite=0;
bool framework_open=false,framework_browser=true,pending=false,is_admitted=false,label_ok=true;
void* ui_owner=reinterpret_cast<void*>(7);
std::string last_command,lobby_id_field;
int native_joins=0,native_failures=0;
bool admitted(){return is_admitted;}
void fail(const char*){++native_failures;}
bool label(const char* name,const std::string& value){
    assert(std::string(name)=="lobby_id");if(label_ok)lobby_id_field=value;return label_ok;
}
void dispatch_action(void* owner,std::string command){
    assert(owner==ui_owner && command=="MT2MPJoin");++native_joins;
}
int plain_join=0,result_join=0,plain_chat=0,result_chat=0;
void cb_run_lobby_join(void*,void*){++plain_join;}
void cb_run_lobby_join_result(void*,void*,bool,uint64_t){++result_join;}
int cb_get_size_lobby_join(void*){return 16;}
void cb_run_lobby_chat(void*,void*){++plain_chat;}
void cb_run_lobby_chat_result(void*,void*,bool,uint64_t){++result_chat;}
int cb_get_size_lobby_chat(void*){return 24;}
class CallbackBase {
public:
    virtual void Run(void*)=0;
    virtual void Run(void*,bool,uint64_t)=0;
    virtual int GetCallbackSizeBytes()=0;
    uint8_t flags=0;
    int32_t id=0;
};
void call_plain(CallbackBase* object){object->Run(nullptr);}
void call_result(CallbackBase* object){object->Run(nullptr,false,1);}
'''
        fixture += callback_tables + poll + search + setter + admission + r'''
int main() {
    SteamMatchmaking mm;
    g_pending_invite_lobby=99;
    mm.poll();
    assert(init_calls==0 && g_pending_invite_lobby==99);
    std::vector<uint64_t> received;
    mm.set_invite_handler([&](uint64_t id){received.push_back(id);});
    init_result=false;mm.poll();mm.poll();
    assert(init_calls==1 && received.empty() && g_pending_invite_lobby==99);
    g_next_invite_init={};init_result=true;mm.poll();
    assert(init_calls==2 && received==std::vector<uint64_t>{99});
    assert(g_pending_invite_lobby==0);
    mm.poll();assert(received.size()==1);
    g_pending_invite_lobby=100;mm.poll();
    assert(received==std::vector<uint64_t>({99,100}));
    mm.request_lobby_list({});
    assert(requests==1 && distance==3 && g_pending_list_call==42);
    assert((filters==std::vector<std::pair<std::string,std::string>>({{"mt2mp","1"},{"mt2mp_version","fixture-version"}})));
    int failures=0;
    mm.request_lobby_list([&](bool ok,const std::vector<int>&){assert(!ok);++failures;});
    assert(requests==1 && failures==1 && errors==1);
    g_pending_list_call=0;fn_AddDistanceFilter=nullptr;
    mm.request_lobby_list([&](bool ok,const std::vector<int>&){assert(!ok);++failures;});
    assert(requests==1 && failures==2 && errors==2);
    process_steam_invite(true);
    assert(native_joins==0 && !framework_open);
    pending_steam_invite=123;process_steam_invite(true);process_steam_invite(true);
    assert(native_joins==1 && pending_steam_invite==0 && lobby_id_field=="123");
    assert(framework_open && !framework_browser && last_command=="MT2MPJoin");
    pending_steam_invite=124;pending=true;process_steam_invite(true);
    assert(native_joins==1 && native_failures==1 && pending_steam_invite==0);
    pending_steam_invite=125;pending=false;is_admitted=true;process_steam_invite(true);
    assert(native_joins==1 && native_failures==2 && pending_steam_invite==0);
    pending_steam_invite=126;is_admitted=false;process_steam_invite(false);
    assert(native_joins==1 && native_failures==3 && pending_steam_invite==0);
    pending_steam_invite=127;label_ok=false;process_steam_invite(true);
    assert(native_joins==1 && pending_steam_invite==0 && lobby_id_field=="123");
#ifdef _WIN32
    // Call the actual hand-built production tables through the Microsoft callback ABI.
    struct CallbackObject {void** table;uint8_t flags;uint8_t pad[3];int32_t id;};
    CallbackObject join{g_lobby_join_vtable_storage+2,0,{},333};
    CallbackObject chat{g_lobby_chat_vtable_storage+2,0,{},507};
    call_plain(reinterpret_cast<CallbackBase*>(&join));
    assert(plain_join==1 && result_join==0);
    call_result(reinterpret_cast<CallbackBase*>(&join));
    call_plain(reinterpret_cast<CallbackBase*>(&chat));
    assert(plain_chat==1 && result_chat==0);
    call_result(reinterpret_cast<CallbackBase*>(&chat));
    assert(plain_join==1 && result_join==1 && plain_chat==1 && result_chat==1);
    assert(reinterpret_cast<CallbackBase*>(&join)->GetCallbackSizeBytes()==16);
    assert(reinterpret_cast<CallbackBase*>(&chat)->GetCallbackSizeBytes()==24);
#endif
}
'''
        with tempfile.TemporaryDirectory(prefix="mt2-steam-invite-") as directory:
            path = Path(directory)
            cpp = path / "behavior.cpp"
            cpp.write_text(fixture, encoding="utf-8")
            executable = path / "behavior.exe"
            options = ["-std=c++17"]
            if sys.platform == "win32" and "clang" in Path(compiler).name:
                # The host Clang may precede the installed MSVC STL's compiler-version gate.
                options.append("-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH")
            if Path(compiler).stem.lower() == "cl":
                command = [compiler, "/nologo", "/EHsc", "/std:c++17", str(cpp), "/Fe:" + str(executable), "/Fo:" + str(path / "behavior.obj")]
            else:
                command = [compiler, *options, str(cpp), "-o", str(executable)]
            built = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
