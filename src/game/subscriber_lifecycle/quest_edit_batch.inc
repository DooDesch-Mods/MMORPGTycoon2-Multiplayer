struct QuestEditBatch {
    Work world;
    local::SaveScopeReceipt scope;
    bool claimed=false,failed=false,may_leave=true;
    std::string failure;
    std::unique_ptr<QuestEditBatch> retained_next;
};
namespace {
std::unique_ptr<QuestEditBatch> retained_quest_batches;
struct QuestBatchStep {
    QuestEditBatch* batch;
    std::string& error;
    bool success=false;
    ~QuestBatchStep(){
        if(batch&&!success){batch->failed=true;if(batch->failure.empty())batch->failure=error.empty()?"Quest batch helper rejected without cause":error;}
    }
};
bool quest_batch_claim(QuestEditBatch& batch,std::string& e){
    if(batch.failed)return fail(e,"Quest batch prior helper failed; no replay: "+batch.failure);
    if(batch.claimed)return true;
    if(busy.test_and_set())return fail(e,"Quest batch shared Subscriber append scope busy");
    batch.claimed=true;return true;
}
bool quest_batch_world(QuestEditBatch* batch,Work& world,std::string& e){
    if(!batch)return read_world(world,false,world.original,e);
    const auto* expected=batch->world.frame;
    const auto* actual=world.frame;
    if(!expected||!actual||world.origin!=batch->world.origin||world.local_sha!=batch->world.local_sha||
       actual->context!=expected->context||actual->binding!=expected->binding||actual->common_epoch!=expected->common_epoch)
        return fail(e,"Quest batch helper frame/origin differs");
    if(!read_world(world,batch->scope.enter_returned,world.original,e))return false;
    if(batch->scope.enter_returned&&!stage::same_client_owner(batch->world.original,world.original))
        return fail(e,"Quest batch held Client owner changed");
    return true;
}
bool quest_batch_callback(local::SaveScopeBody fn,void* user,std::string& e){
    try{return fn(user,e);}
    catch(const std::exception& ex){e=std::string("save scope callback exception: ")+ex.what();}
    catch(...){e="save scope callback unknown C++ exception";}
    return false;
}
bool quest_batch_run(QuestEditBatch& batch,Work& world,local::SaveScopeBody body,
                     local::SaveScopeBody guard,void* user,local::SaveScopeReceipt& receipt,std::string& e){
    receipt={};
    if(!batch.scope.enter_returned){
        batch.world.original=world.original;
        const local::SaveScopeApi save{reinterpret_cast<local::SaveScopeCall>(image_base+0x4a03a0),reinterpret_cast<local::SaveScopeCall>(image_base+0x49f6f0)};
        if(!local::begin_save_scope(save,world.original.origin.mode,batch.scope,e)){
            receipt.unknown=batch.scope.unknown;return false;
        }
    }
    receipt.body_attempted=true;
    const bool ok=quest_batch_callback(body,user,receipt.body_cause);
    if(!ok&&receipt.body_cause.empty())receipt.body_cause="save scope body rejected without cause";
    receipt.exit_guard_confirmed=quest_batch_callback(guard,user,receipt.cleanup_cause);
    if(!receipt.exit_guard_confirmed){
        receipt.unknown=true;batch.may_leave=false;
        if(receipt.cleanup_cause.empty())receipt.cleanup_cause="save scope exit owner guard rejected without cause";
    }
    e=receipt.body_cause;
    if(!receipt.cleanup_cause.empty()){if(!e.empty())e+="; cleanup: ";e+=receipt.cleanup_cause;}
    if(receipt.unknown)batch.scope.unknown=true;
    return ok&&receipt.exit_guard_confirmed;
}
}
bool quest_edits_held(const QuestEditBatch& batch)noexcept{
    return batch.scope.enter_returned&&!batch.scope.leave_attempted;
}
bool verify_quest_edits_held(QuestEditBatch& batch,std::string& e){
    if(!batch.claimed||!quest_edits_held(batch)||batch.scope.unknown||!batch.may_leave)
        return fail(e,"Quest phase does not own a healthy held SaveScope");
    stage::ClientWorld current;
    if(!read_world_roots(batch.world,true,current,e))return false;
    return stage::same_client_owner(batch.world.original,current)||fail(e,"Quest phase held Client owner changed");
}
bool with_quest_edit_row(QuestEditBatch& batch,
    const std::function<bool(QuestEditBatch&,std::string&)>& body,
    const std::function<bool(std::string&)>& exit_guard,std::string& e){
    if(batch.failed)return fail(e,"Quest batch prior helper failed; no replay: "+batch.failure);
    bool okay=false;
    try{okay=body(batch,e);}
    catch(const std::exception& ex){e=std::string("Quest edit batch C++: ")+ex.what();}
    catch(...){e="Quest edit batch unknown C++ exception";}
    if(batch.failed){okay=false;if(e.empty())e=batch.failure;}
    if(batch.scope.enter_returned){
        std::string cleanup;bool guarded=false;
        if(batch.may_leave){
            try{
                stage::ClientWorld held;
                guarded=read_world(batch.world,true,held,cleanup)&&
                    (stage::same_client_owner(batch.world.original,held)||fail(cleanup,"Quest batch owner changed before SaveScope leave"))&&exit_guard(cleanup);
            }catch(const std::exception& ex){cleanup=std::string("Quest batch exit C++: ")+ex.what();}
            catch(...){cleanup="Quest batch exit unknown C++ exception";}
        }else cleanup="Quest batch helper exit guard failed; original leave not replayed";
        if(!guarded){
            if(cleanup.empty())cleanup="save scope exit owner guard rejected without cause";
            if(!e.empty())e+="; cleanup: ";e+=cleanup;
            batch.may_leave=false;batch.scope.unknown=true;okay=false;
        }
    }
    if(!okay){batch.failed=true;if(batch.failure.empty())batch.failure=e.empty()?"Quest batch row rejected without cause":e;}
    return okay;
}
bool with_quest_edits(const std::string& origin,const FrameBinding& frame,
    const std::function<bool(QuestEditBatch&,std::string&)>& body,
    const std::function<bool(std::string&)>& exit_guard,local::SaveScopeReceipt& receipt,std::string& e){
    receipt={};e.clear();
    auto owner=std::make_unique<QuestEditBatch>();auto& batch=*owner;
    batch.world.origin=origin;batch.world.local_sha=frame.local_sha;batch.world.frame=&frame;
    bool ok=false;
    try{ok=body(batch,e);}
    catch(const std::exception& ex){e=std::string("Quest edit batch C++: ")+ex.what();}
    catch(...){e="Quest edit batch unknown C++ exception";}
    if(batch.failed){ok=false;if(e.empty())e=batch.failure;}
    if(batch.scope.enter_returned){
        batch.scope.body_attempted=true;batch.scope.body_cause=e;
        if(!ok&&batch.scope.body_cause.empty())batch.scope.body_cause="save scope body rejected without cause";
        bool guard_ok=false;
        if(batch.may_leave){
            try{
                stage::ClientWorld held;
                guard_ok=read_world(batch.world,true,held,batch.scope.cleanup_cause)&&
                    (stage::same_client_owner(batch.world.original,held)||fail(batch.scope.cleanup_cause,"Quest batch owner changed before SaveScope leave"))&&
                    exit_guard(batch.scope.cleanup_cause);
            }catch(const std::exception& ex){batch.scope.cleanup_cause=std::string("Quest batch exit C++: ")+ex.what();}
            catch(...){batch.scope.cleanup_cause="Quest batch exit unknown C++ exception";}
        }else batch.scope.cleanup_cause="Quest batch helper exit guard failed; original leave not replayed";
        const local::SaveScopeApi save{reinterpret_cast<local::SaveScopeCall>(image_base+0x4a03a0),reinterpret_cast<local::SaveScopeCall>(image_base+0x49f6f0)};
        ok=local::finish_save_scope(save,batch.world.original.origin.mode,ok,guard_ok,batch.scope,e);
        if(ok){
            try{
                stage::ClientWorld closed;
                if(!read_world(batch.world,false,closed,e)||!stage::same_client_owner(batch.world.original,closed)){
                    if(e.empty())e="Quest batch current owner changed after SaveScope";
                    batch.scope.unknown=true;ok=false;
                }
            }catch(const std::exception& ex){e=std::string("Quest batch closed read C++: ")+ex.what();batch.scope.unknown=true;ok=false;}
            catch(...){e="Quest batch closed read unknown C++ exception";batch.scope.unknown=true;ok=false;}
        }
        if(!ok)batch.scope.unknown=true;
    }
    if(batch.claimed)busy.clear();
    receipt=batch.scope;
    if(batch.scope.unknown){
        blocked_epoch=batch.world.original.origin.world_epoch;blocked_generation=batch.world.original.origin.generation;blocked_cause=e;
        batch.world.frame=nullptr;batch.retained_next=std::move(retained_quest_batches);retained_quest_batches=std::move(owner);
        return false;
    }
    return ok;
}
