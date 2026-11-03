#pragma once
#include "gm_policy_cascade.h"
#include <map>
namespace mt2mp::game::gm_policy {
struct SpatialModel {uintptr_t object=0,transform=0;int32_t node=-1;uint8_t dynamic=0;bool operator==(const SpatialModel&)const=default;};
struct SpatialNode {int32_t parent=-1,transforms=0,total=0;PolicyArray models,render;bool operator==(const SpatialNode&)const=default;};
struct Spatial {
    uintptr_t tree=0,node_data=0,bucket_data=0;int32_t node_count=0;uint32_t buckets=0,shift=0;
    PolicyArray models,pending;std::map<int32_t,SpatialNode> nodes;std::map<uint32_t,std::vector<uintptr_t>> chains;
    bool operator==(const Spatial&)const=default;
};
template<class Read>bool spatial_read(Read& read,uintptr_t tree,const std::vector<SpatialModel>& witnesses,Spatial& out,std::string& error){
    out={};out.tree=tree;
    if(!tree||!read(tree,0x10,out.node_data,error)||!read(tree,0x18,out.node_count,error)||!read(tree,0x28,out.bucket_data,error)||!read(tree,0x30,out.buckets,error)||!read(tree,0x34,out.shift,error))return false;
    if(out.node_count<0||out.node_count>1048576||(!out.node_data&&out.node_count)||(out.node_count&&out.node_data>UINTPTR_MAX-size_t(out.node_count)*120)||!out.buckets||out.buckets>1048576||!out.bucket_data||out.shift>31){error="GM policy spatial owner/node/hash extent invalid";return false;}
    if(!policy_array(read,tree,0x40,1048576,out.models,error)||!policy_array(read,tree,0x58,1048576,out.pending,error))return false;
    for(const auto& model:witnesses){if(!model.object)continue;
        const auto bucket=(uint32_t(model.object)*uint32_t(0x9e3779ff))>>out.shift;
        if(bucket>=out.buckets){error="GM policy spatial model hash exceeds current buckets";return false;}
        if(!out.chains.contains(bucket)){
            uintptr_t link=0;if(!read(out.bucket_data,size_t(bucket)*24+0x10,link,error))return false;
            std::set<uintptr_t> visited,objects;auto& chain=out.chains[bucket];
            while(link){if(!visited.insert(link).second||visited.size()>1048576){error="GM policy spatial hash chain cycle/extent invalid";return false;}
                uintptr_t object=0,next=0;uint32_t key=0;if(!read(link,0,object,error)||!read(link,8,key,error)||!read(link,0x10,next,error))return false;
                if(!object||key!=uint32_t(object)||!objects.insert(object).second){error="GM policy spatial hash key/object differs";return false;}chain.push_back(object);link=next;}
        }
        std::set<int32_t> seen;auto index=model.node;
        while(index!=-1){if(index<0||index>=out.node_count||!seen.insert(index).second||seen.size()>64){error="GM policy spatial ancestor index/cycle invalid";return false;}
            if(!out.nodes.contains(index)){SpatialNode node;const auto object=out.node_data+size_t(index)*120;
                if(!read(object,0x20,node.parent,error)||!read(object,0x58,node.transforms,error)||!read(object,0x5c,node.total,error)||!policy_array(read,object,0x30,1048576,node.models,error)||!policy_array(read,object,0x48,1048576,node.render,error))return false;
                if(node.transforms<0||node.total<0){error="GM policy spatial ancestor counts invalid";return false;}out.nodes.emplace(index,std::move(node));}
            index=out.nodes.at(index).parent;
        }
    }
    return true;
}
template<class Read>bool spatial_models(Read& read,const std::vector<uintptr_t>& masters,std::vector<SpatialModel>& out,std::string& error){
    out.assign(masters.size(),{});
    for(size_t slot=0;slot<masters.size();++slot){auto& model=out[slot];if(!read(masters[slot],0x488,model.object,error))return false;if(!model.object)continue;
        uintptr_t entity=0;uint8_t owned=0;
        if(!read(model.object,8,model.transform,error)||!read(model.object,0x118,entity,error)||!read(model.object,0x124,model.node,error)||!read(model.object,0x128,owned,error)||!read(model.object,0x12b,model.dynamic,error))return false;
        if(entity!=masters[slot]||model.transform!=masters[slot]+0x30||owned||model.dynamic>1){error="GM policy spatial model current master/embedded transform differs";return false;}
    }
    return true;
}
inline bool spatial_remove(std::vector<uintptr_t>& members,uintptr_t object,bool unordered){
    const auto found=std::find(members.begin(),members.end(),object);if(found==members.end())return false;
    if(unordered){*found=members.back();members.pop_back();}else members.erase(found);return true;
}
inline bool spatial_expected(const Spatial& before,const std::vector<SpatialModel>& models,const std::vector<size_t>& removed,Spatial& after,std::string& error){
    after=before;
    for(auto slot:removed){if(slot>=models.size()){error="GM policy spatial removed slot exceeds fixed pool";return false;}const auto& model=models[slot];if(!model.object)continue;
        const auto bucket=(uint32_t(model.object)*uint32_t(0x9e3779ff))>>after.shift;
        auto chain=after.chains.find(bucket);
        if(chain==after.chains.end()||!spatial_remove(chain->second,model.object,false)||!spatial_remove(after.models.members,model.object,true)){error="GM policy original model hash/global membership absent";return false;}
        --after.models.count;
        if(spatial_remove(after.pending.members,model.object,false))--after.pending.count;
        auto index=model.node;
        if(index!=-1){auto found=after.nodes.find(index);if(found==after.nodes.end()){error="GM policy spatial leaf witness absent";return false;}
            auto& leaf=found->second;
            if(!spatial_remove(leaf.models.members,model.object,true)){error="GM policy spatial leaf model membership absent";return false;}--leaf.models.count;
            if(model.dynamic){if(!spatial_remove(leaf.render.members,model.transform,true)){error="GM policy spatial leaf render membership absent";return false;}--leaf.render.count;}
            while(index!=-1){auto& node=after.nodes.at(index);--node.total;if(model.dynamic)--node.transforms;
                if(node.total<0||node.transforms<0){error="GM policy spatial original ancestor decrement underflows";return false;}index=node.parent;}
        }
    }
    return true;
}
}
