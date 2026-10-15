#include "developer_staff/shared_state.h"
#include <iostream>
#include <limits>
#include <stdexcept>
namespace staff=mt2mp::game::developer_staff;
namespace {
unsigned checks=0;
void check(bool ok,const char* cause){++checks;if(!ok)throw std::runtime_error(cause);}
staff::PoolState pool(){staff::PoolState s;s.count=1;for(int32_t i=0;i<50;++i){staff::PoolRow r;r.id=49-i;r.employed=r.active=i==0;s.rows.push_back(r);s.indoor.push_back(r.id);}return s;}
void run(){std::string e;auto source=pool(),out=source;
    check(staff::decode(staff::encode(source),out,e)&&source==out,"full50 portable pool roundtrip rejected");
    check(out.rows[0].id==49&&out.indoor[0]==49,"wire changed persisted ID into row index");
    auto hired=source;hired.count=2;hired.rows[1].employed=hired.rows[1].active=true;
    check(staff::count_effect(true,source,hired,e),"original settled Hire projection rejected");
    auto fired=hired;fired.count=1;fired.rows[1].employed=false;
    check(staff::count_effect(false,hired,fired,e)&&fired.rows[1].active,"original Fire falsely required synchronous inactive actor");
    auto changed=fired;changed.rows[1].active=false;check(!staff::count_effect(false,hired,changed,e),"early retirement substituted for original synchronous Fire");
    changed=hired;changed.rows[49].position[0]=1;check(!staff::count_effect(true,source,changed,e),"unrelated actor pose changed inside Hire receipt");
    auto raw=staff::encode(source);
    const auto reject=[&](nlohmann::json bad,const char* cause){check(!staff::decode(bad,out,e)&&out.rows.empty()&&!e.empty(),cause);};
    auto bad=raw;bad["count"]=true;reject(bad,"boolean worker count accepted");
    bad=raw;bad["count"]=51;reject(bad,"worker count51 accepted");
    bad=raw;bad["rows"].erase(0);reject(bad,"incomplete fixed50 pool accepted");
    bad=raw;bad["rows"][1][0]=bad["rows"][0][0];reject(bad,"duplicate persisted actor ID accepted");
    bad=raw;bad["rows"][0][1]=8;reject(bad,"native job enum8 accepted");
    bad=raw;bad["rows"][0][2]=1;reject(bad,"integer employment flag accepted");
    bad=raw;bad["rows"][0][3]=1;reject(bad,"integer active flag accepted");
    bad=raw;bad["rows"][0][5]="false";reject(bad,"string job-link flag accepted");
    bad=raw;bad["rows"][0][6]=std::numeric_limits<double>::infinity();reject(bad,"infinite native position accepted");
    bad=raw;bad["rows"][0].push_back(0);reject(bad,"extra row field accepted");
    bad=raw;bad["extra"]=false;reject(bad,"extra pool field accepted");
    bad=raw;bad["outdoor"].push_back(49);reject(bad,"duplicate native manager membership accepted");
    bad=raw;bad["indoor"].erase(0);reject(bad,"missing manager registration accepted");
    bad=raw;bad["rows"][0][4]=true;reject(bad,"on-map actor remained only in indoor manager");
    changed=source;changed.rows[49].job_link=true;check(staff::valid(changed,e),"real jobful row was hidden by wire codec instead of native ownership admission");
    auto reordered=source;std::swap(reordered.indoor[0],reordered.indoor[1]);check(staff::valid(reordered,e)&&reordered!=source,"manager update order was discarded");
    check(!staff::count_effect(true,source,reordered,e),"unrelated manager reorder accepted as Hire");
}
}
int main(){try{run();std::cout<<checks<<" fixed50 pool codec/effect checks passed\n";return 0;}catch(const std::exception& ex){std::cerr<<"error: "<<ex.what()<<'\n';return 1;}}
