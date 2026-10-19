bool LivePort::bind(const a::Context& c,const a::Binding& b,uint64_t epoch,std::string& e){
    auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||locked_||!epoch||cycle_runtime.original||cycle_runtime.frame_original)return fail(e,"Release Frame bind requires current idle original owner");
    if(bound_&&(context_.session!=c.session||context_.role!=c.role||context_.local_peer!=c.local_peer||context_.peer_mask!=c.peer_mask))return fail(e,"Release Frame immutable Session changed");
    uintptr_t manager=0,release=0,celebration=0,mode=0;int32_t native_phase=0;
    if(!cycle_roots(*r,c,b,manager,release,celebration,mode,native_phase,e)||!code(*r,e))return false;
    context_=c;binding_=b;epoch_=epoch;bound_=true;suspended_=false;last_=nullptr;cause_.clear();
    cycle_runtime.context=c;cycle_runtime.binding=b;cycle_runtime.pending.reset();cycle_runtime.finalize_id.clear();cycle_runtime.local_finished=false;
    // A loaded active original celebration has no historical Place/action
    // receipt. Its shared snapshot UUID names this already existing cycle.
    cycle_runtime.phase=c.role==a::Role::Host?(native_phase==1?cycle::Phase::Announcement:native_phase==2?cycle::Phase::Playing:cycle::Phase::Idle):cycle::Phase::Idle;
    cycle_runtime.cycle=cycle_runtime.phase==cycle::Phase::Idle?std::string{}:c.snapshot_uuid;
    e.clear();return true;
}
bool LivePort::suspend(std::string& e){if(locked_)return fail(e,cause_);if(cycle_runtime.original||cycle_runtime.frame_original)return fail(e,"Release Frame suspend waits for original callback");suspended_=true;e.clear();return true;}
bool LivePort::capture(const a::Context& c,const a::Binding& b,cycle::Publication& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);if(!r||!bound_||suspended_||locked_||c!=context_||b!=binding_||c.role!=a::Role::Host)return fail(e,"Release Frame capture exact saved Host Context/Binding unavailable");
    cycle::Proof proof;a::Ledger money;return cycle_sample(*r,c,b,out,proof,money,e);
}
bool LivePort::apply(const a::Context& c,const a::Binding& b,const live::Frame& frame,live::ReleaseReceipt& receipt,std::string& e){
    receipt={};auto* r=published.load(std::memory_order_acquire);
    if(!r||!bound_||suspended_||locked_||c!=context_||b!=binding_||c.role!=a::Role::Client||frame.session!=c.session||frame.snapshot!=c.snapshot_uuid||frame.epoch!=epoch_||!frame.release)return fail(e,"Release Frame Apply exact saved Client Context/Binding/publication unavailable");
    release_graph_native::ApplyReceipt manager,prepared;bool attempted=false;
    const auto retain=[&](bool confirmed){last_={{"revision",frame.revision},{"for_action",frame.for_action},{"attempted",attempted},{"confirmed",confirmed},{"unknown",!confirmed&&attempted},{"cause",e},{"host",cycle::encode(*frame.release)},
        {"manager_load_attempted",manager.load_attempted},{"manager_load_returned",manager.load_returned},{"manager_readback_confirmed",manager.readback_confirmed},{"current_pointer_confirmed",manager.current.confirmed},{"old_release_retired",manager.old_release_retired},
        {"prepared_load_attempted",prepared.load_attempted},{"prepared_load_returned",prepared.load_returned},{"prepared_storage_attempted",prepared.prepared_storage.attempted},{"prepared_storage_stored",prepared.prepared_storage.stored},{"prepared_readback_confirmed",prepared.readback_confirmed}};};
    if(!cycle_adopt(*r,c,b,*frame.release,true,attempted,manager,prepared,e)){
        receipt.attempted=attempted;receipt.unknown=attempted;receipt.cause=e;locked_=attempted;cause_=e;retain(false);return false;}
    auto actual=std::make_unique<cycle::Publication>();cycle::Proof proof;a::Ledger money;
    if(!cycle_sample(*r,c,b,*actual,proof,money,e)||*actual!=*frame.release||money!=live::ledger(frame.values)){
        if(e.empty())e="Release full actual value/Ledger differs before Frame ACK";receipt.attempted=attempted;receipt.unknown=attempted;receipt.cause=e;locked_=attempted;cause_=e;retain(false);return false;}
    receipt.attempted=attempted;receipt.confirmed=true;retain(true);last_["actual"]=cycle::encode(*actual);++cycle_runtime.frame_applies;e.clear();return true;
}
nlohmann::json LivePort::status()const{return {{"bound",bound_},{"suspended",suspended_},{"locked",locked_},{"cause",cause_},{"last",last_}};}
