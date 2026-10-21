#pragma once
#include "duplicate_record_witness.h"
#include "mt2mp/core/hash.h"
#include "vendor/nlohmann/json.hpp"
#include <algorithm>

namespace mt2mp::game::duplicate_records {
inline constexpr size_t diagnostic_sources=4,diagnostic_bytes=128*1024;
inline std::string diagnostic_sha(const std::string& text){return mt2mp::sha256(std::vector<unsigned char>(text.begin(),text.end()));}
// Numeric record paths preserve tree order. String contents and canonical
// external owners are hashed; the private allocation pointers never enter here.
inline nlohmann::json diagnose(const nlohmann::json& records,const nlohmann::json& links,const Witness& witness)try{
    using json=nlohmann::json;
    json out={{"schema_version",1},{"record_sha",witness.sha},{"records_sha",diagnostic_sha(records.dump())},
        {"manifest_sha",diagnostic_sha(links.dump())},{"record_count",witness.records},{"token_count",witness.tokens},
        {"link_count",witness.links},{"records",json::array()},{"links",json::array()}};
    size_t bytes=2048,tokens=0;bool truncated=false;
    const auto token=[](const json& value){const auto type=value.at(0).get<int32_t>();
        if(type==1||type==2){const auto& text=value.at(1).get_ref<const std::string&>();
            return json{{"type",type},{"text_bytes",text.size()},{"text_sha",diagnostic_sha(text)}};}
        return json{{"type",type},{"bits",value.at(1)}};
    };
    for(size_t index=0;index<records.size();++index){
        if(index>=2048){truncated=true;break;}
        const auto& source=records.at(index);
        json row={{"record_index",index},{"parent_index",index?source.at(0):json(nullptr)},
            {"child_index",source.at(1)},{"record_sha",diagnostic_sha(source.dump())},
            {"label",token(source.at(2))},{"token_count",source.at(3).size()},{"tokens",json::array()}};
        const auto base=row.dump().size()+1;if(base>diagnostic_bytes-bytes){truncated=true;break;}bytes+=base;
        for(size_t ordinal=0;ordinal<source.at(3).size();++ordinal){
            if(tokens>=4096){truncated=true;break;}
            auto value=token(source.at(3).at(ordinal));value["token_index"]=ordinal;
            const auto size=value.dump().size()+1;if(size>diagnostic_bytes-bytes){truncated=true;break;}
            bytes+=size;++tokens;row["tokens"].push_back(std::move(value));
        }
        out["records"].push_back(std::move(row));
    }
    for(size_t index=0;index<links.size();++index){
        if(index>=2048){truncated=true;break;}
        const auto& source=links.at(index);json row={{"id",source.at(0)},{"kind",source.at(1)}};
        if(source.at(1)=="inline")row["record_index"]=source.at(2);
        else if(source.at(1)=="external")row["owner_sha"]=diagnostic_sha(source.at(2).get<std::string>());
        const auto size=row.dump().size()+1;if(size>diagnostic_bytes-bytes){truncated=true;break;}
        bytes+=size;out["links"].push_back(std::move(row));
    }
    out["retained_records"]=out.at("records").size();out["retained_tokens"]=tokens;out["retained_links"]=out.at("links").size();
    out["truncated"]=truncated;out["state"]=truncated?"truncated":"complete";
    out["cause"]=truncated?"Selected-source Record diagnostic budget reached; full record and manifest digests retained":"";
    return out;
}catch(const std::exception& exception){
    // Diagnostics cannot alter authority admission. Retain a visible privacy-safe failure.
    return {{"state","failed"},{"record_sha",witness.sha},{"cause","Selected-source Record diagnostic generation failed"},
        {"exception_sha",diagnostic_sha(exception.what())}};
}
}
