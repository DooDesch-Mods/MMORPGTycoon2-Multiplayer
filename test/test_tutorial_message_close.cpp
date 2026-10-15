#include "../src/game/welcome_close.cpp"
#include <iostream>
#include <stdexcept>
#include <vector>

using NativeWorld=mt2mp::game::building_mixed_identity::opened_file::CurrentWorld;
namespace {
NativeWorld current_world;
unsigned passed=0;
void require(bool value,const char* cause){if(!value)throw std::runtime_error(cause);++passed;}
template<class T>void put(uintptr_t base,size_t offset,const T& value){std::memcpy(reinterpret_cast<void*>(base+offset),&value,sizeof(value));}
struct Fixture {
    std::vector<unsigned char> image=std::vector<unsigned char>(0x1800000),state=std::vector<unsigned char>(0x200),log=std::vector<unsigned char>(0x80),instance=std::vector<unsigned char>(0x80),sequence=std::vector<unsigned char>(0x80),step=std::vector<unsigned char>(0x1b0),window=std::vector<unsigned char>(0x500),window_type=std::vector<unsigned char>(0x200);
    std::array<uintptr_t,107> steps{};
    uintptr_t active=0;
    static uintptr_t ptr(const std::vector<unsigned char>& value){return reinterpret_cast<uintptr_t>(value.data());}
    mt2mp::game::tutorial_welcome::Request request{"test-token","fixture",1,2,103};
    Fixture(){
        const auto base=ptr(image);active=ptr(instance);
        put(ptr(state),0x1e8,ptr(log));put(base,0x13df330,ptr(log));
        put(ptr(log),0x20,reinterpret_cast<uintptr_t>(&active));put<int32_t>(ptr(log),0x28,1);
        put(ptr(instance),0,base+0x173ce70);put(ptr(instance),0x18,reinterpret_cast<uintptr_t>("initial_tutorial"));put<uint64_t>(ptr(instance),0x20,16);put(ptr(instance),0x58,ptr(sequence));
        put(ptr(sequence),0,base+0x16dbd80);put(ptr(sequence),0x20,reinterpret_cast<uintptr_t>(steps.data()));put<int32_t>(ptr(sequence),0x28,107);put(ptr(sequence),0x48,ptr(step));
        put(ptr(step),0,base+0x16fda00);put<uint64_t>(ptr(step),0x28,7);put(ptr(step),0x1a8,ptr(window));
        put(ptr(window),0,ptr(window_type));put(ptr(window_type),0x1d0,base+0x81ec20);put<uint8_t>(ptr(window),0x45f,1);
        current_world={};current_world.state=state.data();current_world.uuid="fixture";current_world.world_epoch=1;current_world.generation=2;
        select(103);
    }
    void select(int32_t index){request.index=index;steps.fill(0);steps[size_t(index)]=ptr(step);put<int32_t>(ptr(sequence),0x50,index+1);put(ptr(step),0x20,reinterpret_cast<uintptr_t>(index==4?"welcome":"allDone"));}
    bool pin(std::string& error){mt2mp::game::tutorial_welcome::Pin value;error.clear();return mt2mp::game::tutorial_welcome::pin(request,ptr(image),value,error);}
};
}
namespace mt2mp::game::native_open_runtime {
bool read_current_world(NativeWorld& out,std::string& error){out=current_world;error.clear();return true;}
}
namespace mt2mp::game::tutorial_observer {
bool read(nlohmann::json&,std::string& error){error="observer is not part of the pin fixture";return false;}
}
namespace mt2mp::game::hud_time_runtime {
bool verify_window_show_hook(uintptr_t,std::string& error){error="no installed hook in the pin fixture";return false;}
}
namespace mt2mp::game::native {
bool read_resolver_receipt(uintptr_t&,uint32_t&,std::string&,std::string& error){error="resolver is outside the message pin fixture";return false;}
bool read_simulation_state(SimulationState&,std::string& error){error="simulation is outside the message pin fixture";return false;}
}
int main(){try{
    Fixture f;std::string error;
    require(f.pin(error),"actual allDone103 pin rejected");
    f.select(4);require(f.pin(error),"actual welcome4 pin rejected");f.select(103);
    f.request.index=102;require(!f.pin(error)&&!error.empty(),"unsupported message index accepted");f.request.index=103;
    put<int32_t>(Fixture::ptr(f.sequence),0x50,103);require(!f.pin(error),"off-by-one current index accepted");put<int32_t>(Fixture::ptr(f.sequence),0x50,104);
    put<int32_t>(Fixture::ptr(f.sequence),0x28,106);require(!f.pin(error),"truncated initial sequence accepted");put<int32_t>(Fixture::ptr(f.sequence),0x28,107);
    put<uint8_t>(Fixture::ptr(f.instance),0x38,1);require(!f.pin(error),"retired instance accepted");put<uint8_t>(Fixture::ptr(f.instance),0x38,0);
    put<uint8_t>(Fixture::ptr(f.sequence),0x54,1);require(!f.pin(error),"completed sequence accepted");put<uint8_t>(Fixture::ptr(f.sequence),0x54,0);
    put(Fixture::ptr(f.sequence),0,uintptr_t{0});require(!f.pin(error),"foreign sequence type accepted");put(Fixture::ptr(f.sequence),0,Fixture::ptr(f.image)+0x16dbd80);
    f.steps[103]=0;require(!f.pin(error),"unowned current step accepted");f.steps[103]=Fixture::ptr(f.step);
    put(Fixture::ptr(f.step),0x20,reinterpret_cast<uintptr_t>("welcome"));require(!f.pin(error),"wrong named message accepted");put(Fixture::ptr(f.step),0x20,reinterpret_cast<uintptr_t>("allDone"));
    put<uint8_t>(Fixture::ptr(f.step),0x18,1);require(!f.pin(error),"completed message accepted");put<uint8_t>(Fixture::ptr(f.step),0x18,0);
    put<uint8_t>(Fixture::ptr(f.window),0x45f,0);require(!f.pin(error),"hidden message accepted");put<uint8_t>(Fixture::ptr(f.window),0x45f,1);
    put<uint8_t>(Fixture::ptr(f.window),0x377,1);require(!f.pin(error),"locked message accepted");put<uint8_t>(Fixture::ptr(f.window),0x377,0);
    current_world.world_epoch=9;require(!f.pin(error),"foreign world origin accepted");current_world.world_epoch=1;
    require(f.pin(error),"restored original owner was not accepted");
    nlohmann::json receipt;f.request.index=99;
    require(!mt2mp::game::tutorial_welcome::close(f.request,receipt,error)&&receipt.at("attempted")==false&&receipt.at("unknown")==false,"unsupported close attempted a native effect");
    std::cout<<passed<<" actual tutorial message pin checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
