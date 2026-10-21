#include "dungeon_building_children.h"
#include <iostream>
#include <stdexcept>
namespace dungeon_building_children_fixture {
namespace flow=mt2mp::game::dungeon_building_children;
namespace child=mt2mp::game::building_npc_lifecycle;
namespace qr=mt2mp::game::monster_quest_retirement;
namespace history=mt2mp::game::duplicate_quest_history;
const std::string parent_key="action:11111111-1111-4111-8111-111111111111";
const std::string request_id="22222222-2222-4222-8222-222222222222";
constexpr uintptr_t parent_pointer=0x10000,child_pointer=0x20000,restored_pointer=0x30000;
constexpr uint64_t parent_uid=0x100000001,child_uid=0x200000001;
child::Receipt receipt(){child::Receipt row;
    row.parent={parent_key,parent_pointer,parent_uid,3,child::MakeForm::Functional,child_uid,parent_key+"/template"};
    row.lease={"snapshot",request_id,request_id,5,6};row.present=true;
    row.child.key="building-npc:"+parent_key;row.child.row.local_pointer=child_pointer;row.child.row.local_uid=child_uid;
    row.child.row.type=0;row.original_record_sha=std::string(64,'a');row.inverse_child_uid=child_uid;return row;
}
qr::Witness witness(){qr::Witness result;const auto source=receipt();qr::Owner row;
    row.pointer=child_pointer;row.uid=child_uid;row.key=source.child.key;result.owners.emplace(row.pointer,row);
    qr::Owner other;other.pointer=0x40000;other.uid=0x400000001;other.key="unrelated";result.owners.emplace(other.pointer,other);return result;
}
struct Port {
    std::map<uint64_t,uintptr_t> registry{{child_uid,child_pointer},{0x400000001,0x40000}};
    flow::Children retained_rows{{parent_key,receipt()}};std::set<std::string> dead;
    std::string calls;int failure=0;
    bool step(int point,std::string& e){if(failure!=point)return true;e="exact controlled boundary cause "+std::to_string(point);return false;}
    bool find(uint64_t uid,uintptr_t& pointer,std::string& e){calls+='f';if(!step(1,e))return false;
        const auto found=registry.find(uid);pointer=found==registry.end()?0:found->second;return true;}
    bool retained(const std::string& key,child::Receipt& out,bool& found,bool& retired,std::string& e){calls+='l';if(!step(2,e))return false;
        const auto row=retained_rows.find(key);found=row!=retained_rows.end();retired=dead.contains(key);out=found?row->second:child::Receipt{};return true;}
    bool inverse(const std::vector<uintptr_t>& operations,child::Receipt& row,std::string& e){calls+='i';
        if(!step(3,e))return false;if(operations!=std::vector<uintptr_t>{0x90000}){e="controlled missing exact MakeNPC inverse";return false;}
        row.inverse_child_uid=row.child.row.local_uid;retained_rows.at(row.parent.key)=row;return true;}
    bool retire(const child::Receipt& row,std::string& e){calls+='r';if(!step(4,e))return false;
        if(registry.contains(row.child.row.local_uid)){e="controlled native UID still live";return false;}dead.insert(row.parent.key);return true;}
    bool restore(const child::Receipt& old,const child::Parent& parent,child::Receipt& out,std::string& e){calls+='s';if(!step(5,e))return false;
        out=old;out.parent=parent;out.child.row.local_pointer=restored_pointer;registry[child_uid]=restored_pointer;
        retained_rows[parent.key]=out;dead.erase(parent.key);return true;}
    bool capture(const child::Parent& parent,const child::Receipt* old,child::Receipt& out,std::string& e){calls+='c';if(!step(6,e))return false;
        out=old?*old:receipt();out.parent=parent;retained_rows[parent.key]=out;return true;}
};
}
int main()try{
    using namespace dungeon_building_children_fixture;
    size_t checks=0;std::string error;
    const auto need=[&](bool okay,const char* cause){++checks;if(!okay)throw std::runtime_error(std::string(cause)+" comparison="+std::to_string(checks)+" cause="+error);};
    auto source=receipt();const flow::Children before{{parent_key,source}};qr::Witness previous;
    Port port;
    need(flow::history_seed(witness(),before,port,previous,error)&&previous.owners.size()==2,"unchanged current and unrelated owners remain exact");
    port=Port{};port.registry.erase(child_uid);port.dead.insert(parent_key);
    need(flow::history_seed(witness(),before,port,previous,error)&&!previous.owners.contains(child_pointer)&&previous.owners.size()==1,"confirmed dead child excluded before native Quest dereference");
    port.dead.clear();need(!flow::history_seed(witness(),before,port,previous,error),"absence alone cannot authorize previous owner deletion");
    port=Port{};port.registry[child_uid]=restored_pointer;port.retained_rows.at(parent_key).child.row.local_pointer=restored_pointer;
    need(flow::history_seed(witness(),before,port,previous,error)&&previous.owners.contains(restored_pointer)&&!previous.owners.contains(child_pointer),"confirmed restored receiver rebinds exact canonical key/full UID");
    need(previous.owners.at(restored_pointer).state.records.empty(),"restored seed does not reread old native Quest list");
    port.retained_rows.at(parent_key).child.key="foreign";need(!flow::history_seed(witness(),before,port,previous,error),"foreign restored key rejected");
    port=Port{};port.registry[child_uid]=restored_pointer;need(!flow::history_seed(witness(),before,port,previous,error),"unconfirmed changed receiver rejected");
    port=Port{};port.registry.erase(0x400000001);need(!flow::history_seed(witness(),before,port,previous,error),"unrelated missing owner is not silently normalized");
    port=Port{};port.registry.erase(child_uid);port.dead.insert(parent_key);port.retained_rows.at(parent_key).inverse_child_uid=0;
    need(!flow::history_seed(witness(),before,port,previous,error),"retirement requires original inverse full UID");
    for(int point:{1,2}){port=Port{};port.registry.erase(child_uid);port.failure=point;error.clear();
        need(!flow::history_seed(witness(),before,port,previous,error)&&error=="exact controlled boundary cause "+std::to_string(point),"history original failure cause retained");}
    flow::Children result;std::set<uintptr_t> retired;std::vector<history::OwnerRebind> restored;
    const std::vector<uintptr_t> inverse{0x90000};
    port=Port{};port.registry.erase(child_uid);
    need(flow::publish(before,nullptr,{},inverse,port,result,retired,restored,error)&&retired==std::set<uintptr_t>{child_pointer}&&port.dead.contains(parent_key),"actual inverse validation precedes confirmed retirement");
    need(port.calls=="fir","retirement invokes only Find, inverse read and existing retirement admission");
    port=Port{};
    need(flow::publish(before,nullptr,{},inverse,port,result,retired,restored,error)&&retired.empty()&&port.calls=="f","live graph-held child not inferred deleted from current roster absence");
    for(int point:{1,3,4}){port=Port{};port.registry.erase(child_uid);port.failure=point;error.clear();
        need(!flow::publish(before,nullptr,{},inverse,port,result,retired,restored,error)&&error=="exact controlled boundary cause "+std::to_string(point),"publication retains exact boundary failure");
        if(point==3)need(!port.dead.contains(parent_key),"failed inverse never retires child");}
    port=Port{};port.registry.erase(child_uid);
    need(!flow::publish(before,nullptr,{}, {},port,result,retired,restored,error)&&!port.dead.contains(parent_key),"missing inverse never authorizes count-only retirement");
    port=Port{};port.registry[child_uid]=restored_pointer;
    need(!flow::publish(before,nullptr,{},inverse,port,result,retired,restored,error),"unexpected live receiver change rejected");
    port=Port{};port.registry.erase(child_uid);port.dead.insert(parent_key);auto parent=source.parent;parent.object+=8;
    need(flow::publish({},&before,{parent},{},port,result,retired,restored,error)&&restored.size()==1&&
        restored[0].before==child_pointer&&restored[0].after==restored_pointer&&result.at(parent_key).parent.object==parent.object,"original restore preserves UID/key and admits new parent/child receiver");
    port=Port{};port.registry.erase(child_uid);port.dead.insert(parent_key);
    need(!flow::publish({},nullptr,{parent},{},port,result,retired,restored,error)&&port.calls=="l","restoration cannot proceed without consumed graph sample");
    auto foreign=before;foreign.at(parent_key).lease.duplicate_uuid="foreign";
    port=Port{};port.dead.insert(parent_key);need(!flow::publish({},&foreign,{parent},{},port,result,retired,restored,error),"foreign lifecycle anchor rejected");
    port=Port{};port.dead.insert(parent_key);port.failure=5;error.clear();
    need(!flow::publish({},&before,{parent},{},port,result,retired,restored,error)&&error=="exact controlled boundary cause 5","partial restoration does not produce published success");
    port=Port{};port.retained_rows.clear();need(flow::publish({},nullptr,{parent},{},port,result,retired,restored,error)&&port.calls=="lc","new current parent uses existing child birth capture once");
    port=Port{};port.failure=6;error.clear();need(!flow::publish({},nullptr,{parent},{},port,result,retired,restored,error)&&error=="exact controlled boundary cause 6","original capture failure retained");
    port=Port{};need(!flow::publish({},nullptr,{parent,parent},{},port,result,retired,restored,error),"duplicate current parent rejected");
    port=Port{};need(!flow::publish({},nullptr,{}, {1,1},port,result,retired,restored,error),"duplicate graph node rejected");
    need(!flow::publish({},nullptr,{}, {0},port,result,retired,restored,error),"null graph node rejected");
    std::cout<<"Dungeon Building child lifecycle "<<checks<<" comparisons passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
