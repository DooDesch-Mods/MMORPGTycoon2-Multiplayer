#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace mt2mp::game::lobby_presentation {
enum class Phase : uint8_t { Choosing, Selected, Loading, Preparing, Sending, ClientLoading, Starting, Playing, Leaving, Failed, Creating };
inline bool parse_phase(std::string_view value,Phase& phase){
    if(value.size()==1&&value[0]>='0'&&value[0]<='9'){phase=Phase(value[0]-'0');return true;}
    if(value=="10"){phase=Phase::Creating;return true;}
    return false;
}
inline Phase host_phase(int preparation, bool transitioning, bool selected, const std::string& snapshot,bool creating=false) {
    if(preparation==10)return Phase::Failed;
    if(preparation==12)return Phase::Leaving;
    if(preparation==9)return Phase::Playing;
    if(snapshot=="awaiting_load")return Phase::ClientLoading;
    if(snapshot=="transferring"||snapshot=="awaiting_receivers"||snapshot=="awaiting_verification")return Phase::Sending;
    if(preparation==8||preparation==7)return Phase::Starting;
    if(preparation==1||preparation==4||transitioning)return Phase::Loading;
    if(preparation!=0)return Phase::Preparing;
    if(creating)return Phase::Creating;
    return selected?Phase::Selected:Phase::Choosing;
}
struct Message {std::string heading,detail;bool busy=false;};
inline Message describe(bool host, bool member, bool pending, int preparation, Phase remote,
                        const std::string& snapshot, uint64_t done, uint64_t total, bool failed) {
    if(failed||preparation==10)return {"Verbindung unterbrochen","Verlasse die Sitzung und verbinde dich neu."};
    if(!member)return pending?Message{"Verbindung wird hergestellt","" ,true}:Message{"Gemeinsam bauen",""};
    if(preparation==12)return {"Sitzung wird beendet","",true};
    if(snapshot=="transferring"&&total){
        const auto percent=done>=total?100:unsigned((done*100)/total);
        return {host?"Welt wird gesendet":"Welt wird empfangen",std::to_string(percent)+" %",true};
    }
    if(snapshot=="awaiting_verification")return {"Welt wird geprüft","",true};
    if(snapshot=="awaiting_load")return {host?"Mitspieler lädt die Welt":"Deine Welt wird geladen",host?"Danach startet ihr gemeinsam.":"Du trittst automatisch bei.",true};
    if(preparation==9)return {"Gemeinsam im Spiel",""};
    const auto phase=remote;
    switch(phase){
    case Phase::Creating:return {host?"Erstelle eure Welt":"Host erstellt eine Welt",host?"Schließe den Assistenten ab.":"Du trittst danach automatisch bei.",true};
    case Phase::Choosing:return {host?"Wähle eure Welt":"Host wählt eine Welt",host?"Lade einen Spielstand oder erstelle eine neue Welt.":"Du trittst danach automatisch bei."};
    case Phase::Selected:return {host?"Bereit zum Start":"Host hat eine Welt gewählt",host?"Starte die Welt für alle Spieler.":"Der Host startet eure Welt."};
    case Phase::Loading:return {host?"Deine Welt wird geladen":"Host lädt die Welt",host?"Deine Mitspieler treten automatisch bei.":"Du trittst automatisch bei.",true};
    case Phase::Preparing:return {"Gemeinsame Welt wird vorbereitet","",true};
    case Phase::Sending:return {host?"Welt wird gesendet":"Welt wird übertragen","",true};
    case Phase::ClientLoading:return {host?"Mitspieler lädt die Welt":"Deine Welt wird geladen","",true};
    case Phase::Starting:return {"Spiel wird gestartet","",true};
    case Phase::Playing:return {"Verbindung wird abgeschlossen","",true};
    case Phase::Leaving:return {"Host hat die Sitzung verlassen","Verlasse die Lobby, um ein neues Spiel zu beginnen."};
    case Phase::Failed:return {"Sitzung unterbrochen","Verlasse die Lobby und verbinde dich neu."};
    }
    return {"Verbindung wird hergestellt","",true};
}
}
