#include "duplicate_records.h"
#include <iostream>
#include <stdexcept>
namespace rec=mt2mp::subscriber_lifecycle;
namespace dr=mt2mp::game::duplicate_records;
void need(bool b,const std::string& e){if(!b)throw std::runtime_error(e);}
rec::Graph fixture(uintptr_t base){rec::Graph g;
    g.records={{SIZE_MAX,0,{2,0,"building"},{}},{0,0,{2,0,"linkId"},{{4,1,""}}},{0,1,{1,0,"plan"},{{4,2,""}}},
        {0,2,{1,0,"layout"},{{3,0x3f800000,""}}},{0,3,{2,0,"art"},{{2,0,"original art payload"}}}};
    g.private_links={{2,base+20,false},{1,base,true}};return g;}

void diagnostics(){
    using json=nlohmann::json;std::string error;dr::Witness plain,observed;json first,second;unsigned owner_calls=0;
    auto owner=[&](uintptr_t,std::string& key,std::string&){++owner_calls;key="private canonical owner";return true;};
    const auto source=fixture(100);need(dr::summarize(source,100,owner,plain,error),error);owner_calls=0;
    need(dr::summarize(source,100,owner,observed,error,&first)&&observed==plain&&owner_calls==1,
         "Record diagnostics changed the original witness or repeated an owner lookup");
    need(first.at("state")=="complete"&&first.at("record_sha")==plain.sha&&first.at("records").size()==source.records.size(),
         "complete selected Record diagnostic is absent");
    const auto output=first.dump();
    need(output.find("original art payload")==std::string::npos&&output.find("private canonical owner")==std::string::npos&&
         output.find("building")==std::string::npos,"Record diagnostic leaked string content or external owner");
    need(first.at("records").at(3).at("parent_index")==0&&first.at("records").at(3).at("child_index")==2&&
         first.at("records").at(3).at("tokens").at(0).at("bits")==0x3f800000,
         "Record diagnostic lost exact numeric tree path/type/scalar bits");
    need(dr::summarize(fixture(200),200,owner,observed,error,&second)&&first==second,
         "local native allocation pointers leaked into diagnostics");
    auto changed=source;++changed.records[3].tokens[0].bits;
    need(dr::summarize(changed,100,owner,observed,error,&second)&&second.at("records_sha")!=first.at("records_sha")&&
         second.at("manifest_sha")==first.at("manifest_sha")&&second.at("records").at(3).at("tokens").at(0).at("bits")==0x3f800001,
         "scalar difference was not isolated from the canonical manifest");
    changed=source;changed.records[4].tokens[0].text+=" changed";
    need(dr::summarize(changed,100,owner,observed,error,&second)&&second.at("records").at(4).at("tokens").at(0).at("text_sha")!=
         first.at("records").at(4).at("tokens").at(0).at("text_sha"),"string-token hash concealed changed original text");
    auto foreign=[](uintptr_t,std::string& key,std::string&){key="different canonical owner";return true;};
    need(dr::summarize(source,100,foreign,observed,error,&second)&&second.at("records_sha")==first.at("records_sha")&&
         second.at("manifest_sha")!=first.at("manifest_sha"),"external canonical link difference was not isolated");
    auto large=source;large.records[2].tokens.resize(10000,rec::Token{4,17,{}});
    need(dr::summarize(large,100,owner,plain,error)&&dr::summarize(large,100,owner,observed,error,&second)&&plain==observed&&
         second.at("truncated")==true&&second.at("state")=="truncated"&&!second.at("cause").get<std::string>().empty()&&
         second.dump().size()<=dr::diagnostic_bytes,"bounded diagnostics changed admission or concealed truncation");
}
#include "duplicate_uptime_record_cases.inc"
int main()try{
    diagnostics();uptime_record_cases::run();
    unsigned count=0;std::string e;dr::Witness a,b;auto owner=[](uintptr_t p,std::string& key,std::string& why){if(p==120||p==220){key="region:67";return true;}why="exact foreign owner";return false;};
    auto first=fixture(100),second=fixture(200);need(dr::summarize(first,100,owner,a,e),e);need(dr::summarize(second,200,owner,b,e)&&a==b,"local pointer crossed shared record digest");count+=2;
    for(size_t i=2;i<5;++i){auto changed=second;if(i==4)changed.records[i].tokens[0].text+=" changed";else ++changed.records[i].tokens[0].bits;
        need(dr::summarize(changed,200,owner,b,e)&&a.sha!=b.sha,"original plan/layout/art token omitted");++count;}
    second.private_links[0].object=300;need(!dr::summarize(second,200,owner,b,e)&&e=="exact foreign owner","external owner fallback hid missing binding");++count;
    second=fixture(200);second.private_links.push_back({2,230,false});need(!dr::summarize(second,200,owner,b,e),"duplicate link ID accepted");++count;
    second=fixture(200);second.private_links[0].saved_link_id=3;need(!dr::summarize(second,200,owner,b,e),"missing SaveContext ID accepted");++count;
    second=fixture(200);second.private_links[1].has_inline_definition=false;need(!dr::summarize(second,200,owner,b,e),"inline manifest mismatch accepted");++count;
    second=fixture(200);second.records[1].label.type=1;need(dr::summarize(second,200,owner,b,e),e);++count;
    std::cout<<count<<" actual original RecordGraph/canonical pointer-manifest cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
