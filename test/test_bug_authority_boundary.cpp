#include <optional>
#include <string>
#include <iostream>
#include <stdexcept>
#include <vector>
struct ApplyReceipt {bool attempted=false,confirmed=false,unknown=false;std::string cause;};
#include "bug_failure_latch.inc"
std::vector<int> calls;
int failure=0;
struct Construction{std::optional<int> work=1;};
struct Pool{std::optional<Construction> construction=Construction{};};
const int* observed_work=nullptr;
namespace bug_authority {
bool preflight_frame(int,int,int,int,std::string& e,const int* work){observed_work=work;calls.push_back(1);if(failure==1){e="original Bug owner";return false;}return true;}
bool adopt_frame(int,int,int,int,bool& attempted,bool& unknown,std::string& e){calls.push_back(3);attempted=failure!=5&&failure!=6&&failure!=7;if(failure==3||failure==5){unknown=failure==3;e="original Bug adopt";return false;}return true;}
}
namespace bug_authority {
bool finish_frame(int,int,int,int,bool& attempted,bool& unknown,std::string& e){calls.push_back(5);attempted=true;
    if(failure==6){unknown=true;e="original Bug finish";return false;}
    if(failure==7){e="original finish exception";throw std::runtime_error(e);}return true;}
}
namespace developer_staff::action_runtime {
bool preflight_frame(int,int,const Pool&,std::string& e){calls.push_back(2);if(failure==2){e="original Developer preflight";return false;}return true;}
bool adopt_frame(int,int,const Pool&,bool& attempted,bool& unknown,std::string& e){calls.push_back(4);attempted=true;if(failure==4){unknown=true;e="original Developer adopt";return false;}return true;}
}
struct Frame {std::optional<int> bugs=1;std::optional<Pool> developers=Pool{};int revision=7;};
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool boundary(const Frame& frame,bool& locked_,ApplyReceipt& receipt_,std::string& e,bool later_failure=false){
    int context_=1,binding_=2;bool bug_attempted=false,bug_completed=false,developer_attempted=false;
    BugFailureLatch actual{bug_attempted,bug_completed,locked_,receipt_,e};
#include "bug_adoption_boundary.inc"
    if(later_failure){e="original later Day failure";return false;}
    receipt_.attempted=bug_attempted||developer_attempted;receipt_.confirmed=true;bug_completed=true;return true;
}
void need(bool ok,const std::string& e){if(!ok)throw std::runtime_error(e);}
int main(){try{
    size_t checks=0;for(failure=0;failure<=6;++failure){calls.clear();bool locked=false;ApplyReceipt receipt;std::string e;
        const bool ok=boundary({},locked,receipt,e);need(ok==(failure==0),"boundary result");
        need(locked==(failure==3||failure==4||failure==6),"pre-effect rejected or partial Bug attempt not distinguished");
        if(failure)need(e.starts_with("original"),"original cause lost");
        if(failure==1)need(calls==std::vector<int>{1},"write after Bug preflight reject");
        if(failure==2)need(calls==std::vector<int>({1,2}),"write after Developer preflight reject");
        if(failure==0)need(calls==std::vector<int>({1,2,3,4,5})&&receipt.confirmed,"wrong native order");++checks;
    }
    failure=0;{bool locked=false;ApplyReceipt receipt;std::string e;need(!boundary({},locked,receipt,e,true)&&locked&&receipt.unknown&&receipt.attempted&&!receipt.confirmed&&receipt.cause=="original later Day failure","later failure falsely confirmed Bug effect");++checks;}
    {Frame legacy;legacy.bugs.reset();calls.clear();bool locked=false;ApplyReceipt receipt;std::string e;need(boundary(legacy,locked,receipt,e)&&calls==std::vector<int>({2,4}),"legacy frame fabricated Bug writes");++checks;}
    {failure=0;Frame frame;bool locked=false;ApplyReceipt receipt;std::string e;
        need(boundary(frame,locked,receipt,e)&&observed_work==&*frame.developers->construction->work,"exact Work pointer not forwarded");++checks;
        frame.developers->construction->work.reset();need(boundary(frame,locked,receipt,e)&&!observed_work,"absent Work fabricated pointer");++checks;}
    {failure=7;bool locked=false,caught=false;ApplyReceipt receipt;std::string e;
        try{boundary({},locked,receipt,e);}catch(const std::runtime_error& x){caught=std::string(x.what())=="original finish exception";}
        need(caught&&locked&&receipt.unknown&&receipt.attempted&&receipt.cause==e,"finish exception lost attempt or cause");++checks;}
    for(int phase=0;phase<5;++phase){bool attempted=phase!=0,completed=phase==1,locked=false;ApplyReceipt receipt;receipt.confirmed=true;std::string cause;
        if(phase==2)cause="original later tutorial failure";if(phase==3)receipt.cause="original receipt";
        {BugFailureLatch actual{attempted,completed,locked,receipt,cause};}
        need(locked==(phase>=2),"failure latch did not preserve attempt/completion");
        if(phase>=2)need(receipt.unknown&&receipt.attempted&&!receipt.confirmed&&!receipt.cause.empty(),"Unknown latch false success");
        if(phase==2)need(receipt.cause==cause,"upstream cause lost");if(phase==3)need(receipt.cause=="original receipt","receipt cause lost");++checks;
    }
    std::cout<<"PASS "<<checks<<" actual Bug Bridge boundary/latch cases\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
