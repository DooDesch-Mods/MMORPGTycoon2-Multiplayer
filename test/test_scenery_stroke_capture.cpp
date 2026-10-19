#define main group_reader_fixture_main
#include "test_group_reader.cpp"
#undef main
#include "scenery_stroke_capture.h"
#include "scenery_stroke_calls.h"
namespace stroke=mt2mp::game::scenery_stroke_capture;
namespace action=mt2mp::action_session;
namespace {
constexpr uintptr_t receiver=0xb0000,options=0xc0000,pattern=0xd0000,generated_array=0xe0000,transform_data=0xf0000;
f::Memory setup(){f::Memory memory;
    memory.put(receiver,f::image+0x17396b0);memory.put(receiver+0xc50,uintptr_t(0));memory.put(receiver+0xb60,f::region);
    memory.put(f::region,f::image+0x1787930);memory.put(f::region+0x70,f::root);memory.put(f::region+0x834,int32_t(4));
    memory.put(receiver+0x174,int32_t(2));memory.put(receiver+0x178,int32_t(3));memory.put(receiver+0x17c,int32_t(5));
    memory.put(receiver+0xb68,options);memory.put(options,f::image+0x1715390);memory.put(options+0x60,uint8_t(1));
    memory.put(receiver+0xbc0,pattern);memory.put(pattern,f::image+0x16fd310);memory.put(f::image+0x16fd310+0x10,f::image+0x18db60);
    return memory;
}
bool catalog(int32_t category,int32_t variant,std::string& type,std::string& file,std::string& error){
    if(category!=3||(variant!=5&&variant!=-1&&variant!=-2)){error="catalog received a foreign native selection";return false;}
    type="Cactus";file=variant==-1?"":variant==-2?"negative-hash":"cactus05";error.clear();return true;
}
bool region(int32_t id,uintptr_t pointer,uintptr_t root,std::string& error){
    if(id==4&&pointer==f::region&&root==f::root)return true;error="current Region/root binding differs";return false;
}
struct Generated {
    unsigned calls=0,reads=0,destructors=0;bool make_failure=false,partial=false,read_failure=false,read_throw=false,cleanup_failure=false;
    bool generate(uintptr_t owner,uintptr_t shape,uintptr_t& out,bool& made,std::string& error){
        ++calls;f::need(owner==pattern&&shape==123,"original generator owner/shape differs");out=generated_array;made=!make_failure||partial;
        if(make_failure){error="first generator cause";return false;}return true;
    }
    bool read(uintptr_t p,size_t budget,std::vector<action::SceneryStrokePose>& out,std::string& error){
        ++reads;f::need(p==generated_array&&budget==20,"generated generated_array/budget differs");
        if(read_throw)throw std::runtime_error("read threw");
        if(read_failure){error="first generated read cause";return false;}out={f::pose()};return true;
    }
    bool destroy(uintptr_t p,std::string& error){++destructors;f::need(p==generated_array,"wrong generated owner destroyed");
        if(cleanup_failure){error="cleanup cause";return false;}return true;
    }
};
}
int main()try{
    std::string error;auto memory=setup();stroke::Local local;action::SceneryStrokeIntent intent;
    constexpr uintptr_t desired_mode=0x120000;bool bulk=false;
    memory.put(options+0x88,desired_mode);memory.put(options+0x90,size_t(6));memory.put(desired_mode,std::array<char,6>{'s','i','n','g','l','e'});
    f::need(stroke::bulk(memory,f::image,receiver,false,bulk,error)&&bulk,"current bulk cursor could not return to Single");
    memory.put(receiver+0x174,int32_t(0));f::need(stroke::bulk(memory,f::image,receiver,false,bulk,error)&&!bulk,"normal Single was captured as bulk");
    memory.put(options+0x90,size_t(5));memory.put(desired_mode,std::array<char,5>{'p','a','i','n','t'});
    f::need(stroke::bulk(memory,f::image,receiver,false,bulk,error)&&bulk,"Single-to-Paint native transition was blocked");
    memory.put(desired_mode,std::array<char,5>{'e','r','a','s','e'});f::need(stroke::bulk(memory,f::image,receiver,false,bulk,error)&&bulk,error);
    memory.put(desired_mode,std::array<char,5>{'o','t','h','e','r'});f::need(!stroke::bulk(memory,f::image,receiver,false,bulk,error),"unknown native mode admitted");
    memory=setup();
    auto read=[&](){return stroke::read(memory,f::image,receiver,action::SceneryStrokeKind::Paint,catalog,region,local,intent,error);};
    f::need(read(),error);f::need(local.pattern==pattern&&intent.replace&&intent.region==4&&intent.mode==2&&intent.type=="Cactus"&&intent.variant=="cactus05",
        "complete native brush input differs");
    memory.put(receiver+0x17c,int32_t(-1));f::need(read()&&intent.random_variant&&intent.variant.empty(),error);
    for(const auto [ap,method]:std::array<std::pair<uintptr_t,uintptr_t>,2>{{{0x1714af0,0x18e3f0},{0x1701990,0x18ecf0}}}){
        memory=setup();memory.put(pattern,f::image+ap);memory.put(f::image+ap+0x10,f::image+method);f::need(read(),error);}
    memory=setup();memory.put(receiver+0x174,int32_t(1));memory.bytes.erase(receiver+0xbc0);f::need(read()&&!local.pattern,"erase-mode capture required an unused generator");
    auto bad=[&](auto change){memory=setup();change(memory);f::need(!read(),"invalid original brush input was accepted");};
    bad([](auto& m){m.put(receiver+0x174,int32_t(0));});
    bad([](auto& m){m.put(receiver+0xc50,uintptr_t(12));});
    bad([](auto& m){m.put(receiver,f::image+0x17396b1);});
    bad([](auto& m){m.put(f::region+0x70,f::root+8);});
    bad([](auto& m){m.put(options+0x60,uint8_t(2));});
    bad([](auto& m){m.put(pattern,f::image+0x16fd311);});
    bad([](auto& m){m.put(f::image+0x16fd310+0x10,f::image+0x18e3f0);});
    memory=setup();memory.put(receiver+0x17c,int32_t(-2));f::need(read()&&!intent.random_variant&&intent.variant=="negative-hash","signed native variant hash was rejected");
    memory=setup();memory.put(receiver,f::image+0x173fc60);memory.put(receiver+0x5c0,uintptr_t(0));memory.put(receiver+0x588,f::region);
    f::need(stroke::read(memory,f::image,receiver,action::SceneryStrokeKind::Erase,catalog,region,local,intent,error)&&local.mode==1&&intent.type.empty(),error);
    memory=setup();f::need(read(),error);std::vector<action::SceneryStrokePose> dots;
    Generated generator;f::need(stroke::generated(generator,local,123,20,dots,error)&&dots.size()==1&&generator.destructors==1,error);
    generator={};generator.make_failure=true;f::need(!stroke::generated(generator,local,123,20,dots,error)&&!generator.destructors&&error=="first generator cause",error);
    generator={};generator.make_failure=generator.partial=generator.cleanup_failure=true;
    f::need(!stroke::generated(generator,local,123,20,dots,error)&&generator.destructors==1&&error=="first generator cause; generated transform cleanup: cleanup cause",error);
    generator={};generator.read_failure=generator.cleanup_failure=true;
    f::need(!stroke::generated(generator,local,123,20,dots,error)&&generator.destructors==1&&dots.empty()&&error.starts_with("first generated read cause"),error);
    generator={};generator.read_throw=true;
    f::need(!stroke::generated(generator,local,123,20,dots,error)&&generator.destructors==1&&dots.empty()&&error.ends_with("read threw"),error);
    memory.put(generated_array,f::image+0x1746400);memory.put(generated_array+8,transform_data);memory.put(generated_array+16,int32_t(1));memory.put(generated_array+20,int32_t(2));memory.put(transform_data,f::pose());
    f::need(stroke::dots(memory,f::image,generated_array,20,dots,error)&&dots==std::vector{f::pose()},error);
    memory.put(transform_data+12,uint32_t(0x7fc00000));f::need(!stroke::dots(memory,f::image,generated_array,20,dots,error),"nonfinite generated orientation accepted");
    memory.put(transform_data,f::pose());memory.put(transform_data+28,uint32_t(0));f::need(!stroke::dots(memory,f::image,generated_array,20,dots,error),"zero generated scale accepted");
    memory.put(transform_data,f::pose());memory.put(generated_array+16,int32_t(21));f::need(!stroke::dots(memory,f::image,generated_array,20,dots,error),"generated packet budget ignored");
    memory=setup();f::need(read(),error);const std::array<uint32_t,6> bounds{0,0,0,0x3f800000,0x3f800000,0x3f800000};memory.put(receiver+0x894,bounds);
    f::need(stroke::finish(memory,local,intent,error)&&intent.bounds==bounds,error);
    memory.put(receiver+0x894,uint32_t(0x7fc00000));f::need(!stroke::finish(memory,local,intent,error),"nonfinite completed stroke bounds accepted");
    std::cout<<f::checks<<" complete native Scenery stroke input/generator lifetime cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
