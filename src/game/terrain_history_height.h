#pragma once
#include "terrain_scenery_diagnostic.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace mt2mp::game::terrain_history_height {
using terrain_scenery_diagnostic::Row;
using terrain_scenery_diagnostic::Snapshot;
inline bool complete(const Snapshot& value){
    return value.total==value.rows.size()&&!value.auxiliary_errors&&value.first_error.empty();
}
// UpdateHeightTween skips elapsed/duration while inactive; TweenHeight writes
// both before activation. The default constructor leaves these two words unset.
inline std::array<uint32_t,6> semantic_height(const Row& row){
    auto value=row.height;if(!row.flags[0])value[3]=value[4]=0;return value;
}
inline bool same_row(const Row& a,const Row& b){
    return a.auxiliary&&b.auxiliary&&a.category==b.category&&a.variant==b.variant&&a.pose==b.pose&&
        semantic_height(a)==semantic_height(b)&&a.flags==b.flags;
}
inline nlohmann::json canonical_row(const Row& row){
    return nlohmann::json::array({row.category,row.variant,row.pose,semantic_height(row),row.flags});
}
struct PublicationScope {
    std::vector<std::array<uint32_t,6>> boxes;
    bool contains(const Row& row)const{
        const float x=std::bit_cast<float>(row.pose[0]),z=std::bit_cast<float>(row.pose[2]);
        for(const auto& box:boxes)if(x>=std::bit_cast<float>(box[0])&&x<=std::bit_cast<float>(box[3])&&
            z>=std::bit_cast<float>(box[2])&&z<=std::bit_cast<float>(box[5]))return true;
        return false;
    }
};
inline bool publication_scope(const nlohmann::json& receipt,PublicationScope& out,std::string& error){
    out={};auto fail=[&](){error="Terrain history publication boxes incomplete or invalid";return false;};
    if(!receipt.is_object()||!receipt.contains("confirmed")||receipt["confirmed"]!=true||
       !receipt.contains("publication_boxes_truncated")||receipt["publication_boxes_truncated"]!=false||
       !receipt.contains("publications")||!receipt["publications"].is_number_integer()||
       !receipt.contains("publication_boxes")||!receipt["publication_boxes"].is_array())return fail();
    const auto count=receipt["publications"].get<int64_t>();
    if(count<0||count>16||size_t(count)!=receipt["publication_boxes"].size())return fail();
    for(const auto& item:receipt["publication_boxes"]){
        if(!item.is_object()||!item.contains("initialized")||item["initialized"]!=1||
           !item.contains("read_exception")||item["read_exception"]!=0||!item.contains("coordinate_bits")||
           !item["coordinate_bits"].is_array()||item["coordinate_bits"].size()!=6)return fail();
        std::array<uint32_t,6> box{};
        for(size_t i=0;i<box.size();++i){const auto& word=item["coordinate_bits"][i];
            if(!word.is_number_integer())return fail();const auto value=word.get<int64_t>();
            if(value<0||value>UINT32_MAX)return fail();box[i]=uint32_t(value);
            if(!std::isfinite(std::bit_cast<float>(box[i])))return fail();}
        for(size_t i=0;i<3;++i)if(std::bit_cast<float>(box[i])>std::bit_cast<float>(box[i+3]))return fail();
        out.boxes.push_back(box);
    }
    return true;
}
// Native ReplaceScenery only visits publication-box XZ. Outside survivors
// retain the complete state from immediately before this history operation.
template<class Ground,class Hash>bool compare(const Snapshot& expected,const Snapshot& actual,Ground&& ground,Hash&& hash,nlohmann::json& receipt,std::string& error,
                                             const Snapshot* current=nullptr,const nlohmann::json* settlement=nullptr){
    receipt={{"confirmed",false},{"checked_rows",0},{"derived_rows",0},{"preserved_rows",0},{"derived_schema",3},{"derived",nlohmann::json::array()}};
    auto reject=[&](uint64_t uid,const std::string& cause){error="Terrain history Scenery UID="+std::to_string(uid)+": "+cause;return false;};
    if(!complete(expected)||!complete(actual)||expected.total!=actual.total)return reject(0,"complete capture/count differs");
    PublicationScope scope;
    if(bool(current)!=bool(settlement)||(current&&!complete(*current)))return reject(0,"pre-operation capture/publication scope incomplete");
    if(settlement&&!publication_scope(*settlement,scope,error))return false;
    size_t checked=0,derived=0,preserved=0;std::vector<std::string> proof_rows;
    for(const auto& [uid,before]:expected.rows){
        const auto found=actual.rows.find(uid);if(found==actual.rows.end())return reject(uid,"missing from actual census");
        const auto& after=found->second;
        if(!before.auxiliary||!after.auxiliary||before.category!=after.category||before.variant!=after.variant)
            return reject(uid,"type/variant/auxiliary read differs");
        for(size_t i=0;i<before.pose.size();++i)if(i!=1&&before.pose[i]!=after.pose[i])return reject(uid,"static pose differs field="+std::to_string(i));
        const auto before_height=semantic_height(before),after_height=semantic_height(after);
        for(size_t i=3;i<before_height.size();++i)if(before_height[i]!=after_height[i])return reject(uid,"relative height/tween timing differs field="+std::to_string(i));
        if(before.flags!=after.flags)return reject(uid,"tween/automatic flags differ");
        const bool published=current&&scope.contains(after);
        const Row* unchanged=nullptr;
        if(current&&!published){const auto previous=current->rows.find(uid);if(previous!=current->rows.end())unchanged=&previous->second;}
        if(unchanged){
            if(!same_row(*unchanged,after))return reject(uid,"outside publication boxes changed from complete pre-operation state");
            proof_rows.push_back(nlohmann::json::array({"preserved",canonical_row(before),canonical_row(*unchanged),canonical_row(after)}).dump());
            ++preserved;
        }else if(published||before.pose[1]!=after.pose[1]){
            if(current&&!published)return reject(uid,"restored Scenery outside publication boxes differs from original height");
            if(after.flags[0]||after.height[0]!=after.pose[1]||after.height[1]!=after.pose[1]||after.height[2]!=after.pose[1])
                return reject(uid,"changed height is not a completed original placement");
            float native_ground=0;std::string cause;
            if(!ground(uid,after,native_ground,cause))return reject(uid,cause);
            const float offset=std::bit_cast<float>(after.height[5]);
            const float target=native_ground+offset;
            const auto bits=std::bit_cast<uint32_t>(target);
            if(!std::isfinite(native_ground)||!std::isfinite(offset)||!std::isfinite(target)||bits!=after.pose[1])
                return reject(uid,"original ground plus offset differs: required_bits="+std::to_string(bits)+" actual_bits="+std::to_string(after.pose[1]));
            ++derived;
            proof_rows.push_back(nlohmann::json::array({"derived",canonical_row(before),canonical_row(after),std::bit_cast<uint32_t>(native_ground)}).dump());
            if(receipt["derived"].size()<terrain_scenery_diagnostic::report_limit)
                receipt["derived"].push_back({{"uid",uid},{"old_y_bits",before.pose[1]},{"actual_y_bits",after.pose[1]},
                    {"native_ground_bits",std::bit_cast<uint32_t>(native_ground)},{"offset_bits",after.height[5]}});
        }else for(size_t i=0;i<3;++i)if(before.height[i]!=after.height[i])return reject(uid,"unchanged Y has different tween state");
        receipt["checked_rows"]=++checked;receipt["derived_rows"]=derived;receipt["preserved_rows"]=preserved;
    }
    // Complete canonical rows preserve multiplicity and publication scope.
    // Local UIDs appear only in bounded display rows, not cross-peer identity.
    std::sort(proof_rows.begin(),proof_rows.end());std::string digest;
    if(!hash(nlohmann::json::array({scope.boxes,proof_rows}).dump(),digest,error))return reject(0,"derived multiset SHA256 failed: "+error);
    receipt["derived_sha256"]=digest;
    receipt["derived_truncated"]=derived>receipt["derived"].size();receipt["confirmed"]=true;return true;
}
}
