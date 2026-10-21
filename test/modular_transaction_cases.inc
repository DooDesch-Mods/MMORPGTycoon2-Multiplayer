#include "modular_transaction.h"
#include "modular_transition.h"
namespace {
struct ModularTestPort {
    std::vector<std::string> order;std::string fail_at;bool fail_cleanup=false,throw_edit=false,cancel_enabled=false,cancel_called=false;
    bool step(const char* name,std::string& e){order.emplace_back(name);if(fail_at==name){e=std::string("original ")+name+" cause";return false;}return true;}
    bool ready(const ModularIntent&,std::string& e){return step("ready",e);}bool purchase(const ModularIntent&,std::string& e){return step("purchase",e);}
    bool begin_edit(bool& opened,std::string& e){opened=true;return step("begin",e);}bool end_edit(std::string& e){if(fail_cleanup){order.emplace_back("end");e="EndEdit cleanup cause";return false;}return step("end",e);}
    bool make_graph(uintptr_t& graph,std::string& e){graph=10;return step("graph",e);}bool layout_inverse(uintptr_t,const ModularIntent&,std::string& e){return step("inverse",e);}
    bool can_edit(const ModularIntent&,std::string& e){return step("can",e);}bool edit(const ModularIntent&,std::string& e){if(throw_edit)throw std::runtime_error("edit exception cause");return step("edit",e);}
    bool release_point(const ModularIntent&,std::string& e){return step("release",e);}bool make_prefab(const ModularIntent&,uintptr_t& object,std::string& e){object=20;return step("prefab",e);}
    bool clear_scenery(const ModularIntent&,uintptr_t,std::string& e){return step("clearance",e);}bool cash_inverse(uintptr_t,uintptr_t,std::string& e){return step("cash",e);}bool destroy_inverse(uintptr_t,uintptr_t,std::string& e){return step("destroy",e);}
    bool commit(uintptr_t,uintptr_t,const ModularIntent&,std::string& e){return step("commit",e);}bool readback(uintptr_t,uintptr_t,const ModularIntent&,std::string& e){return step("readback",e);}
    bool cancel_no_effect(mt2mp::game::modular_transaction::Receipt&,std::string&){cancel_called=true;return cancel_enabled;}
    bool discard_rejected_graph(mt2mp::game::modular_transaction::Receipt& value,std::string&){if(cancel_enabled&&value.graph){order.emplace_back("discard");value.graph=0;}return true;}
};
unsigned modular_transaction_cases(){namespace transaction=mt2mp::game::modular_transaction;unsigned count=0;std::string error;
    for(uint8_t mode=0;mode<5;++mode){ModularTestPort port;transaction::Receipt receipt;ModularIntent input;input.mode=mode;need(transaction::execute(port,input,receipt,error)&&receipt.confirmed&&receipt.graph_committed&&receipt.edit_closed,error);++count;
        std::vector<std::string> expected=mode==0?std::vector<std::string>{"ready","purchase","begin","prefab","release","graph","clearance","cash","destroy","commit","end","readback"}:mode<3?std::vector<std::string>{"ready","begin","graph","inverse","can","release","edit","commit","end","readback"}:std::vector<std::string>{"ready","begin","graph","inverse","edit","release","commit","end","readback"};need(port.order==expected,"Modular original operation order differs");++count;
        const auto prior=port.order;need(!transaction::execute(port,input,receipt,error)&&port.order==prior,"Modular attempted transaction replayed");++count;
        for(const auto& failed:expected){port={};port.fail_at=failed;receipt={};need(!transaction::execute(port,input,receipt,error)&&error=="original "+failed+" cause",error);++count;
            if(failed=="ready")need(!receipt.attempted,"Pure admission failure marked mutation");else need(receipt.attempted,"Attempted original failure lost its uncertainty");++count;
            const auto ends=std::count(port.order.begin(),port.order.end(),"end");need(ends<=1,"EndEdit retried after uncertainty");++count;
        }
    }
    ModularTestPort port;transaction::Receipt receipt;ModularIntent input;input.mode=3;port.fail_at="edit";port.fail_cleanup=true;need(!transaction::execute(port,input,receipt,error)&&error=="original edit cause; cleanup: EndEdit cleanup cause"&&receipt.unknown,error);++count;
    port={};receipt={};port.throw_edit=true;need(!transaction::execute(port,input,receipt,error)&&error=="Modular transaction exception: edit exception cause"&&receipt.edit_closed,error);++count;
    port={};receipt={};input.mode=1;port.fail_at="can";port.cancel_enabled=true;need(!transaction::execute(port,input,receipt,error)&&port.cancel_called&&!receipt.unknown&&receipt.edit_closed&&error=="original can cause",error);++count;
    need(port.order==std::vector<std::string>{"ready","begin","graph","inverse","can","discard","end"},"Rejected native graph must be disposed before EndEdit");++count;
    const auto before_calls=port.order;need(!transaction::execute(port,input,receipt,error)&&port.order==before_calls,"Confirmed no-effect original was replayed");++count;
    namespace transition=mt2mp::game::modular_transition;auto original=object(old(30),DuplicateFamily::Building),changed=original;changed.record_sha=std::string(64,'e');auto untouched=object(old(31));std::map<std::string,DuplicateObject> before{{original.key,original},{untouched.key,untouched}},after{{changed.key,changed},{untouched.key,untouched}};ModularEffect effect;std::string hashed;
    auto hash=[&](const std::string& value,std::string& digest,std::string&){hashed=value;digest=std::string(64,'a');return true;};need(transition::difference(before,after,hash,effect,error)&&effect.changed==std::vector<std::array<DuplicateObject,2>>{{original,changed}}&&effect.removed.empty()&&effect.created.empty()&&hashed.find(untouched.key)!=std::string::npos&&hashed.find(original.key)==std::string::npos,error);++count;
    return count;
}
}
