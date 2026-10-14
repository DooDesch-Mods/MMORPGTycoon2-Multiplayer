#pragma once
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace mt2mp::game::live_host_state::day::economy {
// Only one callback-free sampling pass owns this index. Native mutations use
// the independent full resolver and cannot observe this local receipt.
class SampleRegionIndex {
    struct Row {uintptr_t region=0;int32_t id=-1;bool operator==(const Row&)const=default;};
    struct Snapshot {
        uintptr_t image=0,map=0,root=0,data=0,owned=0;
        int32_t capacity=-1,owned_capacity=-1;
        std::vector<Row> saved;
        std::vector<uintptr_t> owned_rows;
        bool operator==(const Snapshot&)const=default;
    } snapshot_;
    std::map<int32_t,size_t> slots_;
    bool active_=false;
    static bool reject(std::string& e,const std::string& cause){e=cause;return false;}
    template<class Read>
    static bool capture(uintptr_t image,uintptr_t map,uintptr_t root,Snapshot& out,Read&& read,std::string& e){
        if(!map||map>std::numeric_limits<uintptr_t>::max()-0x364||root!=map+0x1c0)return reject(e,"sample region Map or root shard differs");
        Snapshot next;next.image=image;next.map=map;next.root=root;
        uintptr_t map_ap=0,root_ap=0,saved_ap=0;int32_t root_type=-1,count=-1,owned_count=-1;
        if(!read(map,0,map_ap,e)||!read(root,0,root_ap,e)||!read(root,0x48,root_type,e)||
           !read(map+0x230,0,saved_ap,e)||!read(map+0x230,8,next.data,e)||!read(map+0x230,0x10,count,e)||!read(map+0x230,0x14,next.capacity,e)||
           !read(map,0x358,next.owned,e)||!read(map,0x360,owned_count,e)||!read(map,0x364,next.owned_capacity,e))return false;
        if(map_ap!=image+0x1742860||root_ap!=image+0x174ad20||root_type!=0||saved_ap!=image+0x16f04e0)return reject(e,"sample region Map, root or array address point differs");
        if(count<1||count>4096||next.capacity<count||!next.data||owned_count<0||owned_count>count||next.owned_capacity<owned_count||(owned_count&&!next.owned))return reject(e,"sample region or owned subset array bounds differ");
        std::set<int32_t> ids;std::set<uintptr_t> saved;
        next.saved.reserve(size_t(count));next.owned_rows.reserve(size_t(owned_count));
        for(int32_t i=0;i<count;++i){
            Row row;uintptr_t ap=0,shard=0;
            if(!read(next.data,size_t(i)*8,row.region,e)||!read(row.region,0,ap,e)||!read(row.region,0x70,shard,e)||!read(row.region,0x834,row.id,e))return false;
            if(!row.region||ap!=image+0x1787930||shard!=root||row.id<0||!saved.insert(row.region).second||!ids.insert(row.id).second)return reject(e,"sample saved region address point, shard, ID or pointer differs index="+std::to_string(i));
            next.saved.push_back(row);
        }
        std::set<uintptr_t> owned_unique;
        for(int32_t i=0;i<owned_count;++i){
            uintptr_t region=0,ap=0,shard=0;
            if(!read(next.owned,size_t(i)*8,region,e)||!read(region,0,ap,e)||!read(region,0x70,shard,e))return false;
            if(!region||ap!=image+0x1787930||shard!=root||!owned_unique.insert(region).second||!saved.contains(region))return reject(e,"sample owned region is duplicate or absent from saved Map.region index="+std::to_string(i));
            next.owned_rows.push_back(region);
        }
        uintptr_t data=0,owned=0;int32_t count_after=-1,owned_after=-1;
        if(!read(map+0x230,8,data,e)||!read(map+0x230,0x10,count_after,e)||!read(map,0x358,owned,e)||!read(map,0x360,owned_after,e))return false;
        if(data!=next.data||count_after!=count||owned!=next.owned||owned_after!=owned_count)return reject(e,"sample region arrays changed during identity scan");
        out=std::move(next);return true;
    }
public:
    bool active()const noexcept{return active_;}
    template<class Read>
    bool resolve(uintptr_t image,uintptr_t map,uintptr_t root,int32_t requested,uintptr_t expected,uintptr_t& selected,Read&& read,std::string& e){
        selected=0;if(requested<0)return reject(e,"sample region saved ID is negative");
        if(!active_){
            if(!capture(image,map,root,snapshot_,read,e))return false;
            for(size_t i=0;i<snapshot_.saved.size();++i)slots_.emplace(snapshot_.saved[i].id,i);
            active_=true;
        }
        if(image!=snapshot_.image||map!=snapshot_.map||root!=snapshot_.root)return reject(e,"sample region World/Map/root binding changed within pass");
        const auto slot=slots_.find(requested);if(slot==slots_.end())return reject(e,"sample region ID is absent from saved Map");
        const auto& row=snapshot_.saved[slot->second];uintptr_t receiver=0,ap=0,shard=0;int32_t id=-1;
        if(!read(snapshot_.data,slot->second*8,receiver,e)||!read(row.region,0,ap,e)||!read(row.region,0x70,shard,e)||!read(row.region,0x834,id,e))return false;
        if(receiver!=row.region||ap!=image+0x1787930||shard!=root||id!=requested||(expected&&row.region!=expected))return reject(e,"sample target region slot/address point/shard/ID/receiver changed");
        selected=row.region;return true;
    }
    template<class Read>
    bool finish(uintptr_t image,uintptr_t map,uintptr_t root,Read&& read,std::string& e)const{
        if(!active_)return true;
        Snapshot current;
        if(!capture(image,map,root,current,read,e))return false;
        return current==snapshot_||reject(e,"sample complete saved/owned region index changed during pass");
    }
};
}
