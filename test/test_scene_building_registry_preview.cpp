#include "../src/game/scene_building_registry_preview.h"
#include <map>
#include <string>
#include <iostream>

struct Entry {
    static inline unsigned copies=0;
    static inline unsigned comparisons=0;
    int value=0;
    explicit Entry(int v=0):value(v){}
    Entry(const Entry& other):value(other.value){++copies;}
    Entry& operator=(const Entry& other){value=other.value;++copies;return *this;}
    bool operator==(const Entry& other)const{++comparisons;return value==other.value;}
};
using Registry=std::map<std::string,Entry>;
using Preview=mt2mp::game::scene_building_registry::Preview<Registry>;
int main(){
    int checks=0;
    const auto need=[&](bool okay,const char* cause){++checks;if(!okay)std::cerr<<cause<<'\n';return okay;};
    Registry registry;for(int i=0;i<1528;++i)registry.emplace(std::to_string(i),Entry(i));
    const auto original=registry;
    Preview unchanged;Entry::copies=0;unchanged.prepare(registry,false);
    if(!need(Entry::copies==0&&unchanged.before.empty()&&!unchanged.after,"empty changes copy no registry rows"))return 1;
    Entry::comparisons=0;
    if(!need(unchanged.current(registry)&&Entry::comparisons==0,"empty changes compare no unrelated rows"))return 1;
    const auto* retained=&registry.begin()->second;Entry::copies=0;
    if(unchanged.current(registry))unchanged.publish_after_check(registry);
    if(!need(Entry::copies==0&&registry==original&&&registry.begin()->second==retained,"unchanged publication preserves entries and allocation"))return 1;
    registry.begin()->second.value=-1;
    if(!need(unchanged.current(registry),"empty changes permit unrelated metadata edits"))return 1;
    unchanged.publish_after_check(registry);
    if(!need(registry.begin()->second.value==-1&&&registry.begin()->second==retained,"empty publication preserves concurrent metadata and allocations"))return 1;
    registry=original;registry.erase(registry.begin());
    if(!need(unchanged.current(registry),"empty changes do not depend on unrelated membership"))return 1;
    unchanged.publish_after_check(registry);
    if(!need(registry.size()==1527,"empty publication does not restore unrelated removed entries"))return 1;
    registry=original;Preview changed;Entry::copies=0;changed.prepare(registry,true);
    if(!need(Entry::copies==3056&&changed.after.has_value(),"changed preview retains separate before and after registries"))return 1;
    changed.after->at("1").value=77;
    if(!need(changed.current(registry)&&registry==original,"staging does not mutate actual registry"))return 1;
    registry.at("2").value=-2;
    if(!need(!changed.current(registry),"nonempty replacement still rejects concurrent metadata"))return 1;
    registry=original;registry.erase("2");
    if(!need(!changed.current(registry),"nonempty replacement still rejects concurrent membership"))return 1;
    registry=original;
    if(changed.current(registry))changed.publish_after_check(registry);
    if(!need(registry.at("1").value==77&&registry.size()==original.size(),"changed publication retains requested output"))return 1;
    changed.prepare(registry,false);
    if(!need(changed.before.empty()&&!changed.after&&changed.current(registry),"reused no-op preview discards old baseline and replacement"))return 1;
    std::cout<<checks<<" Scene registry preview checks passed\n";
}
