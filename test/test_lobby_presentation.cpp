#include "lobby_presentation.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::game::lobby_presentation;
void require(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
int main(){
    try{
        require(host_phase(1,false,true,"idle")==Phase::Loading,"selected-save load was not announced");
        require(host_phase(0,true,false,"idle")==Phase::Loading,"native world transition was not announced");
        require(host_phase(0,false,true,"idle")==Phase::Selected,"selected world lost start action");
        require(host_phase(6,false,true,"awaiting_load")==Phase::ClientLoading,"Host did not wait for actual Client load");
        require(host_phase(10,true,true,"transferring")==Phase::Failed,"failure lost precedence over transfer");
        require(host_phase(0,false,false,"idle",true)==Phase::Creating,"native wizard was not announced");
        require(host_phase(0,true,false,"idle",true)==Phase::Loading,"native creation transition lost precedence");
        require(host_phase(10,false,false,"idle",true)==Phase::Failed,"wizard hid a preparation failure");
        for(unsigned i=0;i<=10;++i){Phase phase=Phase::Failed;require(parse_phase(std::to_string(i),phase)&&unsigned(phase)==i,"published phase did not round-trip");}
        for(const auto text:{"","11","-1","010","1x"," 1"}){Phase phase=Phase::Playing;require(!parse_phase(text,phase)&&phase==Phase::Playing,"invalid phase changed the last confirmed state");}
        auto creating=describe(false,true,false,0,Phase::Creating,"idle",0,0,false);
        require(creating.heading=="Host erstellt eine Welt"&&creating.busy,"Client did not see native creation state");
        auto view=describe(false,true,false,0,Phase::Loading,"idle",0,0,false);
        require(view.heading=="Host lädt die Welt"&&view.busy,"Client stayed in generic Host-wait state");
        view=describe(false,true,false,0,Phase::Sending,"transferring",64,128,false);
        require(view.heading=="Welt wird empfangen"&&view.detail=="50 %","Client did not use confirmed received bytes");
        view=describe(true,true,false,6,Phase::Sending,"transferring",0,128,false);
        require(view.detail=="0 %","queued bytes were presented as acknowledged bytes");
        view=describe(false,true,false,0,Phase::Sending,"transferring",0,0,false);
        require(view.detail.empty(),"unknown transfer length invented a percentage");
        view=describe(false,true,false,0,Phase::Sending,"awaiting_load",128,128,false);
        require(view.heading=="Deine Welt wird geladen"&&view.busy,"complete bytes were presented as ready world");
        view=describe(false,true,false,9,Phase::Playing,"active",128,128,false);
        require(view.heading=="Gemeinsam im Spiel"&&!view.busy,"actual Playing did not close loading state");
        view=describe(false,true,false,9,Phase::Playing,"active",128,128,true);
        require(view.heading=="Verbindung unterbrochen","error was hidden by previous Playing state");
        std::cout<<"PASS Host load, actual byte progress, Client load, Playing and failure precedence\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
