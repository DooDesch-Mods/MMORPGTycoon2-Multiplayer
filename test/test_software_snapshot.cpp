#include "mt2mp/net/software_snapshot.h"
#include <iostream>
using namespace mt2mp::net;
using json=nlohmann::json;
static void require(bool value,const char* cause) { if(!value) throw std::runtime_error(cause); }
static void rejected(const json& packet) {
    try { parse_initial_software_cache(packet); }
    catch(const std::runtime_error&) { return; }
    throw std::runtime_error("invalid software snapshot was accepted");
}
int main() {
    try {
        const json valid={{"revision",1},{"rating",700},{"fun",40}};
        auto pair=parse_initial_software_cache(valid);
        require(pair.rating==700&&pair.fun==40,"host cache changed during parsing");
        rejected(json::array({1,700,40}));
        for(auto key:{"revision","rating","fun"}) { auto bad=valid; bad.erase(key); rejected(bad); }
        auto bad=valid; bad["pointer"]=123; rejected(bad);
        for(auto revision:{json(0),json(-1),json(2),json(1.0),json(true),json("1")}) {
            bad=valid; bad["revision"]=revision; rejected(bad);
        }
        for(auto rating:{json(-1),json(1001),json(true),json("700"),json(1e100),json(nullptr)}) {
            bad=valid; bad["rating"]=rating; rejected(bad);
        }
        bad=valid; bad["fun"]=1e100; rejected(bad);
        bad=valid; bad["fun"]=false; rejected(bad);
        bad=valid; bad["fun"]=-50.0;
        require(parse_initial_software_cache(bad).fun==-50,"unbounded native fun was arbitrarily clamped");
        std::cout<<"software snapshot validation passed\n"; return 0;
    } catch(const std::exception& error) { std::cerr<<"software snapshot failure: "<<error.what()<<'\n'; return 1; }
}
