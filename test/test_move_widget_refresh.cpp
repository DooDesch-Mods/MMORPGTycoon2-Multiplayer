#include "move_widget_refresh.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
namespace widgets=mt2mp::game::move_widget_refresh;
unsigned checks=0;
void require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
struct Fixture {
    static constexpr uintptr_t image=0x140000000,target=0x10000,data=0x20000;
    std::unordered_map<uintptr_t,unsigned char> bytes;
    widgets::Receipt receipt;
    unsigned calls=0;
    std::string error;
    enum class Effect {Refresh,StaleContext,StaleModel,Dirty,Member,Local,Color,Visibility,NativeFault,GuardFail,ReadFault};
    Effect effect=Effect::Refresh;
    uintptr_t unreadable=0;
    template<class T> void put(uintptr_t address,const T& value){
        const auto* raw=reinterpret_cast<const unsigned char*>(&value);
        for(size_t i=0;i<sizeof value;++i)bytes[address+i]=raw[i];
    }
    bool read(uintptr_t address,void* output,size_t count,std::string& cause){
        for(size_t i=0;i<count;++i){
            if(address+i==unreadable||!bytes.contains(address+i)){cause="fixture native read failed";return false;}
            static_cast<unsigned char*>(output)[i]=bytes[address+i];
        }
        return true;
    }
    static widgets::Matrix matrix(float x){
        return {1,0,0,0,0,1,0,0,0,0,1,0,x,25,7428,1};
    }
    Fixture(){
        put(target+0x370,data);put(target+0x378,int32_t(3));
        put(target+0x128,uint8_t(1));put(target+0x12c,matrix(3820));
        for(size_t i=0;i<3;++i){
            const uintptr_t widget=0x30000+i*0x1000,model=0x50000+i*0x1000;
            put(data+i*8,widget);put(widget,image+0x1712b20);put(widget+8,model);
            put(widget+0x10,matrix(3820));
            put(widget+0x50,std::array<float,3>{0,float(i),-2});
            put(widget+0x5c,std::array<float,4>{0,0,0,1});
            put(widget+0x6c,std::array<float,4>{0,1,0,1});
            put(widget+0x80,uint8_t(1));put(widget+0x81,uint8_t(0));
            auto old=matrix(3820);old[13]+=float(i);old[14]-=2;
            put(model,old);put(model+0x70,uint8_t(1));
        }
    }
    bool snapshot(widgets::Snapshot& out){
        return widgets::snapshot(image,target,out,[&](auto p,auto q,auto n,auto& e){return read(p,q,n,e);},error);
    }
    bool run(){
        return widgets::refresh(image,target,receipt,
            [&](auto p,auto q,auto n,auto& e){return read(p,q,n,e);},
            [&](widgets::Receipt& result,std::string& cause){
                ++calls;
                if(effect==Effect::GuardFail){cause="original body differs";return false;}
                result.attempted=true;
                if(effect==Effect::NativeFault){result.exception=0xc0000005;cause="native widget fault";return false;}
                result.returned=true;
                put(target+0x128,uint8_t(0));put(target+0x12c,matrix(3860));
                for(size_t i=0;i<3;++i){
                    const uintptr_t widget=0x30000+i*0x1000,model=0x50000+i*0x1000;
                    if(effect!=Effect::StaleContext)put(widget+0x10,matrix(3860));
                    auto now=matrix(3860);now[13]+=float(i);now[14]-=2;
                    if(effect!=Effect::StaleModel)put(model,now);
                }
                if(effect==Effect::Dirty)put(0x30081,uint8_t(1));
                if(effect==Effect::Member)put(data,uintptr_t(0x31000));
                if(effect==Effect::Local)put(0x30050,2.0f);
                if(effect==Effect::Color)put(0x3006c,1.0f);
                if(effect==Effect::Visibility)put(0x50070,uint8_t(0));
                if(effect==Effect::ReadFault)unreadable=target+0x370;
                return true;
            },error);
    }
};
int main()try{
    Fixture good;widgets::Snapshot before,after;
    require(good.snapshot(before)&&before.widgets.size()==3,"actual reader captures crosshair and dotted path");
    require(before.widgets[0].model_matrix[12]==3820,"fixture retains old remote marker position");
    require(good.run()&&good.calls==1,"actual helper invokes target leaf once");
    require(good.receipt.required&&good.receipt.attempted&&good.receipt.returned&&good.receipt.readback&&
            good.receipt.widget_count==3&&!good.receipt.exception&&good.receipt.error.empty(),"complete native effect receipt");
    require(good.snapshot(after)&&after.widgets[0].model_matrix[12]==3860&&after.widgets[2].model_matrix[13]==27,
            "crosshair and all dotted entrance models follow current Building");
    require(after.widgets[0].object==before.widgets[0].object&&after.widgets[0].model==before.widgets[0].model&&
            after.widgets[0].local==before.widgets[0].local&&after.widgets[0].color==before.widgets[0].color,
            "refresh preserves local widget identity and entrance properties");
    require(!good.run()&&good.calls==1,"native effect cannot replay");
    Fixture empty;empty.put(Fixture::target+0x378,int32_t(0));
    require(empty.run()&&!empty.receipt.required&&!empty.receipt.attempted&&empty.calls==0,"no existing widgets creates none");
    Fixture invalid;invalid.put(0x30000,Fixture::image+0x1712b28);
    require(!invalid.run()&&invalid.calls==0,"foreign widget class blocks native call");
    Fixture duplicate;duplicate.put(Fixture::data+8,uintptr_t(0x30000));
    require(!duplicate.run()&&duplicate.calls==0,"duplicate widget owner blocks native call");
    Fixture missing;missing.unreadable=0x30050;
    require(!missing.run()&&missing.calls==0&&missing.receipt.error=="fixture native read failed","preflight read cause retained");
    Fixture guard;guard.effect=Fixture::Effect::GuardFail;
    require(!guard.run()&&!guard.receipt.attempted&&guard.receipt.error=="original body differs","pin failure precedes native effect");
    Fixture native;native.effect=Fixture::Effect::NativeFault;
    require(!native.run()&&native.receipt.attempted&&!native.receipt.returned&&!native.receipt.readback&&
            native.receipt.exception==0xc0000005&&native.receipt.error=="native widget fault","native fault stays distinct");
    for(auto effect:{Fixture::Effect::StaleContext,Fixture::Effect::StaleModel,Fixture::Effect::Dirty,
                     Fixture::Effect::Member,Fixture::Effect::Local,Fixture::Effect::Color,
                     Fixture::Effect::Visibility,Fixture::Effect::ReadFault}){
        Fixture broken;broken.effect=effect;
        require(!broken.run()&&broken.receipt.returned&&!broken.receipt.readback&&!broken.receipt.error.empty(),
                "returned leaf alone cannot confirm changed ownership or stale presentation");
    }
    auto rotated=Fixture::matrix(100);
    rotated[0]=0;rotated[2]=-2;rotated[8]=3;rotated[10]=0;
    require(widgets::translation(rotated,{4,5,6},0)==118&&widgets::translation(rotated,{4,5,6},1)==30&&
            widgets::translation(rotated,{4,5,6},2)==7420,"readback retains rotated scaled entrance position");
    std::cout<<checks<<" actual Move entrance widget checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
