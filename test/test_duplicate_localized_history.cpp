#define main group_reader_fixture_main
#include "test_group_reader.cpp"
#undef main
#include "duplicate_localized_history.h"
namespace lh=mt2mp::game::duplicate_localized_history;
int main()try{
    constexpr uintptr_t value=0x91000,args=0x92000,nested=args+0x20,nested_args=0x94000;
    f::Memory memory;std::map<uintptr_t,std::string> texts;
    auto setup=[&](){memory={};texts={{value,"Built in {region} on day {day}, cost {cash}"},{args,"region"},
        {nested,"{name}"},{nested_args,"name"},{nested_args+0x20,"Forest"},{args+0x68,"day"},{args+0xd0,"cash"}};
        memory.put(value+0x20,std::array<uintptr_t,3>{args,args+3*0x68,args+5*0x68});
        memory.put(args+0x60,int32_t(0));memory.put(nested+0x20,std::array<uintptr_t,3>{nested_args,nested_args+0x68,nested_args+0x68});
        memory.put(nested_args+0x60,int32_t(0));memory.put(nested_args+0x40,std::array<uintptr_t,3>{0,0,0});
        memory.put(args+0x68+0x60,int32_t(1));memory.put(args+0x68+0x58,int32_t(32));
        memory.put(args+0xd0+0x60,int32_t(2));memory.put(args+0xd0+0x58,int32_t(2));memory.put(args+0xd0+0x5c,std::bit_cast<uint32_t>(25.5f));
    };
    auto text=[&](uintptr_t p,std::string& out,std::string& e){const auto found=texts.find(p);
        if(found==texts.end()){e="missing fixture text";return false;}out=found->second;return true;};
    std::string error;lh::Value result;lh::Budget budget{16,16,4096};
    auto read=[&](){return lh::read(memory,value,budget,text,result,error);};setup();
    f::need(read(),error);const auto expected=result;
    f::need(result.size()==3&&result[0].arguments.size()==3&&result[0].arguments[0].child==1&&
        result[1].arguments[0].child==2&&result[2].text=="Forest"&&result[0].arguments[1].integer==32&&
        result[0].arguments[2].real==std::bit_cast<uint32_t>(25.5f),"nested original localized value lost");
    memory.put(args+0x68+0x5c,uint32_t(0x7fc00000));
    f::need(read()&&result==expected,"inactive numeric union bytes affected native localized equality");
    memory.put(value+0x30,args+3*0x68);f::need(read()&&result==expected,"copy capacity changed localized value");
    setup();texts[nested_args+0x20]="Desert";f::need(read()&&result!=expected,"changed nested localized text ignored");
    setup();memory.put(args+0x68+0x58,int32_t(33));f::need(read()&&result!=expected,"changed integer argument ignored");
    setup();memory.put(args+0xd0+0x58,int32_t(3));f::need(read()&&result!=expected,"changed float precision ignored");
    setup();memory.put(args+0xd0+0x5c,std::bit_cast<uint32_t>(25.75f));f::need(read()&&result!=expected,"changed float value ignored");
    auto bad=[&](auto change){setup();change();f::need(!read(),"invalid localized description admitted");};
    bad([&](){memory.put(value+0x28,args+1);});
    bad([&](){memory.put(value+0x30,args);});
    bad([&](){memory.put(args+0x60,int32_t(3));});
    bad([&](){memory.put(args+0xd0+0x5c,uint32_t(0x7fc00000));});
    bad([&](){memory.put(nested+0x20,std::array<uintptr_t,3>{args,args+0x68,args+0x68});});
    setup();budget.strings=2;f::need(!read(),"localized string resource budget ignored");
    budget={16,2,4096};f::need(!read(),"localized argument resource budget ignored");
    budget={16,16,5};f::need(!read(),"localized text resource budget ignored");
    budget={16,16,4096};setup();bool changed=false;
    auto changing_text=[&](uintptr_t p,std::string& out,std::string& e){
        if(p==args&&!changed){changed=true;memory.put(value+0x30,args+4*0x68);}return text(p,out,e);};
    f::need(!lh::read(memory,value,budget,changing_text,result,error)&&error.find("changed during read")!=std::string::npos,
        "changed native vector header accepted");
    std::cout<<f::checks<<" actual localized Building inverse value cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
