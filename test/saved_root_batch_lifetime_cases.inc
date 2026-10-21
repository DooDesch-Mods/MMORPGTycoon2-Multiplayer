#include <memory>
#include "native_open_witness.h"
namespace saved_root_lifetime_fixture {
struct WorldState;struct LookupApi;
namespace opened=mt2mp::game::building_mixed_identity::opened_file;
inline unsigned constructed=0,alive=0,finished=0,last_id=0;
inline bool reject=false,throw_constructor=false;
#include "saved_root_batch_lifetime_type.inc"
struct RootSceneryReadBatch::State {
    unsigned id=0;
    State(){if(throw_constructor)throw std::runtime_error("original batch allocation failure");id=++constructed;++alive;}
    ~State(){--alive;}
    bool finish(std::string& error){++finished;last_id=id;if(reject){error="original batch final census failure";return false;}return true;}
};
#include "saved_root_batch_lifetime_body.inc"
}
void saved_root_batch_lifetime_cases(){
    namespace f=saved_root_lifetime_fixture;
    std::string error;
    {f::RootSceneryReadBatch outer;
        need(f::constructed==1&&f::alive==1,"actual batch creates one state for the synchronous traversal");
        {f::RootSceneryReadBatch inner;
            need(f::constructed==1&&f::alive==1&&inner.finish(error)&&f::last_id==1,"actual nested collector joins the current complete owner batch");}
        need(f::alive==1&&outer.finish(error)&&f::last_id==1,"actual child return preserves the outer batch and state");
        f::reject=true;need(!outer.finish(error)&&error=="original batch final census failure","actual publication wrapper retains native finalization failure");f::reject=false;
        bool caught=false;
        try{f::RootSceneryReadBatch inner;throw std::runtime_error("original readonly traversal exception");}
        catch(const std::runtime_error& cause){caught=std::string(cause.what())=="original readonly traversal exception";}
        need(caught&&outer.finish(error)&&f::last_id==1,"actual RAII restores parent on exception without publishing the failed child");
        f::throw_constructor=true;caught=false;
        // A joined child cannot allocate another state.
        {f::RootSceneryReadBatch inner;need(inner.finish(error)&&f::constructed==1,"nested batch does not allocate unrelated state");}
        f::throw_constructor=false;
    }
    need(f::alive==0,"actual outer exit releases every cached native witness");
    f::throw_constructor=true;bool caught=false;
    try{f::RootSceneryReadBatch failed;}
    catch(const std::runtime_error& cause){caught=std::string(cause.what())=="original batch allocation failure";}
    f::throw_constructor=false;
    need(caught&&f::alive==0,"failed batch construction leaves no TLS owner or retained data");
    {f::RootSceneryReadBatch next;need(f::constructed==2&&next.finish(error)&&f::last_id==2,"next traversal starts fresh after return and allocation failure");}
    need(f::alive==0,"no owner cache survives the next synchronous traversal");
}
