#include "dungeon_cells_native.h"
#include "dungeon_cells_capture.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <stdexcept>
namespace native=mt2mp::game::dungeon_cells_native;
namespace capture=mt2mp::game::dungeon_cells_capture;
struct Port {
    std::string failed,cleanup_failed;std::vector<std::string> calls;std::map<std::string,int> counts;bool changes=true;
    bool call(const std::string& name,std::string& error){calls.push_back(name);++counts[name];if(name==failed||name==cleanup_failed){error="original "+name+" failed";return false;}return true;}
    bool ready(const std::vector<native::Cell>&,std::string& e){return call("ready",e);}
    bool start(uintptr_t& graph,bool& started,std::string& e){graph=99;started=true;return call("start",e);}
    bool active(uintptr_t graph,std::string& e){return call(graph?"active":"clear",e);}
    bool cell(const native::Cell&,bool& changed,std::string& e){changed=changes;return call("cell",e);}
    bool drain(uintptr_t,std::string& e){return call("drain",e);}
    bool finish(std::string& e){return call("finish",e);}
    bool end(std::string& e){return call("end",e);}
    bool release_marker(std::string& e){return call("marker",e);}
    bool commit(uintptr_t,std::string& e){return call("commit",e);}
};
int main()try{
    unsigned checks=0;const auto check=[&](bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);};
    std::string error;const std::vector<native::Cell> cells{{7,native::Edit::Close,""},{9,native::Edit::Close,""}};
    Port port;native::Receipt receipt;check(native::apply(port,cells,receipt,error),error);
    check(receipt.committed&&receipt.closed&&receipt.changes==2&&receipt.drains==2&&!receipt.active,"Complete original receipt differs");
    check(port.calls==std::vector<std::string>{"ready","start","active","cell","drain","cell","drain","finish","clear","end","marker","commit"},"Original active/edit/Undo ownership order differs");
    const auto previous=port.calls;check(!native::apply(port,cells,receipt,error)&&port.calls==previous,"Native mutation was replayed");
    for(const auto* failed:{"ready","start","active","cell","drain","finish","clear","end","marker","commit"}){
        Port broken;broken.failed=failed;native::Receipt actual;check(!native::apply(broken,cells,actual,error),"Failed original native step was accepted");
        check(error.find("original "+std::string(failed)+" failed")!=std::string::npos,"Original failure cause was replaced");
        check(broken.counts["clear"]<=1&&broken.counts["end"]<=1&&broken.counts["marker"]<=1,"Failed cleanup was retried");
        if(std::string(failed)!="commit")check(!actual.committed&&broken.counts["commit"]==0,"Failed or incomplete transaction committed Undo");
    }
    Port both;both.failed="cell";both.cleanup_failed="clear";native::Receipt failed;
    check(!native::apply(both,cells,failed,error)&&error.find("original cell failed")!=std::string::npos&&error.find("original clear failed")!=std::string::npos,"Cleanup hid the original cell failure");
    Port skipped;skipped.changes=false;native::Receipt unchanged;check(!native::apply(skipped,cells,unchanged,error)&&!unchanged.committed&&skipped.counts["drain"]==0,"Unchanged cells created a success or drained a nonexistent deletion");
    capture::Collector collector;capture::Stroke stroke;stroke.receiver=10;stroke.dungeon=20;stroke.key="owner";stroke.snapshot="snapshot";stroke.intent.theme="stone";
    check(collector.append(stroke,7,error)&&collector.append(stroke,7,error)&&collector.append(stroke,9,error),error);
    auto foreign=stroke;foreign.dungeon=21;check(!collector.append(foreign,11,error),"Stroke silently crossed Dungeon owners");
    std::optional<capture::Stroke> result;check(!collector.take(11,result,error)&&collector.active(),"Foreign release lost the owned stroke");
    check(collector.take(10,result,error)&&result&&result->intent.cells==std::vector<int32_t>{7,9}&&!collector.active(),"Ordered unique original stroke cells differ");
    check(collector.take(10,result,error)&&!result,"Empty release replayed the old stroke");
    std::cout<<checks<<" Dungeon native once-only graph, cleanup and normal stroke capture comparisons passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
