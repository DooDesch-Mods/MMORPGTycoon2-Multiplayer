#include "wall_history_owner.h"
#include <stdexcept>
#include <iostream>
namespace wall_owner_fixture {
namespace h=mt2mp::game::wall_history_owner;
unsigned checks=0;
void need(bool value){++checks;if(!value)throw std::runtime_error("owner assertion "+std::to_string(checks));}
struct Native {
    std::map<uintptr_t,uintptr_t> owners{{10,20},{11,20}};
    unsigned sets=0,reads=0,verifications=0;
    bool pin=true,read_ok=true,set_ok=true,readback=true;
    bool verify(std::string& e){++verifications;if(!pin)e="original pin failure";return pin;}
    bool read(uintptr_t path,uintptr_t& out,std::string& e){++reads;if(!read_ok){e="original owner read failure";return false;}out=owners.at(path);return true;}
    bool set(uintptr_t path,uintptr_t root,std::string& e){++sets;if(!set_ok){e="original SetShard SEH";return false;}if(readback)owners.at(path)=root;return true;}
};
inline unsigned run(){checks=0;
    const std::map<std::string,uintptr_t> paths{{"source:0",10},{"source:1",11}};
    std::string error;h::Receipt receipt;Native native;
    need(h::capture(native,20,paths,receipt,error)&&receipt.root==20&&receipt.keys.size()==2&&native.sets==0);
    native.owners[10]=native.owners[11]=0;
    need(h::restore(native,20,paths,receipt,error)&&native.sets==2&&native.owners[10]==20&&native.owners[11]==20);
    need(h::restore(native,20,paths,receipt,error)&&native.sets==2);
    for(int mode=0;mode<8;++mode){Native n;h::Receipt out;auto p=paths;uintptr_t root=20;
        if(mode==0)root=0;if(mode==1)p.clear();if(mode==2)n.owners[11]=21;
        if(mode==3)n.pin=false;if(mode==4)n.read_ok=false;if(mode==5)p["source:0"]=0;if(mode==6)n.owners[11]=0;if(mode==7)p["source:1"]=10;
        need(!h::capture(n,root,p,out,error)&&!n.sets&&out.keys.empty());}
    for(int mode=0;mode<9;++mode){Native n;n.owners[10]=0;auto p=paths;auto wanted=receipt;uintptr_t root=20;
        if(mode==0)root=21;if(mode==1)wanted.keys.clear();if(mode==2)p.erase("source:1");
        if(mode==3){p.erase("source:1");p["foreign"]=11;}if(mode==4)p["source:1"]=10;
        if(mode==5)n.owners[11]=21;if(mode==6)n.pin=false;if(mode==7)n.read_ok=false;if(mode==8)p["source:1"]=0;
        need(!h::restore(n,root,p,wanted,error)&&!n.sets&&n.owners[10]==0);}
    native={};native.owners[10]=0;native.set_ok=false;
    need(!h::restore(native,20,paths,receipt,error)&&native.sets==1&&error=="original SetShard SEH");
    native={};native.owners[10]=0;native.readback=false;
    need(!h::restore(native,20,paths,receipt,error)&&native.sets==1&&error.find("exact owner readback")!=std::string::npos);
    return checks;
}
}
