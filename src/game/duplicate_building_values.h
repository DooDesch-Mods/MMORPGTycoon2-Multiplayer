#pragma once
#include "duplicate_localized_history.h"
#include "duplicate_owned_history.h"

namespace mt2mp::game::duplicate_building_values {
// GetFluff returns an owned GNU vsLocString. Cleanup attempts both native
// owners once and retains an inspection failure as the original cause.
struct LocalizedReceipt {
    bool returned=false,read=false,arguments_destroyed=false,text_destroyed=false;
    std::string cleanup_cause;
};
template<class Port>
bool localized(Port& port,uintptr_t building,std::string& sha,LocalizedReceipt& receipt,std::string& e){
    sha.clear();receipt={};uintptr_t pointer=0;
    if(!port.get(building,pointer,e))return false;
    receipt.returned=true;duplicate_localized_history::Value value;
    try{receipt.read=port.read(pointer,value,e)&&duplicate_localized_history::summarize(value,sha,e);}
    catch(const std::exception& ex){e=std::string("Duplicate original localized description inspection failed: ")+ex.what();}
    if(!receipt.read&&e.empty())e="Duplicate original localized description inspection not confirmed";
    auto cleanup=[&](const char* label,bool arguments,bool& done){std::string why;
        if(port.destroy(pointer,arguments,why)){done=true;return;}
        if(why.empty())why=std::string("Duplicate localized ")+label+" cleanup not confirmed";
        if(!receipt.cleanup_cause.empty())receipt.cleanup_cause+="; ";receipt.cleanup_cause+=why;
    };
    cleanup("arguments",true,receipt.arguments_destroyed);cleanup("text",false,receipt.text_destroyed);
    if(!receipt.cleanup_cause.empty()){if(!e.empty())e+="; cleanup: ";e+=receipt.cleanup_cause;}
    if(!receipt.read||!receipt.arguments_destroyed||!receipt.text_destroyed){sha.clear();return false;}
    return true;
}

enum class OwnedKind:uint8_t {Layout=1,Plan=2,Bugs=3};
struct Methods {uintptr_t addresspoint=0,prepare=0,save=0;};
inline bool methods(OwnedKind kind,Methods& out,std::string& e){
    if(kind==OwnedKind::Layout)out={0x1712a60,0xf8c370,0xf8c3d0};
    else if(kind==OwnedKind::Plan)out={0x16fea40,0xf29ab0,0xf29b10};
    else if(kind==OwnedKind::Bugs)out={0x1705640,0xf5d680,0xf5d6e0};
    else return npc_quest_targets::fail(e,"Duplicate original owned payload kind invalid");
    return true;
}
template<class Memory,class Capture>
bool live(Memory& memory,uintptr_t image,uintptr_t object,OwnedKind kind,Capture&& capture,std::string& sha,std::string& e){
    sha.clear();Methods original;
    if(!methods(kind,original,e)||!npc_quest_targets::typed(memory,image,object,original.addresspoint,e))return false;
    subscriber_lifecycle::Graph graph;
    if(!capture(object,original.prepare,original.save,graph,e))return false;
    return duplicate_owned_history::captured(graph,sha,e);
}
inline bool stored(subscriber_lifecycle::Memory& memory,uintptr_t record,const subscriber_lifecycle::Budget& budget,
                   std::string& sha,std::string& e){
    sha.clear();std::vector<subscriber_lifecycle::Record> rows;
    return subscriber_lifecycle::read_owned_record(memory,record,budget,rows,e)&&duplicate_owned_history::summarize(rows,sha,e);
}
}
