#include "npc_quest_list_fixture.h"
#include "building_npc_key.h"
#include <iostream>
#include <memory>
#include "npc_quest_removal_cases.inc"
int main()try{
    size_t checks=0;std::string error;std::vector<uint8_t> bytes;auto decoded_storage=std::make_unique<Message>();auto& decoded=*decoded_storage;
    const auto check=[&](bool okay,const std::string& label){++checks;need(okay,label+" cause="+error);};
    const std::string key=mt2mp::game::building_npc_key::make(owner_key);
    for(bool host_result:{false,true}){
        auto message_storage=std::make_unique<Message>(host_result?result():request());auto& message=*message_storage;message.request.canonical_record_key=key;
        check(encode(message,bytes,error),"Building child NPC Quest owner encoding");
        check(decode(bytes.data(),bytes.size(),decoded,error)&&decoded==message,"Building child NPC complete typed packet roundtrip");
        for(const auto& invalid:std::vector<std::string>{"building-npc:","building-npc:"+key,"building-npc:action:bad"}){
            auto bad_storage=std::make_unique<Message>(message);auto& bad=*bad_storage;bad.request.canonical_record_key=invalid;
            check(!encode(bad,bytes,error),"foreign or nested child canonical parent accepted");
        }
    }
    for(const auto action:{Action::MoveNPC,Action::RemoveNPC}){
        auto message_storage=std::make_unique<Message>(request());auto& message=*message_storage;message.request.action=action;message.request.npc_quest_list={};message.request.canonical_record_key=key;
        if(action==Action::MoveNPC)message.request.building.alive=true;
        check(encode(message,bytes,error),"Building child NPC move/remove target encoding");
        check(decode(bytes.data(),bytes.size(),decoded,error)&&decoded==message,"Building child NPC move/remove actual target roundtrip");
        message.request.target_action_uuid="33333333-3333-4333-8333-333333333333";
        check(!encode(message,bytes,error),"Building child NPC key accepted with an unrelated free NPC target UUID");
    }
    npc_quest_removal_cases(checks);
    std::cout<<"Building child NPC actual protocol "<<checks<<" comparisons passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
