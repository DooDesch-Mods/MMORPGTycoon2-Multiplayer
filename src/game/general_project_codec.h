#pragma once
#include "general_project_state.h"
#include <climits>
namespace mt2mp::game::general_project {
namespace codec {
inline void shape(const nlohmann::json& j,std::initializer_list<const char*> names){if(!j.is_object()||j.size()!=names.size())throw std::runtime_error("General project exact object fields required");for(auto name:names)if(!j.contains(name))throw std::runtime_error("General project object field missing");}
inline int32_t i32(const nlohmann::json& v){if(!v.is_number_integer()||(v.is_number_unsigned()?v.get<uint64_t>()>uint64_t(INT32_MAX):v.get<int64_t>()<INT32_MIN||v.get<int64_t>()>INT32_MAX))throw std::runtime_error("General project int32 required");return v.get<int32_t>();}
inline uint64_t u64(const nlohmann::json& v){if(!v.is_number_integer()||(!v.is_number_unsigned()&&v.get<int64_t>()<0))throw std::runtime_error("General project uint64 required");return v.get<uint64_t>();}
inline bool boolean(const nlohmann::json& v){if(!v.is_boolean())throw std::runtime_error("General project bool required");return v.get<bool>();}
inline std::string string(const nlohmann::json& v){if(!v.is_string())throw std::runtime_error("General project string required");auto s=v.get<std::string>();if(!text(s))throw std::runtime_error("General project string bound invalid");return s;}
inline bool digest(const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
inline Source source(const nlohmann::json& j){shape(j,{"kind","tech","art","statistic"});Source v{i32(j.at("kind")),i32(j.at("statistic")),string(j.at("tech")),string(j.at("art"))};if(!valid(v))throw std::runtime_error("General project source kind/key/statistic invalid");return v;}
inline Progress progress(const nlohmann::json& j){shape(j,{"current","work","lines","bugs_total","bugs_known","workers"});Progress p;if(!j.at("current").is_null())p.current=source(j.at("current"));const auto bits=u64(j.at("work"));if(bits>UINT32_MAX)throw std::runtime_error("General project work bits exceed32");p.work=uint32_t(bits);p.lines=i32(j.at("lines"));p.bugs_total=i32(j.at("bugs_total"));p.bugs_known=i32(j.at("bugs_known"));p.workers=i32(j.at("workers"));if(!valid(p))throw std::runtime_error("General project progress values invalid");return p;}
inline Catalogue selected(const nlohmann::json& j){
    shape(j,{"source","valid","visible","owned","choose","afford","tech_flags","art_purchased","art_deployed"});Catalogue v;v.source=source(j.at("source"));v.valid=boolean(j.at("valid"));v.visible=boolean(j.at("visible"));v.owned=boolean(j.at("owned"));v.choose=boolean(j.at("choose"));v.afford=boolean(j.at("afford"));v.art_purchased=boolean(j.at("art_purchased"));v.art_deployed=boolean(j.at("art_deployed"));
    const auto& flags=j.at("tech_flags");if(!flags.is_array()||flags.size()!=3)throw std::runtime_error("General project three TechRecord flags required");for(size_t i=0;i<3;++i){const auto n=i32(flags[i]);if(n<0||n>1)throw std::runtime_error("General project TechRecord bool outside0/1");v.tech_flags[i]=uint8_t(n);}return v;
}
inline Proof proof(const nlohmann::json& j){
    shape(j,{"selected","progress","catalogue_sha256","ownership_sha256","random"});Proof p;p.selected=selected(j.at("selected"));p.progress=progress(j.at("progress"));p.catalogue_sha256=string(j.at("catalogue_sha256"));p.ownership_sha256=string(j.at("ownership_sha256"));
    if(!digest(p.catalogue_sha256)||!digest(p.ownership_sha256)||!j.at("random").is_array()||j.at("random").size()!=4)throw std::runtime_error("General project complete digest/RNG shape invalid");for(size_t i=0;i<4;++i)p.random[i]=u64(j.at("random")[i]);return p;
}
inline Intent intent(const nlohmann::json& j){shape(j,{"command","source","workers"});const auto command=i32(j.at("command"));if(command<1||command>4)throw std::runtime_error("General project command outside1..4");Intent i{Command(command),source(j.at("source")),i32(j.at("workers"))};if(!valid(i))throw std::runtime_error("General project command arguments invalid");return i;}
}
inline nlohmann::json encode(const Intent& i){return {{"command",uint8_t(i.command)},{"source",encode(i.source)},{"workers",i.workers}};}
inline bool effect(const Intent& i,const Proof& before,const Proof& after,std::string& e){
    if(!valid(i)||before.selected.source!=i.source||after.selected.source!=i.source||before.catalogue_sha256!=after.catalogue_sha256||!codec::digest(before.catalogue_sha256)||!codec::digest(before.ownership_sha256)||!codec::digest(after.ownership_sha256)||!valid(before.progress)||!valid(after.progress)){e="General project complete effect source/digest/progress differs";return false;}
    if(i.command==Command::Start){
        const bool same=before.progress.current&&*before.progress.current==i.source;
        if(!before.selected.valid||!before.selected.visible||before.selected.owned||!before.selected.choose||!before.selected.afford||!after.progress.current||*after.progress.current!=i.source||(!same&&(after.progress.work||after.progress.lines||after.progress.bugs_total||after.progress.bugs_known))||(same&&before.progress!=after.progress)){e="General project original Start effect differs";return false;}
        if((i.source.kind==0&&!after.selected.tech_flags[0])||(i.source.kind==1&&!after.selected.art_purchased)){e="General project original purchase is not present";return false;}
    }else{
        if(!before.progress.current||*before.progress.current!=i.source){e="General project exact current source missing before effect";return false;}
        if(i.command==Command::Workers){auto expected=before;expected.progress.workers=i.workers;if(expected!=after){e="General project worker effect changed unrelated native state";return false;}}
        else if(after.progress.current||after.progress.work||after.progress.lines||after.progress.bugs_total||after.progress.bugs_known||after.progress.workers){e="General project original owning retirement/reset absent";return false;}
        if(i.command==Command::Cancel){auto expected=before;expected.progress={};if(expected!=after){e="General project Cancel changed ownership/RNG";return false;}}
        if(i.command==Command::Commit&&((i.source.kind==0&&(!after.selected.owned||!after.selected.tech_flags[1]))||(i.source.kind==1&&(!after.selected.owned||!after.selected.art_deployed)))){e="General project original deployment is not present";return false;}
    }e.clear();return true;
}
}
