#pragma once
#include "npc_quest_records.h"
#include <map>
#include <set>

namespace mt2mp::game::npc_history_graph {
struct Node {
    uintptr_t object=0,ap=0;
    uint64_t uid=0;
    uint8_t flag=0;
    int32_t cash=0;
    std::string sha;
    bool operator==(const Node&)const=default;
};
struct Graph {
    uintptr_t pointer=0;
    std::vector<Node> nodes;
    int32_t cash=0;
    bool operator==(const Graph&)const=default;
};
// Read only drained operations supplied by the existing native GraphWitness.
// Pointer-bearing bytes remain local lifetime receipts, never wire identities.
template<class Memory,class Hash>
bool read(Memory& memory,uintptr_t image,uintptr_t graph,int32_t count,uint64_t target,
          bool moving,bool restoring,Hash&& hash,Graph& out,std::string& error){
    const auto fail=[&](const char* cause){error=cause;return false;};
    out={};if(!image||!graph||!target||count<1||count>4099)return fail("NPC history original graph extent or target differs");
    const auto at=[&](uintptr_t object,size_t offset,auto& value){return npc_quest_records::get(memory,object,offset,value,"NPC history graph",error);};
    uintptr_t data=0;if(!at(graph,0x20,data))return false;
    if(!data||data>UINTPTR_MAX-size_t(count)*8)return fail("NPC history original graph operation span differs");
    Graph next;next.pointer=graph;std::set<uintptr_t> unique;unsigned targets=0,cash_nodes=0;
    for(int32_t index=0;index<count;++index){Node node;uintptr_t method=0;size_t extent=0;
        if(!at(data,size_t(index)*8,node.object))return false;
        if(!node.object||!unique.insert(node.object).second)return fail("NPC history original graph node is null or repeated");
        if(!at(node.object,0,node.ap)||!at(node.ap,0x10,method))return false;
        if(moving){if(node.ap!=image+0x1786ff0||method!=image+0x55ed80)return fail("NPC Move history contains a foreign original node");extent=0xe8;}
        else if(node.ap==image+0x16de8a0&&method==image+0x556ff0){if(!restoring)return fail("NPC removal history unexpectedly contains MakeNPC");extent=0x188;}
        else if(node.ap==image+0x16de860&&method==image+0x53e1c0){if(restoring)return fail("NPC restoration history unexpectedly contains Destroy");extent=0xc0;}
        else if(node.ap==image+0x1713330&&method==image+0x563f50)extent=0xe0;
        else if(node.ap==image+0x1786fb0&&method==image+0x53ccf0){extent=0xc8;++cash_nodes;
            if(!at(node.object,0xb8,node.cash))return false;
            const auto sum=int64_t(next.cash)+node.cash;if(sum<INT32_MIN||sum>INT32_MAX)return fail("NPC history original Cash sum overflows");next.cash=int32_t(sum);}
        else return fail("NPC history contains an unrelated original operation");
        if(node.ap!=image+0x1786fb0){if(!at(node.object,0xb8,node.uid))return false;
            if(!node.uid)return fail("NPC history original full UID is zero");
            if(node.ap!=image+0x1713330){if(node.uid!=target)return fail("NPC history original graph targets another full UID");++targets;}
        }
        if(node.ap==image+0x1713330){if(!at(node.object,0xc0,node.flag))return false;
            if(node.flag>1||(node.flag&&(!restoring||node.uid!=target)))return fail("NPC history SetNPCQuests own/incoming inverse flag differs");}
        std::string bytes(extent,'\0');if(!memory.copy(node.object,bytes.data(),bytes.size(),error))return false;
        if(node.ap==image+0x16de8a0){std::string name;
            if(!npc_quest_records::read_string(memory,node.object,0xc0,"NPC history Make name",name,error))return false;
            bytes+=std::to_string(name.size())+":"+name;
            uintptr_t stored=0,array_ap=0;int32_t records=0,capacity=0;
            if(!at(node.object,0x170,array_ap)||!at(node.object,0x178,stored)||!at(node.object,0x180,records)||!at(node.object,0x184,capacity))return false;
            if(array_ap!=image+0x17469a0)return fail("NPC history Make stored Quest array class differs");
            if(records<0||records>4096||capacity<records||capacity>100000||(records&&!stored)||stored>UINTPTR_MAX-size_t(records)*0x58)
                return fail("NPC history Make stored Quest array bounds differ");
            for(int32_t row=0;row<records;++row){const auto record=stored+size_t(row)*0x58;std::string raw(0x58,'\0'),name_text,description;
                if(!memory.copy(record,raw.data(),raw.size(),error)||!npc_quest_records::read_string(memory,record,0x18,"NPC history Make Quest name",name_text,error)||
                   !npc_quest_records::read_string(memory,record,0x38,"NPC history Make Quest description",description,error))return false;
                bytes+=raw+std::to_string(name_text.size())+":"+name_text+std::to_string(description.size())+":"+description;}
        }
        if(node.ap==image+0x1713330){uintptr_t records=0;int32_t size=0,capacity=0;
            if(!at(node.object,0xd0,records)||!at(node.object,0xd8,size)||!at(node.object,0xdc,capacity))return false;
            if(size<0||size>4096||capacity<size||capacity>100000||(size&&!records)||records>UINTPTR_MAX-size_t(size)*0x58)
                return fail("NPC history SetNPCQuests stored array bounds differ");
            for(int32_t row=0;row<size;++row){const auto record=records+size_t(row)*0x58;std::string raw(0x58,'\0'),name_text,description;
                if(!memory.copy(record,raw.data(),raw.size(),error)||!npc_quest_records::read_string(memory,record,0x18,"NPC history stored Quest name",name_text,error)||
                   !npc_quest_records::read_string(memory,record,0x38,"NPC history stored Quest description",description,error))return false;
                bytes+=raw+std::to_string(name_text.size())+":"+name_text+std::to_string(description.size())+":"+description;}
        }
        if(!hash(bytes,node.sha,error))return false;
        next.nodes.push_back(std::move(node));
    }
    if(targets!=1||(moving&&count!=1)||cash_nodes>1)return fail("NPC history original target or Cash node is repeated or absent");
    out=std::move(next);error.clear();return true;
}
}
