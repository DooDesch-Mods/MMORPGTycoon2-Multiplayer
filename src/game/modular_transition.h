#pragma once
#include "typed_action/modular_action.h"
#include "scenery_stroke_transition.h"
#include "scenery_stroke_state.h"
#include <algorithm>
#include <bit>
#include <cmath>
namespace mt2mp::game::modular_transition {
namespace a=action_session;
inline bool fail(std::string& e,const char* why){e=why;return false;}
inline nlohmann::json records_json(const std::vector<subscriber_lifecycle::Record>& records){auto out=nlohmann::json::array();const auto token=[](const auto& value){return nlohmann::json::array({value.type,value.bits,value.text});};
    for(const auto& row:records){auto tokens=nlohmann::json::array();for(const auto& t:row.tokens)tokens.push_back(token(t));out.push_back(nlohmann::json::array({row.parent==SIZE_MAX?int64_t{-1}:int64_t(row.parent),row.child_index,token(row.label),tokens}));}return out;}
inline std::string record_readback_difference(const nlohmann::json& expected,const nlohmann::json& actual,bool plan){
    if(expected==actual)return {};
    const auto describe=[](const nlohmann::json& value){std::string out=value.is_number_unsigned()?"unsigned":value.is_number_integer()?"integer":value.type_name();
        if(value.is_array()||value.is_object())return out+"(count="+std::to_string(value.size())+")";
        if(value.is_string()){const auto& text=value.get_ref<const std::string&>();return out+"(bytes="+std::to_string(text.size())+",prefix="+nlohmann::json(text.substr(0,16)).dump(-1,' ',false,nlohmann::json::error_handler_t::replace)+")";}
        return out+"("+value.dump()+")";
    };
    const auto owner=[&](const nlohmann::json& rows,size_t index){
        for(size_t depth=0;depth<8&&rows.is_array()&&index<rows.size();++depth){const auto& row=rows[index];
            if(!row.is_array()||row.size()!=4)return std::string{};
            if(row[2].is_array()&&row[2].size()==3&&row[2][2].is_string()&&!row[2][2].get_ref<const std::string&>().empty())return describe(row[2][2]);
            if(!row[0].is_number_integer())return std::string{};const auto parent=row[0].get<int64_t>();if(parent<0||uint64_t(parent)>=index)return std::string{};index=size_t(parent);
        }return std::string{};
    };
    std::string out=std::string("Modular original detached ")+(plan?"Plan":"Layout")+" Record readback differs expected_records="+std::to_string(expected.size())+" actual_records="+std::to_string(actual.size());
    size_t differences=0;
    const auto report=[&](const std::string& path,const nlohmann::json& want,const nlohmann::json& got,size_t row){
        auto detail="; path="+(path.empty()?std::string("/"):path)+" expected="+describe(want)+" actual="+describe(got);
        if(row!=SIZE_MAX)detail+=" expected_owner="+owner(expected,row)+" actual_owner="+owner(actual,row);
        // Ordered-action causes have a 1024-byte wire limit; leave room for the
        // existing Host error prefix and retain only complete diagnostic fields.
        if(out.size()+detail.size()>900){differences=4;return;}out+=detail;++differences;
    };
    const auto visit=[&](auto&& self,const nlohmann::json& want,const nlohmann::json& got,const std::string& path,size_t row,size_t depth)->void{
        if(differences==4||want==got)return;
        if(!want.is_array()||!got.is_array()||depth==8){report(path,want,got,row);return;}
        if(want.size()!=got.size())report(path,want,got,row);
        const auto count=std::min(want.size(),got.size());
        for(size_t i=0;i<count&&differences<4;++i)self(self,want[i],got[i],path+"/"+std::to_string(i),depth?row:i,depth+1);
    };
    visit(visit,expected,actual,"",SIZE_MAX,0);return out;
}
inline bool records(const nlohmann::json& j,bool plan,std::vector<subscriber_lifecycle::Record>& out,std::string& e)try{
    out.clear();if(!j.is_array()||j.empty()||j.size()>100000)return fail(e,"Modular typed owned Record count differs");size_t text=0,tokens=0;std::vector<size_t> children(j.size());std::set<uint32_t> definitions;
    const auto token=[&](const auto& value,subscriber_lifecycle::Token& result){if(!value.is_array()||value.size()!=3||!value[0].is_number_integer()||!value[1].is_number_unsigned()||!value[2].is_string())return false;
        const auto type=value[0].template get<int64_t>();const auto bits=value[1].template get<uint64_t>();if(type<0||type>4||bits>UINT32_MAX)return false;result={int32_t(type),uint32_t(bits),value[2].template get<std::string>()};text+=result.text.size();
        return text<=32*1024*1024&&result.text.find('\0')==std::string::npos&&((type==1||type==2)?bits==0:result.text.empty())&&(type!=3||std::isfinite(std::bit_cast<float>(result.bits)));};
    for(size_t i=0;i<j.size();++i){const auto& row=j[i];if(!row.is_array()||row.size()!=4||!row[0].is_number_integer()||!row[1].is_number_unsigned()||!row[3].is_array())return fail(e,"Modular exact native Record row required");
        const auto parent=row[0].get<int64_t>();const auto index=row[1].get<uint64_t>();if((!i&&(parent!=-1||index))||(i&&(parent<0||uint64_t(parent)>=i||index!=children[size_t(parent)]++)))return fail(e,"Modular complete native Record tree order differs");
        subscriber_lifecycle::Record result;result.parent=i?size_t(parent):SIZE_MAX;result.child_index=size_t(index);if(!token(row[2],result.label)||result.label.type>2)return fail(e,"Modular Record label type/bits differ");tokens+=row[3].size();if(tokens>1000000)return fail(e,"Modular Record token budget exceeded");
        for(const auto& t:row[3]){subscriber_lifecycle::Token value;if(!token(t,value))return fail(e,"Modular native Record token differs");result.tokens.push_back(std::move(value));}if(result.label.text=="linkId"&&(result.tokens.size()!=1||result.tokens[0].type!=4||!result.tokens[0].bits||result.tokens[0].bits>INT32_MAX||!definitions.insert(result.tokens[0].bits).second))return fail(e,"Modular inline Record definition is invalid or repeated");out.push_back(std::move(result));}
    if(out[0].label.type!=1||out[0].label.text!=(plan?"mmoBuildingPlan":"mmoBuildingLayout"))return fail(e,"Modular native Record typed root differs");e.clear();return true;
}catch(const std::exception& error){e=std::string("Modular owned Record decode: ")+error.what();return false;}
inline bool resolved_layout_record(const std::vector<subscriber_lifecycle::Record>& rows,nlohmann::json& out,std::string& e){
    out={};if(rows.empty()||rows[0].label.type!=1||rows[0].label.text!="mmoBuildingLayout")return fail(e,"Modular bounds projection requires Layout root");
    const std::array<const char*,7> names{"grid","dimX","dimY","dimZ","bounds","origin","gridSize"};std::array<size_t,7> at;at.fill(SIZE_MAX);
    for(size_t i=1;i<rows.size();++i)if(rows[i].parent==0&&rows[i].label.type==1)for(size_t n=0;n<names.size();++n)if(rows[i].label.text==names[n]){
        if(at[n]!=SIZE_MAX)return fail(e,"Modular bounds projection has repeated root property");at[n]=i;
    }
    for(auto index:at)if(index==SIZE_MAX)return fail(e,"Modular bounds projection lacks root property");
    const auto typed=[&](size_t n,size_t count,int type){const auto& tokens=rows[at[n]].tokens;if(tokens.size()!=count)return false;for(const auto& token:tokens)if(token.type!=type||!token.text.empty()||(type==3&&!std::isfinite(std::bit_cast<float>(token.bits))))return false;return true;};
    if(!typed(1,1,4)||!typed(2,1,4)||!typed(3,1,4)||!typed(4,6,3)||!typed(5,3,3)||!typed(6,1,3))return fail(e,"Modular bounds projection property token shape differs");
    std::array<size_t,3> dims{};size_t count=1;for(size_t i=0;i<3;++i){const auto value=std::bit_cast<int32_t>(rows[at[i+1]].tokens[0].bits);if(value<0||value>1000000)return fail(e,"Modular bounds projection dimension differs");dims[i]=size_t(value);count*=dims[i];if(count>1000000)return fail(e,"Modular bounds projection grid budget exceeded");}
    std::vector<uint32_t> grid;for(const auto& token:rows[at[0]].tokens){if(token.type!=4)return fail(e,"Modular bounds projection grid token differs");grid.push_back(token.bits);}
    size_t empty_children=0;for(size_t i=1;i<rows.size();++i){const auto& row=rows[i];
        for(size_t n=1;n<at.size();++n)if(row.parent==at[n])return fail(e,"Modular bounds projection scalar property has children");
        if(row.parent==at[0]){if(!rows[at[0]].tokens.empty()||row.label.type||row.label.bits||!row.label.text.empty()||row.tokens.size()>1||(!row.tokens.empty()&&row.tokens[0].type!=4))return fail(e,"Modular bounds projection grid child differs");if(!row.tokens.empty())grid.push_back(row.tokens[0].bits);else if(count||++empty_children>1)return fail(e,"Modular bounds projection grid has unexpected empty child");}
        else if(row.parent<rows.size()&&rows[row.parent].parent==at[0])return fail(e,"Modular bounds projection grid child has descendants");
    }
    if(grid.size()!=count)return fail(e,"Modular bounds projection grid length differs from dimensions");
    const float spacing=std::bit_cast<float>(rows[at[6]].tokens[0].bits);std::array<float,3> origin{};for(size_t i=0;i<3;++i)origin[i]=std::bit_cast<float>(rows[at[5]].tokens[i].bits);
    // Resolve resets origin.y, then BuildBoundingBox unions both corners of
    // every occupied cell. Bounds is serialized, but is rebuilt from the grid.
    origin[1]=0.f;std::array<float,6> bounds{};bool occupied=false;
    const auto expand=[&](const std::array<float,3>& point){for(size_t i=0;i<3;++i){if(!occupied){bounds[i]=bounds[i+3]=point[i];}else{bounds[i]=bounds[i]<point[i]?bounds[i]:point[i];bounds[i+3]=bounds[i+3]>point[i]?bounds[i+3]:point[i];}}occupied=true;};
    for(size_t x=0;count&&x<dims[0];++x)for(size_t y=0;y<dims[1];++y)for(size_t z=0;z<dims[2];++z)if(grid[x+dims[0]*y+dims[0]*dims[1]*z]){
        for(size_t corner=0;corner<2;++corner){std::array<float,3> point{float(x)+(corner?0.5f:-0.5f),float(y+(corner?2:0)),float(z)+(corner?0.5f:-0.5f)};
            // The original uses separate SSE multiply and add instructions.
            for(size_t i=0;i<3;++i){volatile float scaled=point[i]*spacing;point[i]=origin[i]+scaled;if(!std::isfinite(point[i]))return fail(e,"Modular bounds projection is not finite");}expand(point);
        }
    }
    auto expected=rows;for(size_t i=0;i<6;++i)expected[at[4]].tokens[i].bits=std::bit_cast<uint32_t>(bounds[i]);out=records_json(expected);e.clear();return true;
}
template<class Key>bool valid(const a::ModularIntent& v,Key&& key,std::string& e){
    if(v.mode>4||v.region<0)return fail(e,"Modular mode/Region outside original domain");for(auto bits:v.position)if(!std::isfinite(std::bit_cast<float>(bits)))return fail(e,"Modular resolved position is not finite");
    if(v.mode){if(!key(v.target)||!duplicate_transition::sha(v.before_record_sha)||!v.theme.empty()||v.pose!=std::array<uint32_t,10>{}||v.clearance!=std::array<uint32_t,6>{}||v.layout!=nlohmann::json::array()||v.plan!=nlohmann::json::array())return fail(e,"Modular edit exact canonical before/unused Prefab fields differ");}
    else{if(!v.target.empty()||!v.before_record_sha.empty()||v.theme.empty()||v.theme.size()>512||v.theme.find('\0')!=std::string::npos||!duplicate_transition::pose(v.pose))return fail(e,"Modular Prefab theme/pose or unused target fields differ");
        for(size_t i=0;i<3;++i)if(v.position[i]!=v.pose[4+i])return fail(e,"Modular Prefab purchase and preview position differ");
        for(auto bits:v.clearance)if(!std::isfinite(std::bit_cast<float>(bits)))return fail(e,"Modular Prefab clearance is not finite");for(size_t i=0;i<3;++i)if(std::bit_cast<float>(v.clearance[i])>std::bit_cast<float>(v.clearance[i+3]))return fail(e,"Modular Prefab clearance minimum exceeds maximum");
        std::vector<subscriber_lifecycle::Record> rows;if(!records(v.layout,false,rows,e)||!records(v.plan,true,rows,e))return false;}
    e.clear();return true;
}
inline bool target_matches(const a::ModularIntent& required,const a::DuplicateObject* actual,bool has_handle,std::string& e){
    const std::string prefix="Modular exact original target changed before Host action: ";
    const auto differs=[&](const std::string& detail){e=prefix+detail;return false;};
    if(!actual)return differs("row required=present actual=absent");
    if(!has_handle)return differs("handle required=present actual=absent");
    if(actual->family!=a::DuplicateFamily::Building)return differs("family required="+std::to_string(uint32_t(a::DuplicateFamily::Building))+" actual="+std::to_string(uint32_t(actual->family)));
    if(actual->region!=required.region)return differs("region required="+std::to_string(required.region)+" actual="+std::to_string(actual->region));
    if(actual->record_sha!=required.before_record_sha)return differs("record_sha required="+required.before_record_sha+" actual="+actual->record_sha);
    return true;
}
inline nlohmann::json authored_status(const a::ModularState& before,const a::ModularState& after,const a::ModularEffect& effect){
    auto removed=nlohmann::json::object(),created=nlohmann::json::object(),changed=nlohmann::json::object();
    for(const auto& row:effect.removed)removed[row.key]=row.record_sha;
    for(const auto& row:effect.created)created[row.key]=row.record_sha;
    for(const auto& row:effect.changed)changed[row[0].key]=nlohmann::json::array({row[0].record_sha,row[1].record_sha});
    return {{"before_sha",before.props_sha},{"after_sha",after.props_sha},{"untouched_sha",effect.untouched_sha},
        {"removed",std::move(removed)},{"created",std::move(created)},{"changed",std::move(changed)}};
}

template<class Hash>bool difference(const std::map<std::string,a::DuplicateObject>& before,const std::map<std::string,a::DuplicateObject>& after,Hash&& hash,a::ModularEffect& out,std::string& e){
    out={};auto untouched=nlohmann::json::array();for(const auto& [key,row]:before){const auto found=after.find(key);if(found==after.end())out.removed.push_back(row);else if(found->second!=row)out.changed.push_back({row,found->second});else untouched.push_back(scenery_stroke_state::row(row));}
    for(const auto& [key,row]:after)if(!before.contains(key))out.created.push_back(row);return hash(untouched.dump(),out.untouched_sha,e);
}
template<class Key>bool valid(const a::ModularEffect& v,Key&& key,std::string& e){
    if(!duplicate_transition::sha(v.untouched_sha)||v.removed.size()>1000000||v.created.size()>1000000||v.changed.size()>1000000)return fail(e,"Modular complete effect budget/hash differs");std::set<std::string> prior,next;
    for(const auto& row:v.removed)if(!scenery_stroke_transition::object(row,key)||!prior.insert(row.key).second)return fail(e,"Modular removed canonical owner repeats or is invalid");
    for(const auto& row:v.created)if(!scenery_stroke_transition::object(row,key)||!next.insert(row.key).second||prior.contains(row.key))return fail(e,"Modular created canonical owner repeats or is invalid");
    for(const auto& row:v.changed)if(!scenery_stroke_transition::object(row[0],key)||!scenery_stroke_transition::object(row[1],key)||row[0]==row[1]||row[0].key!=row[1].key||row[0].family!=row[1].family||row[0].region!=row[1].region||!prior.insert(row[0].key).second||!next.insert(row[1].key).second)return fail(e,"Modular changed exact owner pair differs");
    for(const auto& sha:v.dependency_sha)if(!duplicate_transition::sha(sha))return fail(e,"Modular complete Service/Quest witness hash differs");std::set<std::string> related;for(const auto& owner:v.related_buildings)if(!key(owner)||!related.insert(owner).second)return fail(e,"Modular reciprocal Building owner is invalid or repeated");
    for(const auto& cash:v.cash){if(cash.amount==INT32_MIN)return fail(e,"Modular native inverse cash cannot be negated");for(auto bits:cash.position)if(!std::isfinite(std::bit_cast<float>(bits)))return fail(e,"Modular native cash position is not finite");}e.clear();return true;
}
}
