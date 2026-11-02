#pragma once
#include "management_service_graph.h"
#include "typed_action/management_action.h"
#include <map>

namespace mt2mp::game::management_service_graph {
// UpdateLinkPrices visits only building-backed links. The original leaf may
// rebuild their destinations, but does not rebuild the Island or spatial graph.
inline bool topology_transition(const Graph& before,const Graph& after,bool rebuild,std::string& error){
    if(before.owner!=after.owner||before.selected!=after.selected||before.spatial_float_bits!=after.spatial_float_bits||before.dimensions!=after.dimensions||before.cells!=after.cells||before.islands.size()!=after.islands.size())
        return fail(error,"Service price changed manager or spatial topology");
    for(size_t i=0;i<before.islands.size();++i){const auto& b=before.islands[i];const auto& a=after.islands[i];
        if(b.region!=a.region||b.shape!=a.shape||b.contracted!=a.contracted||b.links.size()!=a.links.size())return fail(error,"Service price changed Island geometry or link membership");
        for(size_t j=0;j<b.links.size();++j){const auto& x=b.links[j];const auto& y=a.links[j];
            if(x.building!=y.building||x.path!=y.path||x.gizmo!=y.gizmo||x.native_flag!=y.native_flag)return fail(error,"Service price changed a link source or native flag");
            if(!rebuild||x.building.empty()){if(x!=y)return fail(error,"Service price changed an unvisited link");}
            else if(y.state!=-1)return fail(error,"Service price rebuilt link lacks original reset state");
        }
    }
    if(before.source_buildings.size()!=after.source_buildings.size())return fail(error,"Service price changed source Building membership");
    for(size_t i=0;i<before.source_buildings.size();++i)if(before.source_buildings[i].key!=after.source_buildings[i].key)return fail(error,"Service price changed source Building order");
    error.clear();return true;
}
inline size_t changed_selected_destination_prices(const Graph& before,const Graph& after){
    size_t changed=0;
    for(size_t i=0;i<before.islands.size();++i)for(size_t j=0;j<before.islands[i].links.size();++j){const auto& old=before.islands[i].links[j];const auto& now=after.islands[i].links[j];
        if(old.building!=before.selected)continue;std::map<uint32_t,std::vector<std::array<int32_t,2>>> a,b;
        for(const auto& d:old.destinations)a[d.island].push_back({d.currency0,d.currency1});for(const auto& d:now.destinations)b[d.island].push_back({d.currency0,d.currency1});
        for(auto& [target,values]:a){auto found=b.find(target);if(found==b.end())continue;std::sort(values.begin(),values.end());std::sort(found->second.begin(),found->second.end());if(values!=found->second)++changed;}
    }
    return changed;
}
inline bool services_transition(const Graph& before,const Graph& after,const action_session::ManagementServices& old_group,
    const action_session::ManagementServices& new_group,const std::string& selected,size_t ordinal,const std::array<int32_t,2>& desired,bool rebuild,std::string& error){
    if(ordinal>=old_group.rows.size()||old_group.config_present!=new_group.config_present||new_group.configs.size()<old_group.configs.size())return fail(error,"Service price selected Service or config manager changed");
    const auto& chosen=old_group.rows[ordinal];std::map<std::string,std::vector<std::array<int32_t,2>>> allowed;
    if(chosen.synced&&!chosen.config_key.empty())allowed[chosen.config_key].push_back(desired);
    if(rebuild)for(const auto& source:before.source_buildings)for(const auto& row:source.services.rows)
        if(row.flags[0]&&!row.destination.empty()&&!row.config_key.empty())allowed[row.config_key].push_back(row.price);
    for(size_t i=0;i<old_group.configs.size();++i){const auto& b=old_group.configs[i];const auto& a=new_group.configs[i];
        if(a.key!=b.key)return fail(error,"Service price reordered existing configs");
        const auto expected=chosen.synced&&b.key==chosen.config_key?desired:b.price;
        if(a.price!=expected)return fail(error,"Service price changed unrelated existing config");}
    for(size_t i=old_group.configs.size();i<new_group.configs.size();++i){const auto& created=new_group.configs[i];const auto found=allowed.find(created.key);
        if(found==allowed.end()||std::find(found->second.begin(),found->second.end(),created.price)==found->second.end())return fail(error,"Service price created config without original source service");}
    if(before.source_buildings.size()!=after.source_buildings.size())return fail(error,"Service price source Service census changed size");
    for(size_t i=0;i<before.source_buildings.size();++i){const auto& b=before.source_buildings[i];const auto& a=after.source_buildings[i];
        if(b.key!=a.key||b.services.rows.size()!=a.services.rows.size()||b.services.configs.size()!=old_group.configs.size()||a.services.configs.size()!=new_group.configs.size())return fail(error,"Service price source Service or config census differs");
        for(size_t n=0;n<b.services.configs.size();++n)if(b.services.configs[n].key!=old_group.configs[n].key||b.services.configs[n].price!=old_group.configs[n].price)return fail(error,"Service price source before config census differs");
        for(size_t n=0;n<a.services.configs.size();++n)if(a.services.configs[n].key!=new_group.configs[n].key||a.services.configs[n].price!=new_group.configs[n].price)return fail(error,"Service price source after config census differs");
        auto expected=b.services;expected.configs=a.services.configs;
        for(size_t n=0;n<expected.rows.size();++n){auto& x=expected.rows[n];const auto& y=a.services.rows[n];const bool target=b.key==selected&&n==ordinal;
            if(x.price!=y.price||x.config!=y.config){
                if(x.config&&!y.config)return fail(error,"Service price removed a native config binding");
                if(target){if(y.price!=desired||(!x.flags[0]&&x.config!=y.config))return fail(error,"Service price selected source row differs");}
                else {if(!rebuild||!x.flags[0]||x.destination.empty()||x.price!=y.price)return fail(error,"Service price changed unvisited source Service");
                    const auto config=std::find_if(a.services.configs.begin(),a.services.configs.end(),[&](const auto& row){return row.key==x.config_key;});
                    if(config==a.services.configs.end()||!y.config)return fail(error,"Service price resolved source Service lacks native config");}
                x.price=y.price;x.config=y.config;
            }
        }
        if(expected!=a.services)return fail(error,"Service price changed protected source Service fields");
    }
    error.clear();return true;
}
}
