#include "../src/game/wall_private_lifecycle.h"
#include <cstdio>
#include <vector>
using namespace mt2mp::game::wall_private_native;
struct Native {
    std::vector<int> calls;
    uint32_t prepare_error=0,purchase_error=0,finish_error=0,cleanup_error=0;
    bool allowed=true;
    uint32_t prepare(Receipt& receipt){calls.push_back(1);receipt.receiver=100;receipt.ghost=200;return prepare_error;}
    uint32_t purchase(bool& value){calls.push_back(2);value=allowed;return purchase_error;}
    uint32_t finish(){calls.push_back(3);return finish_error;}
    uint32_t cleanup(Receipt&){calls.push_back(4);return cleanup_error;}
};
int main(){
    int checks=0;
    auto check=[&](bool ok){++checks;if(!ok){std::printf("FAIL check %d\n",checks);return false;}return true;};
    {
        Native n;Receipt r;
        if(!check(execute(n,r)==0)||!check(n.calls==std::vector<int>{1,2,3,4})||
           !check(r.prepared&&r.purchase_attempted&&r.purchase_returned&&r.purchase_allowed)||
           !check(r.finish_attempted&&r.finish_returned&&r.cleanup_attempted&&r.cleanup_returned)||
           !check(r.receiver==100&&r.ghost==200))return 1;
    }
    {
        Native n;n.allowed=false;Receipt r;
        if(!check(execute(n,r)==purchase_denied)||!check(n.calls==std::vector<int>{1,2,4})||
           !check(r.prepared&&r.purchase_returned&&!r.purchase_allowed)||
           !check(!r.finish_attempted&&!r.finish_returned&&r.cleanup_returned))return 1;
    }
    {
        Native n;n.prepare_error=0xc0000005;Receipt r;
        if(!check(execute(n,r)==n.prepare_error)||!check(n.calls==std::vector<int>{1})||
           !check(!r.prepared&&!r.purchase_attempted&&!r.cleanup_attempted)||
           !check(r.receiver==100&&r.ghost==200))return 1;
    }
    {
        Native n;n.purchase_error=0xc0000005;Receipt r;
        if(!check(execute(n,r)==n.purchase_error)||!check(n.calls==std::vector<int>{1,2})||
           !check(r.purchase_attempted&&!r.purchase_returned&&!r.finish_attempted&&!r.cleanup_attempted))return 1;
    }
    {
        Native n;n.finish_error=0xc0000005;Receipt r;
        if(!check(execute(n,r)==n.finish_error)||!check(n.calls==std::vector<int>{1,2,3})||
           !check(r.purchase_allowed&&r.finish_attempted&&!r.finish_returned&&!r.cleanup_attempted))return 1;
    }
    {
        Native n;n.cleanup_error=0xc0000005;Receipt r;
        if(!check(execute(n,r)==n.cleanup_error)||!check(r.finish_returned&&r.cleanup_attempted&&!r.cleanup_returned)||
           !check(r.cause==0&&r.cleanup_cause==n.cleanup_error))return 1;
    }
    {
        Native n;n.allowed=false;n.cleanup_error=0xc0000005;Receipt r;
        if(!check(execute(n,r)==purchase_denied)||!check(r.cause==purchase_denied&&r.cleanup_cause==n.cleanup_error)||
           !check(r.cleanup_attempted&&!r.cleanup_returned&&!r.finish_attempted))return 1;
    }
    std::printf("PASS %d Wall private lifecycle checks\n",checks);
}
