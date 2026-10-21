#include "dungeon_fill_record.h"
#include <iostream>
#include <stdexcept>
namespace {
namespace fill=mt2mp::game::dungeon_fill;
using json=nlohmann::json;
int checks=0;
void expect(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
json value(){return json::array({
    json::array({int64_t(-1),uint64_t(0),json::array({1,uint64_t(0),"mmoFill"}),json::array()}),
    json::array({int64_t(0),uint64_t(0),json::array({1,uint64_t(0),"fillType"}),json::array({json::array({1,uint64_t(0),"existing-native-fill"})})}),
    json::array({int64_t(0),uint64_t(1),json::array({1,uint64_t(0),"floatValues"}),json::array()}),
    json::array({int64_t(2),uint64_t(0),json::array({1,uint64_t(0),"value"}),json::array({json::array({3,uint64_t(0x3f800000),""})})})});}
void run(){std::string error;std::vector<mt2mp::subscriber_lifecycle::Record> rows;const auto before=value();
    expect(fill::records(before,rows,error)&&rows.size()==4&&rows[3].parent==2&&rows[3].tokens[0].bits==0x3f800000,"complete nested native Fill value retains exact float bits and order");
    auto changed=before;changed[0][2][2]="mmoLogo";expect(!fill::records(changed,rows,error),"foreign native root rejected");
    changed=before;changed[3][0]=int64_t(3);expect(!fill::records(changed,rows,error),"cyclic/self parent rejected before allocation");
    changed=before;changed[2][1]=uint64_t(0);expect(!fill::records(changed,rows,error),"repeated native child order rejected");
    changed=before;changed[3][3][0][0]=5;expect(!fill::records(changed,rows,error),"unsupported original token type rejected");
    changed=before;changed[3][3][0][1]=uint64_t(0x7f800000);expect(!fill::records(changed,rows,error),"nonfinite Fill parameter rejected");
    changed=before;changed[1][3][0][1]=uint64_t(1);expect(!fill::records(changed,rows,error),"string token cannot carry hidden numeric bits");
    changed=before;changed[3][3][0][2]="hidden";expect(!fill::records(changed,rows,error),"numeric token cannot carry hidden string");
    changed=before;changed[1][3][0][2]=std::string("bad\0fill",8);expect(!fill::records(changed,rows,error),"embedded NUL native type rejected");
    changed=before;changed[0][0]=false;expect(!fill::records(changed,rows,error),"Boolean parent is not native parent index");
    changed=before;changed[1][3][0][1]=uint64_t(UINT32_MAX)+1;expect(!fill::records(changed,rows,error),"overflowing token bits rejected");
    changed=before;changed.push_back(changed[3]);expect(!fill::records(changed,rows,error),"unsequenced extra property rejected");
    expect(!fill::records(json::array(),rows,error),"missing Fill definition rejected");
}
}
int main(){try{run();std::cout<<checks<<" Dungeon Fill Record checks passed\n";return 0;}
catch(const std::exception& exception){std::cerr<<"Dungeon Fill Record: "<<exception.what()<<'\n';return 1;}}
