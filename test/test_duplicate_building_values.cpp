#define main owned_record_fixture_main
#include "test_duplicate_owned_record.cpp"
#undef main
#include "duplicate_building_values.h"
#include "duplicate_history_value_calls.h"
namespace bv=mt2mp::game::duplicate_building_values;
namespace loc=mt2mp::game::duplicate_localized_history;
struct Port {
    loc::Value value{{"Built in {region}",{{"region",0,0,0,1}}},{"Forest",{}}};
    std::vector<std::string> calls;bool fail_get=false,fail_read=false,throw_read=false,fail_args=false,fail_text=false;
    bool get(uintptr_t building,uintptr_t& pointer,std::string& e){calls.push_back("get");
        if(fail_get||building!=42){e="first native GetFluff cause";return false;}pointer=0x90000;return true;}
    bool read(uintptr_t pointer,loc::Value& out,std::string& e){calls.push_back("read");
        if(throw_read)throw std::runtime_error("fixture allocation failure");
        if(fail_read||pointer!=0x90000){e="first localized read cause";return false;}out=value;return true;}
    bool destroy(uintptr_t pointer,bool arguments,std::string& e){calls.push_back(arguments?"arguments":"text");
        if(pointer!=0x90000||(arguments?fail_args:fail_text)){e=arguments?"arguments cause":"text cause";return false;}return true;}
};
int main()try{
    using fixture::need;std::string error,sha;bv::LocalizedReceipt receipt;Port port;
    need(bv::localized(port,42,sha,receipt,error),error);const auto original=sha;
    need(sha.size()==64&&receipt.returned&&receipt.read&&receipt.arguments_destroyed&&receipt.text_destroyed&&
        port.calls==std::vector<std::string>{"get","read","arguments","text"},"owned localized call/cleanup order differs");
    port={};port.value[1].text="Desert";need(bv::localized(port,42,sha,receipt,error)&&sha!=original,"nested original description was omitted");
    for(int mode=0;mode<6;++mode){port={};error.clear();
        if(mode==0)port.fail_get=true;if(mode==1)port.fail_read=true;if(mode==2)port.fail_args=true;
        if(mode==3)port.fail_text=true;if(mode==4){port.fail_read=true;port.fail_args=true;port.fail_text=true;}
        if(mode==5)port.throw_read=true;
        need(!bv::localized(port,42,sha,receipt,error)&&sha.empty(),"failed owned localized value escaped as success");
        need(port.calls==(mode==0?std::vector<std::string>{"get"}:std::vector<std::string>{"get","read","arguments","text"}),
            "cleanup was skipped or repeated after original localized read");
        if(mode==4)need(error=="first localized read cause; cleanup: arguments cause; text cause","cleanup replaced the original read failure");
        if(mode==5)need(error.find("fixture allocation failure")!=std::string::npos&&receipt.arguments_destroyed&&receipt.text_destroyed,
            "inspection exception skipped original owners");
    }
    port={};auto invalid=port.value;invalid.push_back({"unreachable",{}});
    need(!loc::summarize(invalid,sha,error),"unowned nested localized value accepted");
    invalid=port.value;invalid[0].arguments[0].child=0;need(!loc::summarize(invalid,sha,error),"cyclic localized value accepted");
    auto memory=fixture::setup();native_record::Graph graph;
    need(native_record::read_graph(memory,fixture::root,fixture::context,{10,10,1024,10},graph,error),error);
    std::string stored_sha;need(bv::stored(memory,fixture::root,{10,10,1024,10},stored_sha,error),error);
    constexpr uintptr_t image=0x140000000,object=0x20000;
    for(auto kind:{bv::OwnedKind::Layout,bv::OwnedKind::Plan,bv::OwnedKind::Bugs}){
        bv::Methods methods;need(bv::methods(kind,methods,error),error);memory.put(object,image+methods.addresspoint);size_t calls=0;
        auto capture=[&](uintptr_t pointer,uintptr_t prepare,uintptr_t save,native_record::Graph& out,std::string& e){++calls;
            if(pointer!=object||prepare!=methods.prepare||save!=methods.save){e="native owned serializer method differs";return false;}
            out=graph;return true;};
        need(bv::live(memory,image,object,kind,capture,sha,error)&&sha==stored_sha&&calls==1,"live original serializer differs from detached owned record");
        memory.put(object,image+methods.addresspoint+8);calls=0;
        need(!bv::live(memory,image,object,kind,capture,sha,error)&&calls==0&&sha.empty(),"foreign owned payload entered original serializer");
        memory.put(object,image+methods.addresspoint);auto foreign=graph;foreign.private_links[0].has_inline_definition=false;
        auto external=[&](uintptr_t,uintptr_t,uintptr_t,native_record::Graph& out,std::string&){out=foreign;return true;};
        need(!bv::live(memory,image,object,kind,external,sha,error),"detached owned payload accepted an external pointer manifest");
    }
    bv::Methods methods;need(!bv::methods(bv::OwnedKind(0),methods,error),"foreign owned payload kind accepted");
    std::cout<<fixture::checks<<" actual Building localized cleanup and owned serializer binding cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
