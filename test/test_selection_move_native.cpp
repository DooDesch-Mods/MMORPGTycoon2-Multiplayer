#include "selection_move_native.h"
#include <iostream>
#include <stdexcept>
namespace native=mt2mp::selection_move_native;namespace move=mt2mp::move_kiss;
unsigned opens=0,closes=0,begins=0,ends=0,dtors=0,moves=0,vectors=0,strings=0;int failure=0;uintptr_t current=0;
std::vector<uintptr_t> targets;
void ctor(void*,const char* text){if(std::string(text)!="Move")throw std::runtime_error("wrong original label");}
void begin(const void*){++begins;}void* open(const void*){++opens;return failure==1?nullptr:reinterpret_cast<void*>(0x1234);}
void move_ctor(void*,void* target,const void*){current=uintptr_t(target);}
void move_do(void*,void* graph){if(uintptr_t(graph)!=0x1234)throw std::runtime_error("wrong shared graph");++moves;if(failure==2&&moves==2)RaiseException(0xe0000001,0,0,nullptr);targets.push_back(current);}
void dtor(void*){++dtors;if(failure==3&&dtors==2)RaiseException(0xe0000002,0,0,nullptr);}
void close(){++closes;}void end(){++ends;}void vec(void*){++vectors;}void str(void*){++strings;}
void need(bool ok,const std::string& e){if(!ok)throw std::runtime_error(e);}
void reset(){opens=closes=begins=ends=dtors=moves=vectors=strings=0;targets.clear();}
int main(){try{unsigned cases=0;std::string e;move::Calls c{ctor,begin,open,move_ctor,move_do,dtor,close,end,vec,str};
    std::vector<native::Target> inputs{{0x1000,{}},{0x2000,{}},{0x3000,{}}};
    for(failure=0;failure<4;++failure){reset();native::Receipt r;const auto ok=native::execute(c,inputs,r,e);
        need(opens==1&&begins==1&&ends==1&&vectors==1&&strings==1,"scope cleanup missing/doubled");++cases;
        if(!failure){need(ok&&closes==1&&moves==3&&dtors==3&&targets==std::vector<uintptr_t>{0x1000,0x2000,0x3000},"one ordered common graph not executed");++cases;}
        else{need(!ok,"native partial/null failure hidden");++cases;
            if(failure==1)need(!closes&&!moves&&!dtors,"null graph attempted original moves");
            else need(closes==1&&moves==2&&dtors==2&&r.scope.exception!=0,"partial native failure continued/replayed");++cases;}
        const auto prior=moves;need(!native::execute(c,inputs,r,e)&&moves==prior,"attempted mutation replayed");++cases;
    }
    std::cout<<cases<<" actual Selection Move original graph/failure cases passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
