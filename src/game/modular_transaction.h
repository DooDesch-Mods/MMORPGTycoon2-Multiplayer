#pragma once
#include "typed_action/modular_action.h"
#include <string>
#include <exception>
namespace mt2mp::game::modular_transaction {
struct Receipt {
    bool attempted=false,unknown=false,confirmed=false,edit_opened=false,edit_end_attempted=false,edit_closed=false,graph_committed=false;
    uint32_t calls=0,returned=0;uintptr_t graph=0,created=0;
    std::string cause,cleanup_cause;
};
// Each port operation invokes one existing native owner and checks its returned
// effect. An attempted failure cannot become a second transaction.
template<class Port>bool execute(Port& port,const action_session::ModularIntent& input,Receipt& out,std::string& e){
    if(out.attempted||out.confirmed){e="Modular transaction already attempted; no replay";return false;}out={};
    const auto fail=[&](){out.cause=e.empty()?"Modular transaction failed without its original cause":e;e=out.cause;out.unknown=out.attempted;
        if constexpr(requires{port.discard_rejected_graph(out,e);}){std::string cleanup;if(!out.graph_committed&&!port.discard_rejected_graph(out,cleanup)){out.cleanup_cause=cleanup.empty()?"Modular rejected graph disposal did not confirm":cleanup;e+="; cleanup: "+out.cleanup_cause;}}
        if(out.edit_opened&&!out.edit_end_attempted){out.edit_end_attempted=true;std::string cleanup;if(port.end_edit(cleanup))out.edit_closed=true;else{if(cleanup.empty())cleanup="Modular EndEdit did not confirm";out.cleanup_cause+=(out.cleanup_cause.empty()?"":"; ")+cleanup;e+="; cleanup: "+cleanup;}}
        if constexpr(requires{port.cancel_no_effect(out,e);}){if(out.cleanup_cause.empty()&&!out.graph_committed){const auto primary=e;std::string cleanup;if(port.cancel_no_effect(out,cleanup))out.unknown=false;else if(!cleanup.empty()){out.cleanup_cause=cleanup;e=primary+"; cleanup: "+cleanup;}}}return false;};
    const auto call=[&](auto operation){out.attempted=true;++out.calls;if(!operation())return false;++out.returned;return true;};
    try {
    if(!port.ready(input,e))return fail();
    if(input.mode==0&&!call([&]{return port.purchase(input,e);}))return fail();
    if(!call([&]{return port.begin_edit(out.edit_opened,e);}))return fail();
    if(input.mode==0){
        if(!call([&]{return port.make_prefab(input,out.created,e);})||!out.created)return fail();
        if(!call([&]{return port.release_point(input,e);})||!call([&]{return port.make_graph(out.graph,e);})||!out.graph)return fail();
        if(!call([&]{return port.clear_scenery(input,out.graph,e);})||!call([&]{return port.cash_inverse(out.graph,out.created,e);})||
           !call([&]{return port.destroy_inverse(out.graph,out.created,e);}))return fail();
    }else{
        if(!call([&]{return port.make_graph(out.graph,e);})||!out.graph||!call([&]{return port.layout_inverse(out.graph,input,e);}))return fail();
        // The original entrance mutations precede their release point. Block
        // operations validate first and then add the release point before edit.
        if(input.mode>=3){if(!call([&]{return port.edit(input,e);})||!call([&]{return port.release_point(input,e);}))return fail();}
        else if(!port.can_edit(input,e)||!call([&]{return port.release_point(input,e);})||!call([&]{return port.edit(input,e);}))return fail();
    }
    if(!call([&]{return port.commit(out.graph,input.mode==0?out.created:uintptr_t{},input,e);}))return fail();out.graph_committed=true;
    if(out.edit_opened){out.edit_end_attempted=true;if(!call([&]{return port.end_edit(e);}))return fail();out.edit_closed=true;}
    if(!port.readback(out.graph,out.created,input,e))return fail();out.confirmed=true;e.clear();return true;
    }catch(const std::exception& error){e=std::string("Modular transaction exception: ")+error.what();return fail();}
}
}
