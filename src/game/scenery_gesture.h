#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <string>
#include <vector>
namespace mt2mp::game::scenery_gesture {
using Point=std::array<float,3>;
enum class Step {Hover,Press,Drag,Release};
struct Receipt {
    size_t attempted=0,returned=0,stamps=0;
    bool captured=false;
    std::string action_uuid,cause;
};
template<class Port>bool execute(Port& port,const std::vector<Point>& points,Receipt& receipt,std::string& error){
    if(receipt.attempted||receipt.captured){error="Scenery gesture already attempted; no replay";return false;}
    if(points.size()<2||points.size()>32){error="Scenery gesture requires 2..32 points";return false;}
    for(const auto& point:points)for(float value:point)if(!std::isfinite(value)){error="Scenery gesture point is nonfinite";return false;}
    auto call=[&](Step step,const Point& point){
        if(!port.current(error))return false;
        ++receipt.attempted;
        if(!port.invoke(step,point,error))return false;
        ++receipt.returned;
        return port.observed(step,error);
    };
    try{
        if(!call(Step::Hover,points.front())||!call(Step::Press,points.front())){receipt.cause=error;return false;}
        for(size_t i=1;i<points.size();++i)if(!call(Step::Drag,points[i])){receipt.cause=error;return false;}
        if(!call(Step::Release,points.back())||!port.completed(receipt.action_uuid,receipt.stamps,error)){
            receipt.cause=error;return false;
        }
        receipt.captured=true;error.clear();return true;
    }catch(const std::exception& failure){error=std::string("Scenery gesture callback failed: ")+failure.what();receipt.cause=error;return false;}
}
}
