#pragma once
#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace mt2mp::game::lobby_ui {
inline constexpr size_t page_size=2;
struct Widget {
    std::string id,type,text,group;
    int x=0,y=0,width=0,height=0,max_length=0;
    bool visible=true,disabled=false;
    std::string style="button",tone,asset;
    int size=16;
};

inline std::vector<uint64_t> ordered_members(std::vector<uint64_t> members,uint64_t host){
    std::sort(members.begin(),members.end(),[host](uint64_t a,uint64_t b){
        if((a==host)!=(b==host))return a==host;
        return a<b;
    });
    return members;
}

inline std::map<std::string,Widget> make_layout(){
    std::map<std::string,Widget> result;
    auto add=[&](const char* id,const char* type,int x,int y,int width,int height,const char* text,const char* group="main",int size=16)->Widget& {
        Widget value{id,type,text,group,x,y,width,height};value.size=size;
        return result.emplace(id,std::move(value)).first->second;
    };
    auto art=[&](const char* id,int x,int y,int width,int height,const char* key,const char* group){
        add(id,"image",x,y,width,height,"",group).asset=key;
    };
    auto panel=[&](const char* id,int x,int y,int width,int height,const char* group){
        add(id,"panel",x,y,width,height,"",group).tone="subtle";
    };
    add("status","text",16,10,788,28,"","report");
    add("report","button",620,420,184,36,"Problem melden","shell").style="quiet";
    add("report_heading","text",16,26,788,32,"Multiplayer-Problem melden","report",24);
    add("report_intro","text",16,70,788,32,"Versionen, letzte Fehler und Multiplayer-Aktionen werden dem Bericht beigefügt.","report");
    add("report_details","text",16,114,788,44,"Spielstände, Chat, Screenshots und Steam-Konto-IDs werden nicht gesendet.","report").tone="muted";
    add("report_hint","text",16,170,788,24,"Was ist passiert? (optional)","report");
    add("report_comment","input",16,198,788,48,"","report").max_length=1000;
    add("report_privacy","text",16,262,788,48,"Private Diagnosedaten bleiben 30 Tage gespeichert. Ein öffentlicher Fehlerbericht kann eine anonyme Zusammenfassung enthalten. Deine Beschreibung bleibt privat.","report",14).tone="muted";
    add("report_auto","button",16,322,788,36,"Automatische technische Berichte: Aus","report").style="quiet";
    add("report_state","text",16,370,788,28,"Bericht wird vorbereitet...","report",14);
    add("report_send","button",16,414,440,36,"Bericht privat senden","report");
    add("report_back","button",472,414,332,36,"Zurück zu Multiplayer","report").style="quiet";

    add("entry_heading","text",24,24,772,40,"Gemeinsam eine Welt bauen","entry",24);
    add("entry_state","text",24,68,772,32,"","entry");
    panel("entry_host_panel",24,104,372,208,"entry");
    panel("entry_find_panel",424,104,372,208,"entry");
    art("entry_host_icon",52,116,48,48,"host","entry");
    art("entry_find_icon",452,116,48,48,"world","entry");
    add("entry_host_title","text",116,132,252,32,"Deine Welt","entry",20);
    add("entry_find_title","text",516,132,256,32,"Offene Spiele","entry",20);
    add("entry_host_detail","text",44,188,332,28,"Welt laden oder neu erstellen.","entry");
    add("entry_find_detail","text",444,188,332,28,"Öffentliche Lobbys finden.","entry");
    add("host","button",44,228,332,40,"Mit Freunden hosten","entry");
    add("host_public","button",44,272,332,32,"Öffentlich hosten","entry").style="quiet";
    add("search","button",444,260,332,44,"Spiel finden","entry");
    add("lobby_id_hint","text",24,346,100,28,"Lobby-ID","entry");
    add("lobby_id","input",128,340,466,36,"","entry").max_length=20;
    add("join","button",610,340,186,36,"Beitreten","entry").style="quiet";

    art("world_icon",24,12,48,48,"world","session");
    add("world","text",88,24,420,64,"Eure Welt","session",24);
    panel("load_panel",24,112,484,190,"session");
    add("load_status","text",40,136,452,48,"","session",22);
    add("load_detail","text",40,184,452,60,"","session");
    add("pick","button",40,252,452,36,"Welt auswählen","session");
    // Retain the legacy action identity for diagnostic compatibility; it is never shown.
    add("start","button",40,252,452,36,"Gemeinsam laden","legacy");
    add("players_heading","text",548,24,248,32,"Mitspieler","party",20);
    for(size_t i=0;i<4;++i){
        const int y=64+int(i)*52;const auto suffix=std::to_string(i);
        art(("avatar"+suffix).c_str(),548,y,40,40,"person","party");
        add(("player"+suffix).c_str(),"text",600,y+8,196,40,"Freier Platz","party");
    }
    add("invite","button",548,272,248,36,"Freunde einladen","party").style="quiet";
    art("chat_icon",24,304,24,24,"chat","session");
    add("chat_status","text",56,308,452,24,"Chat","session");
    add("chat_transcript","text",24,336,772,48,"","session",14);
    add("chat_input","input",24,384,676,32,"","session").max_length=240;
    add("chat_send","button",714,384,82,32,"Senden","session");
    add("leave","button",16,420,212,36,"Lobby verlassen","shell").style="quiet";

    art("browser_icon",24,16,40,40,"world","browser");
    add("browser_heading","text",88,24,420,40,"Spiel finden","browser",24);
    add("list_status","text",24,80,484,48,"Öffentliche Lobbys","browser");
    for(size_t i=0;i<page_size;++i){
        const int y=152+int(i)*80;const auto n=std::to_string(i);
        panel(("list_panel"+n).c_str(),24,y,484,68,"picker");
        art(("list_icon"+n).c_str(),40,y+18,32,32,"world","picker");
        add(("list_text"+n).c_str(),"text",88,y+20,216,40,"","picker");
        add(("list"+n).c_str(),"button",320,y+16,180,36,"Gemeinsam laden","picker");
        panel(("find_panel"+n).c_str(),24,y,772,68,"find");
        art(("find_icon"+n).c_str(),40,y+14,40,40,"world","find");
        add(("find_text"+n).c_str(),"text",96,y+8,476,52,"","find");
        add(("find"+n).c_str(),"button",588,y+16,208,36,"Beitreten","find");
    }
    add("new_world","button",24,308,484,32,"Neue Welt erstellen","picker").style="quiet";
    add("list_refresh","button",24,348,176,36,"Aktualisieren","browser").style="quiet";
    add("list_previous","button",212,348,136,36,"Vorherige","browser").style="quiet";
    add("list_next","button",360,348,136,36,"Nächste","browser").style="quiet";
    add("list_back","button",16,420,212,36,"Zurück","find").style="quiet";
    return result;
}
}
