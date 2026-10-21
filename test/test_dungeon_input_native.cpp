#include "dungeon_gizmo_control_native.h"
#include <algorithm>
#include "dungeon_input_native.h"
#ifdef _WIN32
#include "dungeon_input_calls.h"
#endif
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
namespace d=mt2mp::game::dungeon_input_native;
int checks=0;
void expect(bool value,const char* name){++checks;if(!value)throw std::runtime_error(name);}
struct Port {
    std::vector<std::string> calls;std::string failure;int32_t price=0;bool owner=true,opened=true,bad_after=false,cleanup_failure=false;
    bool call(const char* name,std::string& error){calls.emplace_back(name);if(failure==name){error=std::string("original ")+name+" failed";return false;}return true;}
    bool preflight(d::Tool,std::string& error){if(!owner){error="original Dungeon owner changed";return false;}return true;}
    bool purchase(std::string& error){return call("purchase",error);}
    bool begin(bool& result,std::string& error){result=opened;return call("begin",error);}
    bool end(std::string& error){if(cleanup_failure){calls.emplace_back("end");error="original EndEdit exception";return false;}return call("end",error);}
    bool create(d::Tool,uintptr_t& result,std::string& error){result=100;return call("create",error);}
    bool edge(uintptr_t,std::string& error){return call("edge",error);}
    bool graph(uintptr_t& result,std::string& error){result=200;return call("graph",error);}
    bool snapshot_paint(uintptr_t,std::string& error){return call("snapshot_paint",error);}
    bool paint(std::string& error){return call("paint",error);}
    bool finish_paint(std::string& error){return call("finish_paint",error);}
    int32_t cost()const{return price;}
    bool cash(uintptr_t,std::string& error){return call("cash",error);}
    bool destroy_inverse(uintptr_t,uintptr_t,std::string& error){return call("destroy",error);}
    bool commit(uintptr_t,std::string& error){return call("commit",error);}
    bool readback(const d::Receipt&,std::string& error){if(bad_after){calls.emplace_back("readback");error="actual scene/ledger result differs";return false;}return call("readback",error);}
};
void run(){std::string error;d::Receipt receipt;Port port;
    expect(d::apply(port,d::Tool::Door,receipt,error)&&receipt.confirmed&&!receipt.unknown,"Door original transaction confirmed only after readback");
    expect(port.calls==std::vector<std::string>{"purchase","begin","create","edge","graph","cash","destroy","commit","end","readback"},"free Door retains original cash inverse and explicit corridor edge");
    const auto count=port.calls.size();expect(!d::apply(port,d::Tool::Door,receipt,error)&&port.calls.size()==count,"confirmed receipt cannot replay");
    port={};receipt={};expect(d::apply(port,d::Tool::Key,receipt,error),"free original Key");
    expect(port.calls==std::vector<std::string>{"purchase","begin","create","graph","destroy","commit","end","readback"},"free Key has no extra cash inverse or Door edge");
    port={};port.price=50;receipt={};expect(d::apply(port,d::Tool::Key,receipt,error)&&port.calls[4]=="destroy"&&port.calls[5]=="cash","paid Key preserves original Destroy then Cash inverse order");
    for(int32_t cost:{0,75}){port={};port.price=cost;port.opened=false;receipt={};
        expect(d::apply(port,d::Tool::Treasure,receipt,error)&&receipt.purchased&&receipt.graph_created&&receipt.confirmed,
            "Treasure original transaction retains free and paid purchase");
        expect(port.calls==std::vector<std::string>{"purchase","begin","create","graph","cash","destroy","commit","readback"},
            "Treasure preserves original Cash then Destroy order including zero cost without invented BeginEdit");}
    port={};receipt={};expect(d::apply(port,d::Tool::Monster,receipt,error)&&!receipt.purchased,"Monster has no invented purchase");
    expect(port.calls==std::vector<std::string>{"begin","create","graph","destroy","commit","end","readback"},"original Monster birth plus retained inverse");
    port={};receipt={};expect(d::apply(port,d::Tool::Paint,receipt,error)&&receipt.paint_saved&&receipt.paint_finished,"Paint captures original inverse before canvas effect");
    expect(port.calls==std::vector<std::string>{"begin","graph","snapshot_paint","paint","finish_paint","commit","end","readback"},"one Paint graph owns complete original paint lifecycle");
    port={};port.owner=false;receipt={};expect(!d::apply(port,d::Tool::Door,receipt,error)&&!receipt.entered&&port.calls.empty(),"changed owner rejected before original effects");
    for(const auto* operation:{"purchase","begin","create","edge","graph","cash","destroy","commit"}){
        port={};port.failure=operation;receipt={};expect(!d::apply(port,d::Tool::Door,receipt,error)&&receipt.unknown&&!receipt.confirmed&&error.find(std::string("original ")+operation+" failed")==0,"original failure retained and never confirmed");
        const auto attempted=port.calls.size();expect(!d::apply(port,d::Tool::Door,receipt,error)&&port.calls.size()==attempted,"partial failed receipt cannot replay");}
    port={};port.failure="create";port.cleanup_failure=true;receipt={};
    expect(!d::apply(port,d::Tool::Key,receipt,error)&&receipt.cause=="original create failed"&&receipt.cleanup_cause=="original EndEdit exception","primary effect and cleanup causes retained separately");
    expect(port.calls.back()=="end"&&receipt.end_attempted,"failed cleanup attempted exactly once");
    port={};port.bad_after=true;receipt={};expect(!d::apply(port,d::Tool::Monster,receipt,error)&&receipt.committed&&receipt.unknown&&!receipt.confirmed,"committed graph with mismatching full readback is Unknown");
    port={};port.opened=false;receipt={};expect(d::apply(port,d::Tool::Monster,receipt,error)&&!receipt.end_attempted,"outer original edit scope is not closed by this adapter");
}
}
#include "gizmo_control_native_cases.inc"
#include "scene_scenery_native_ports.inc"
#include "scene_scenery_native_cases.inc"
int main(){try{run();const auto control_check=[](bool ok,const char* why){expect(ok,why);};gizmo_control_native_fixture::run(control_check);scene_scenery_native_fixture::run(control_check);std::cout<<checks<<" Dungeon input native checks passed\n";return 0;}
catch(const std::exception& exception){std::cerr<<"Dungeon input native: "<<exception.what()<<'\n';return 1;}}
