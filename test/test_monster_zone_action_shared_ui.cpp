#include "monster_zone_shared_ui.h"
#include <iostream>
#include <stdexcept>
namespace u=mt2mp::game::monster_zone_shared_ui;using json=nlohmann::json;using mt2mp::action_session::Binding;
void need(bool v,const std::string& e){if(!v)throw std::runtime_error(e);}
int main(){try{unsigned count=0,calls=0;json controller{{"initialized",true},{"started",true},{"locked",false},{"pending",false}};
    const std::string snapshot="11111111-1111-4111-8111-111111111111";Binding expected{3,4},actual;std::string e;
    auto read=[&](const std::string& value,Binding& out,std::string&){need(value==snapshot,"wrong binding snapshot");++calls;out=expected;return true;};
    need(u::admit(controller,true,false,snapshot,actual,read,e)&&actual==expected&&calls==1,e);++count;
    for(const auto* key:{"initialized","started","locked","pending"}){auto bad=controller;bad[key]=!bad.at(key).get<bool>();calls=0;
        need(!u::admit(bad,true,false,snapshot,actual,read,e)&&!calls,"invalid Controller reached provider");++count;}
    for(const auto& flags:{std::pair{false,false},std::pair{true,true}}){calls=0;need(!u::admit(controller,flags.first,flags.second,snapshot,actual,read,e)&&!calls,"non-owned or pending capture accepted");++count;}
    auto unavailable=[](const std::string&,Binding&,std::string& out){out="actual current Origin unavailable";return false;};
    need(!u::admit(controller,true,false,snapshot,actual,unavailable,e)&&e=="actual current Origin unavailable",e);++count;
    need(u::current(snapshot,expected,snapshot,read,e),e);++count;
    calls=0;need(!u::current(snapshot,expected,"other",read,e)&&!calls&&e.find("snapshot")!=std::string::npos,e);++count;
    auto different=[](const std::string&,Binding& out,std::string&){out={3,5};return true;};
    need(!u::current(snapshot,expected,snapshot,different,e)&&e.find("binding")!=std::string::npos,e);++count;
    need(!u::current(snapshot,expected,snapshot,unavailable,e)&&e=="actual current Origin unavailable",e);++count;
    const json before={{"cash",100},{"zone_count",0},{"random",{1,2,3,4}}},receipt={{"captured",true},{"submitted",false}};
    need(u::captured(before,before,receipt,true,"",e),e);++count;
    auto after=before;after["zone_count"]=1;need(!u::captured(before,after,receipt,true,"",e)&&e.find("changed native")!=std::string::npos,e);++count;
    auto bad=receipt;bad["submitted"]=true;need(!u::captured(before,before,bad,true,"native original cause",e)&&e.find("native original cause")!=std::string::npos,e);++count;
    bad=receipt;bad["captured"]=false;need(!u::captured(before,before,bad,true,"Press owner mismatch",e)&&e.find("Press owner mismatch")!=std::string::npos,e);++count;
    need(!u::captured(before,before,receipt,false,"",e),"absent pending capture accepted");++count;
    std::cout<<count<<" actual shared UI admission/receipt cases passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
