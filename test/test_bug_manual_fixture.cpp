#include "bug_authority/manual_fixture.h"
#include <iostream>
#include <map>
using namespace mt2mp::game;
namespace f=bug_authority::manual_action::fixture;
int checks=0;
void require(bool okay,const char* cause){++checks;if(!okay)throw std::runtime_error(cause);}
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;uintptr_t strings=0x30000;
    template<class T>void put(uintptr_t p,T value){const auto* raw=reinterpret_cast<const unsigned char*>(&value);for(size_t n=0;n<sizeof value;++n)bytes[p+n]=raw[n];}
    void text(uintptr_t p,const std::string& value){put(p,strings);put<uint64_t>(p+8,value.size());for(size_t i=0;i<=value.size();++i)put<char>(strings+i,value.c_str()[i]);strings+=256;}
    bool copy(uintptr_t p,void* out,size_t n,std::string& e){for(size_t i=0;i<n;++i){auto at=bytes.find(p+i);if(at==bytes.end()){e="fixture exact unreadable field";return false;}static_cast<unsigned char*>(out)[i]=at->second;}return true;}
};
struct Model {
    Memory m;developer_staff::Capture c;uintptr_t window=0x3000,detail=0x5000,bug=0x6000,button=0xa000,details=0x9000;
    Model(){c.image=0x10000000;c.pin.origin.mode=reinterpret_cast<void*>(0x1000);
        m.put<uintptr_t>(0x10a0,0x2000);m.put<int32_t>(0x10a8,1);m.put<int32_t>(0x10ac,1);m.put<uintptr_t>(0x2000,window);
        m.put(window,c.image+0x1722e60);m.text(window+0x1b0,"development");m.put<uintptr_t>(window+0x318,0);
        m.put<uintptr_t>(window+0x540,0x4000);m.put<uintptr_t>(0x4000,c.image+0x17015b0);m.put<uintptr_t>(0x43b8,0x7000);
        m.put<uintptr_t>(0x7000,c.image+0x16dfe90);m.put<uintptr_t>(0x4280,window);m.put<uintptr_t>(0x7280,window);m.put<uint8_t>(window+0x45f,1);
        page(0x7000,0x8000,"{dev_tab_bugs}");page(detail,details,"details");
        m.put<uintptr_t>(window+0x558,detail);m.put<uintptr_t>(detail,c.image+0x172abf0);m.put<uintptr_t>(detail+0x460,bug);
        m.put<uintptr_t>(detail+0x468,0xb000);m.put<uintptr_t>(0xb000,bug+0x18);m.put<uintptr_t>(bug+0x20,0xb000);
        m.put<uintptr_t>(detail+0x450,button);m.put<uintptr_t>(button,c.image+0x16e3ec0);
        for(auto p:{button,details})m.put<uintptr_t>(p+0x280,window);
        m.put<uintptr_t>(button+0x278,details);m.put<uintptr_t>(details+0x278,detail);
        for(size_t offset:{0x46c,0x46d,0x46e,0x584})m.put<uint8_t>(button+offset,0);
        m.put<uint8_t>(button+0x174,1);m.text(button+0x388,"MessageTo Parent BugButton");m.put<uintptr_t>(c.image+0x16e3ec0+0x1c8,c.image+0xb63ee0);
    }
    void page(uintptr_t view,uintptr_t value,const char* name){m.text(view+0x3e8,name);m.put<uintptr_t>(view+0x410,value);m.put<uintptr_t>(view+0x418,value);m.put<int32_t>(view+0x408,0);m.put<uintptr_t>(value,c.image+0x1745a80);m.text(value+0x1b0,name);}
    bool read(std::string& e,bool pressed=false){f::Control out;return f::button([&](auto p,auto target,auto n,auto& error){return m.copy(p,target,n,error);},c,bug,pressed,out,e);}
};
struct Click {
    int phase=0,fail=0;std::vector<int> inputs;
    bool step(std::string& e){++phase;if(phase==fail){e="original phase cause";return false;}return true;}
    bool before(f::Input,std::string& e){return step(e);}
    bool input(f::Input v,std::string& e){inputs.push_back(int(v));return step(e);}
    bool after(f::Input,std::string& e){return step(e);}
};
int main(){try{
    std::string e;Model m;require(m.read(e),"actual owned visible selected Button");m.m.put<uint8_t>(m.button+0x584,1);
    require(m.read(e,true),"actual pressed Button readback");require(!m.read(e),"pressed Button cannot start another click");
    for(int i=0;i<13;++i){Model bad;
        if(i==0)bad.m.put<uint8_t>(bad.window+0x45f,0);
        if(i==1)bad.m.put<int32_t>(0x7408,1);
        if(i==2)bad.m.text(0x73e8,"{dev_tab_policies}");
        if(i==3)bad.m.put<uintptr_t>(0xb000,0);
        if(i==4)bad.m.put<uint8_t>(bad.button+0x46d,1);
        if(i==5)bad.m.put<uint8_t>(bad.button+0x46e,1);
        if(i==6)bad.m.put<uint8_t>(bad.button+0x46c,1);
        if(i==7)bad.m.text(bad.button+0x388,"MessageTo Parent Hire");
        if(i==8)bad.m.put<uintptr_t>(bad.button+0x280,0xdead);
        if(i==9)bad.m.put<uintptr_t>(bad.details+0x278,bad.button);
        if(i==10)bad.m.put<uintptr_t>(bad.button+0x278,bad.detail);
        if(i==11)bad.m.put<uintptr_t>(bad.c.image+0x16e3ec0+0x1c8,bad.c.image+0x674780);
        if(i==12)bad.m.bytes.erase(bad.button+0x584);
        e.clear();require(!bad.read(e)&&!e.empty(),"foreign/unready/changed control rejects");
        if(i==12)require(e=="fixture exact unreadable field","original read cause retained");
    }
    for(const uintptr_t pane:{uintptr_t(0x8000),uintptr_t(0x9000)}){
        Model policy;policy.m.put<uintptr_t>(pane,policy.c.image+0x1705ee0);e.clear();
        require(!policy.read(e)&&e.find("requires original mmoView AP0x1745A80")!=std::string::npos,
            "policy subclass is not the shipped Bugs/details mmoView");
    }
    const std::string uuid="11111111-1111-4111-8111-111111111111";
    f::Json args{{"command","Show"},{"token",uuid},{"native_origin_uuid",uuid},{"epoch",1},{"generation",1},
        {"bug",f::Json::array({0,"action:"+uuid,0,std::string(64,'a'),0,0})},{"schedule",true}};
    for(const auto command:{"Show","Select","Click"}){f::Request r;args["command"]=command;require(f::parse(args,r,e),"all three fixed requests parse");}
    for(int i=0;i<10;++i){auto bad=args;f::Request r;
        if(i==0)bad["extra"]=0;if(i==1)bad.erase("bug");if(i==2)bad["command"]="MakeJob";if(i==3)bad["schedule"]=1;
        if(i==4)bad["epoch"]=-1;if(i==5)bad["generation"]=0;if(i==6)bad["token"]="invalid";
        if(i==7)bad["bug"][0]=2;if(i==8)bad["bug"][4]=-1;if(i==9)bad["bug"][3]=uuid;
        require(!f::parse(bad,r,e)&&!e.empty(),"malformed request rejects before native input");
    }
    for(int fail=0;fail<=9;++fail){Click click;click.fail=fail;uint32_t attempted=0,returned=0;e.clear();
        const bool okay=f::click(click,attempted,returned,e);
        require(okay==(fail==0),"exact original input phase result");
        require((returned&~attempted)==0,"never report an unattempted phase returned");
        if(!fail)require(click.inputs==std::vector<int>{0,1,2}&&attempted==7&&returned==7,"Hover Press Release exactly once");
        else{require(e=="original phase cause"&&click.phase==fail,"partial input stops with original cause");require(click.inputs.size()<=3,"no replay after input failure");}
    }
    std::cout<<checks<<" BugButton fixture checks PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<"FAIL "<<ex.what()<<" after "<<checks<<" checks\n";return 1;}}
