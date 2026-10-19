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
int main()try{
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
