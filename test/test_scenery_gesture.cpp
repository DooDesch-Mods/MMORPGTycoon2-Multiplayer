#include "scenery_gesture.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mt2mp::game::scenery_gesture;
static unsigned checks=0;
void need(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
struct Port {
    std::vector<Step> calls;
    unsigned owners=0,observations=0;
    int fail_owner=-1,fail_call=-1,fail_observation=-1;
    bool throw_call=false,complete_failure=false;
    bool current(std::string& e){if(int(owners++)==fail_owner){e="original owner changed";return false;}return true;}
    bool invoke(Step step,const Point&,std::string& e){calls.push_back(step);if(int(calls.size()-1)==fail_call){if(throw_call)throw std::runtime_error("original callback exception");e="original callback failed";return false;}return true;}
    bool observed(Step,std::string& e){if(int(observations++)==fail_observation){e="original capture rejected";return false;}return true;}
    bool completed(std::string& id,size_t& stamps,std::string& e){if(complete_failure){e="original Finish did not queue";return false;}id="captured-action";stamps=3;return true;}
};
int main()try{
    const std::vector<Point> points{{0,0,0},{10,0,0},{20,0,0}};
    std::string error;
    Port normal;Receipt good;
    need(execute(normal,points,good,error),"complete gesture rejected");
    need(normal.calls==std::vector<Step>{Step::Hover,Step::Press,Step::Drag,Step::Drag,Step::Release},"original callback order differs");
    need(good.attempted==5&&good.returned==5&&good.captured&&good.stamps==3&&good.action_uuid=="captured-action","capture receipt differs");
    need(!execute(normal,points,good,error)&&normal.calls.size()==5,"completed gesture replayed");
    for(int index=0;index<5;++index){
        Port owner;owner.fail_owner=index;Receipt r;
        need(!execute(owner,points,r,error)&&error=="original owner changed"&&owner.calls.size()==size_t(index),"owner failure passed to native call");
        Port failed;failed.fail_call=index;Receipt q;
        need(!execute(failed,points,q,error)&&q.attempted==size_t(index+1)&&q.returned==size_t(index)&&!q.captured&&q.cause=="original callback failed","uncertain callback cause/receipt lost");
        need(!execute(failed,points,q,error)&&failed.calls.size()==size_t(index+1),"uncertain callback replayed");
        Port capture;capture.fail_observation=index;Receipt s;
        need(!execute(capture,points,s,error)&&s.attempted==size_t(index+1)&&s.returned==s.attempted&&s.cause=="original capture rejected","capture failure ran later input");
    }
    Port exception;exception.fail_call=1;exception.throw_call=true;Receipt thrown;
    need(!execute(exception,points,thrown,error)&&thrown.attempted==2&&thrown.returned==1&&error.find("original callback exception")!=std::string::npos,"callback exception lost");
    Port finish;finish.complete_failure=true;Receipt missing;
    need(!execute(finish,points,missing,error)&&missing.returned==5&&!missing.captured&&missing.cause=="original Finish did not queue","return treated as capture confirmation");
    for(size_t size:{size_t(0),size_t(1),size_t(33)}){Port p;Receipt r;need(!execute(p,std::vector<Point>(size),r,error)&&p.calls.empty(),"invalid point count executed input");}
    auto invalid=points;invalid[1][0]=std::numeric_limits<float>::infinity();Port p;Receipt r;
    need(!execute(p,invalid,r,error)&&p.calls.empty(),"nonfinite point executed input");
    std::cout<<"PASS "<<checks<<" actual Scenery gesture driver assertions\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
