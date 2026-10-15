#pragma once
#include "native_open_witness.h"
#include "typed_action/action_session.h"
#include <array>
#include <string>
#include <vector>
namespace mt2mp::game::developer_staff {
using Origin=building_mixed_identity::opened_file::CurrentWorld;
struct Row {
    int32_t id=-1,slot=-1,state=0;
    bool employed=false,active=false,on_map=false,job_link=false;
    std::array<float,3> position{};
    bool operator==(const Row&)const=default;
};
struct State {
    int32_t business_count=0,lounge_count=0;
    std::vector<Row> rows;
    bool operator==(const State&)const=default;
};
struct Pin {
    Origin origin;action_session::Ledger ledger;
    float time_scale=0,pause_scale=0;
};
struct Local {
    uintptr_t lounge=0,business=0,data=0,indoor=0,outdoor=0;
    int32_t capacity=0;
    std::vector<uintptr_t> developers,inside,outside;
    bool operator==(const Local&)const=default;
};
struct Capture {Pin pin;uintptr_t image=0;uint32_t thread=0;State state;Local local;};
struct Port {
    virtual ~Port()=default;
    virtual bool pin(Pin&,std::string&)=0;
    virtual bool resolver(uintptr_t&,uint32_t&,std::string&)=0;
    virtual bool copy(uintptr_t,void*,size_t,std::string&)=0;
};
bool read(Port&,Capture&,std::string&);
bool read_process(Capture&,std::string&);
nlohmann::json encode(const Capture&);
bool same_origin(const Origin&,const Origin&);
enum class Command {Hire,Fire,Show};
bool payment(Command,const action_session::Ledger&,const action_session::Ledger&,std::string&);
}
