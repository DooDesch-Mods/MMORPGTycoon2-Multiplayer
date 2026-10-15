#include "zone_memory_fixture.h"
#include "monster_zone_selection.h"
namespace s=mt2mp::game::monster_zone_selection;
int main(){try{unsigned checks=0;
    s::Binding value{10,20,30,0x140000000,44},other{50,60,70,0x140000000,44};
    const s::Binding* slot=nullptr;uintptr_t brush=0;bool active=false;
    {s::Scope scope(slot,value);s::PaintScope paint(brush,active,value.brush);
        for(auto rva:{0x5f1842u,0x5f1956u,0x5f1989u}){require(s::matches(slot,brush,value.window,value.image+rva,44,active),"exact original caller rejected");++checks;}
        require(!s::matches(slot,brush,value.window,value.image+0x5f1843,44,active),"adjacent arbitrary caller accepted");++checks;
        require(!s::matches(slot,11,value.window,value.image+0x5f1842,44,active),"foreign brush accepted");++checks;
        require(!s::matches(slot,brush,21,value.image+0x5f1842,44,active),"foreign Window accepted");++checks;
        require(!s::matches(slot,brush,value.window,value.image+0x5f1842,45,active),"foreign thread accepted");++checks;
        require(!s::matches(slot,brush,value.window,value.image+0x5f1842,44,false),"outside paint accepted");++checks;
        try{s::Scope nested(slot,other);s::PaintScope nested_paint(brush,active,other.brush);throw std::runtime_error("original callback boundary");}
        catch(const std::runtime_error& e){require(std::string(e.what())=="original callback boundary","original exception lost");}
        require(slot==&value&&brush==10&&active,"nested failure did not restore previous scope");++checks;
    }
    require(!slot&&!brush&&!active,"normal exit did not restore empty scope");++checks;
    try{s::Scope scope(slot,value);s::PaintScope paint(brush,active,value.brush);throw std::runtime_error("partial native call");}
    catch(const std::runtime_error& e){require(std::string(e.what())=="partial native call","native cause lost");}
    require(!slot&&!brush&&!active,"exception exit retained private selection");++checks;
    auto fixture=[](){Memory m;const uintptr_t image=0x140000000,manager=0x40000,data=0x50000;
        m.space(manager,0x200);m.put(image+0x13df7a0,manager);m.put(manager,image+0x1732430);m.put(manager+0xd0,image+0x16eb860);
        m.put(manager+0xd8,data);m.put(manager+0xe0,int32_t(4));m.put(manager+0xe4,int32_t(4));
        for(int32_t i=0;i<4;++i){const uintptr_t character=0x60000+uintptr_t(i)*0x100;m.space(character,0x40);m.put(data+size_t(i)*8,character);
            m.put(character,image+0x1705990);m.put(character+0x2c,std::array<int32_t,2>{1,i});}return m;};
    {auto m=fixture();s::Catalogue out;std::string e;require(s::read(m,0x140000000,2,out,e),e);require(out.character==0x60200&&out.id==2,"wrong catalogue member");++checks;}
    const auto bad=[&](auto mutate,const char* cause){auto m=fixture();mutate(m);s::Catalogue out;std::string e;require(!s::read(m,0x140000000,2,out,e),cause);++checks;};
    bad([](Memory& m){m.put(0x40000,uintptr_t(1));},"foreign manager accepted");
    bad([](Memory& m){m.put(0x400d0,uintptr_t(1));},"foreign ArrayStore accepted");
    bad([](Memory& m){m.put(0x400e4,int32_t(3));},"capacity underflow accepted");
    bad([](Memory& m){m.put(0x50010,uintptr_t(0x60100));},"duplicate member accepted");
    bad([](Memory& m){m.put(0x6022c,std::array<int32_t,2>{2,2});},"foreign catalogue slot accepted");
    bad([](Memory& m){m.put(0x6022c,std::array<int32_t,2>{1,1});},"wrong array index accepted");
    bad([](Memory& m){m.put(0x60200,uintptr_t(1));},"wrong Character class accepted");
    std::cout<<checks<<" actual catalogue/scope checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
