"""Exercise the production Framework transcript against acknowledged Steam slots."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class FrameworkChatNamesTests(unittest.TestCase):
    def test_current_binding_names_and_retired_conversation(self):
        compiler = shutil.which(os.environ.get("MT2_STEAM_TEST_CXX", "clang++")) or shutil.which("g++")
        if not compiler:
            self.skipTest("A C++17 compiler is required for the production transcript test")
        source = (ROOT / "src/game/framework_lobby_domain.inc").read_text(encoding="utf-8")
        body = source[source.index("std::array<std::string,5> chat_speaker_names("):source.index("bool send_chat()")]
        integration = (ROOT / "src/game/native_lobby_framework.inc").read_text(encoding="utf-8")
        descriptors = integration[integration.index("        nlohmann::json widgets=nlohmann::json::array();"):integration.index('        auto response=framework_request("ui.native"')]
        fixture = r'''
#include "vendor/nlohmann/json.hpp"
#include "lobby_layout.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
struct Config {
    bool local_test=false;
    static Config& instance(){static Config value;return value;}
};
namespace net {
namespace chat {
enum class Delivery {Queued,Failed,Received,Confirmed};
struct Entry {uint8_t origin;std::string text;Delivery delivery=Delivery::Received;};
struct Snapshot {
    bool connected=true;uint64_t session=123;uint8_t local_peer=1;
    std::vector<Entry> entries;uint64_t conversation=4;
};
}
namespace session_wire {
bool binding_ok=true;
uint64_t active_session=123,local=1;
int binding_reads=0;
std::array<uint64_t,5> slots{0,1001,2001,0,0};
bool read_binding(std::array<uint64_t,5>& ids,uint64_t& lobby,uint64_t& mine,std::string& error){
    ++binding_reads;if(!binding_ok){error="physical handshake incomplete";return false;}
    ids=slots;lobby=999;mine=local;return true;
}
uint64_t session_id(){return active_session;}
}
struct SteamNetworking {
    std::map<uint64_t,std::string> names{{1001,"HostName"},{2001,"Pascal"}};
    static SteamNetworking& instance(){static SteamNetworking value;return value;}
    std::string get_persona_name(uint64_t id){return names.count(id)?names.at(id):std::to_string(id);}
};
}
'''
        fixture += body + r'''
const auto framework_widgets=mt2mp::game::lobby_ui::make_layout();
bool framework_effectively_visible(const mt2mp::game::lobby_ui::Widget& widget){return widget.visible;}
nlohmann::json descriptor(){
''' + descriptors + r'''
    return widgets;
}
int main(){
    const auto widgets=descriptor();bool chat_found=false;
    for(const auto& item:widgets){
        if(item.at("id")=="chat_input"){
            assert(item.at("submit")=="chat_send" && item.at("type")=="input");chat_found=true;
        }else assert(!item.contains("submit"));
    }
    assert(chat_found);
    using net::chat::Delivery;
    net::chat::Snapshot s;
    s.entries={{1,"Hello",Delivery::Queued},{2,"Hi"}};
    assert(chat_transcript(s)=="You: Hello - Sending\nPascal: Hi");
    net::SteamNetworking::instance().names[2001]="Renamed";
    assert(chat_transcript(s)=="You: Hello - Sending\nRenamed: Hi");
    // Failed bindings and disconnected sessions preserve names already observed.
    net::session_wire::binding_ok=false;
    assert(chat_transcript(s)=="You: Hello - Sending\nRenamed: Hi");
    s.connected=false;int before=net::session_wire::binding_reads;
    assert(chat_transcript(s)=="You: Hello - Sending\nRenamed: Hi");
    assert(net::session_wire::binding_reads==before);
    // A new conversation must not attribute a reused slot to its former player.
    s.conversation++;s.connected=true;
    assert(chat_transcript(s)=="You: Hello - Sending\nPlayer 2: Hi");
    // Physical/session mismatches never borrow names from another epoch or peer.
    net::session_wire::binding_ok=true;net::session_wire::active_session++;
    assert(chat_transcript(s)=="You: Hello - Sending\nPlayer 2: Hi");
    net::session_wire::active_session=s.session;net::session_wire::local=2;
    assert(chat_transcript(s)=="You: Hello - Sending\nPlayer 2: Hi");
    // Client sees Host persona and its own local delivery suffix.
    s.local_peer=2;s.entries={{1,"Hello"},{2,"Hi",Delivery::Failed}};
    assert(chat_transcript(s)=="HostName: Hello\nYou: Hi - Not delivered");
    // Local peers sharing one Steam account keep independent player labels.
    Config::instance().local_test=true;s.conversation++;
    before=net::session_wire::binding_reads;
    assert(chat_transcript(s)=="Host: Hello\nYou: Hi - Not delivered");
    assert(net::session_wire::binding_reads==before);
    // Missing Steam persona falls back without displaying a long account ID.
    Config::instance().local_test=false;s.conversation++;net::session_wire::slots[1]=3001;
    assert(chat_transcript(s)=="Host: Hello\nYou: Hi - Not delivered");
    s.entries={{1,"older"},{1,"one"},{2,"two"},{4,"three"}};
    assert(chat_transcript(s)=="Host: one\nYou: two\nPlayer 4: three");
}
'''
        with tempfile.TemporaryDirectory(prefix="mt2-chat-names-") as directory:
            path = Path(directory)
            cpp, exe = path / "behavior.cpp", path / "behavior.exe"
            cpp.write_text(fixture, encoding="utf-8")
            if Path(compiler).stem.lower() == "cl":
                command = [compiler, "/nologo", "/EHsc", "/std:c++17", "/I" + str(ROOT / "include"), "/I" + str(ROOT / "src/game"), str(cpp), "/Fe:" + str(exe), "/Fo:" + str(path / "behavior.obj")]
            else:
                options = ["-std=c++17"]
                if sys.platform == "win32" and "clang" in Path(compiler).name:
                    options.append("-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH")
                command = [compiler, *options, "-I" + str(ROOT / "include"), "-I" + str(ROOT / "src/game"), str(cpp), "-o", str(exe)]
            built = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == "__main__":
    unittest.main()
