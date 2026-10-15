#pragma once
#include <array>
#include <string>
#include <vector>
namespace mt2mp::game::monster_zone_stroke {
using Point=std::array<float,3>;
// Each original callback is attempted once. A partial failure leaves the
// retained caller receipt Unknown; no Release or cleanup is synthesized.
template<class Call,class Observe>
bool run(const std::vector<Point>& points,Call&& call,Observe&& after_press,std::string& error){
    if(points.size()<2||points.size()>8){error="zone probe stroke point count outside 2..8";return false;}
    if(!call(0,points.front(),error)||!call(1,points.front(),error)||!after_press(error))return false;
    for(size_t i=1;i<points.size();++i)if(!call(2,points[i],error))return false;
    return call(3,points.back(),error);
}
}
