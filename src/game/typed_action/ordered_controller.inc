namespace tc=tick_commands;
struct Controller::Ordered {
    tc::Queue queue;
    OrderedExecute execute;
    OrderedAdvance advance;
    OrderedCaptureAdmission capture_admission;
    std::optional<Request> captured;
    uint64_t capture_deadline=0;
    std::deque<tc::Input> waiting;
    std::optional<tc::Input> local;
    uint64_t local_tick=0,time_sequence=0,snapshot_sequence=0,local_deadline=0,waiting_deadline=0;
    bool snapshot_hold=false;uint64_t snapshot_hold_started=0;OrderedSnapshotReceive snapshot_receive;OrderedPeriodicReceive periodic_receive;
    struct Boundary {uint64_t deadline=0;uint8_t mask=0;};
    std::map<uint64_t,Boundary> boundaries;
    std::vector<uint8_t> last_outgoing;
    Ordered(const tc::Config& config,OrderedValidate validate,OrderedExecute run,OrderedAdvance step)
        :queue(config,std::move(validate)),execute(std::move(run)),advance(std::move(step)),
         time_sequence(config.sequences[1][config.context.local_peer]),snapshot_sequence(config.sequences[2][config.context.local_peer]){}
};
bool Controller::ordered_mode()const noexcept{return bool(ordered_);}
uint64_t Controller::ordered_time_sequence()const noexcept{return ordered_?ordered_->time_sequence:0;}
tc::State Controller::ordered_state()const{return ordered_?ordered_->queue.state():tc::State{};}
std::vector<tc::CompletedAction> Controller::ordered_completed_actions()const{
    if(!ordered_||std::this_thread::get_id()!=thread_)return {};
    const auto& actions=ordered_->queue.completed_actions();return {actions.begin(),actions.end()};
}
bool Controller::start_ordered(const tc::Config& config,OrderedValidate validate,OrderedExecute execute,OrderedAdvance advance,uint64_t now,std::string& error,OrderedCaptureAdmission capture){
    if(status_.started||ordered_||thread_!=std::thread::id{})return fail("Ordered mode must be selected before controller start",error);
    if(!validate||!execute||!advance)return fail("Ordered controller callbacks incomplete",error);
    Context actual;
    if(!invoke([&]{return session_.current(actual,error);},error)||actual!=config.context)return fail("Ordered start immutable session differs: "+error,error);
    if(!invoke([&]{ordered_=std::make_unique<Ordered>(config,std::move(validate),std::move(execute),std::move(advance));return true;},error))return fail(error,error,true);
    ordered_->capture_admission=std::move(capture);
    if(!start(now,error))return false;
    if(context_!=config.context)return fail("Ordered session changed during start",error,true);
    local_sequence_=config.sequences[0][context_.local_peer];status_.host_revision=config.initial_order;status_.required_mask=context_.peer_mask;
    return true;
}
bool Controller::ordered_send(uint64_t target,const std::vector<uint8_t>& bytes,std::string& error){
    ordered_->last_outgoing=bytes;
    if(!invoke([&]{return tc::wire::send(transport_,session_,context_,target,bytes,error);},error))return fail("Ordered transport enqueue: "+error,error,true);
    return true;
}
void Controller::ordered_status(){
    auto& o=*ordered_;status_.host_revision=o.queue.state().completed_order;
    if(o.local&&o.local_tick){
        const auto it=o.boundaries.find(o.local_tick);
        status_.ack_mask=it==o.boundaries.end()?context_.peer_mask:it->second.mask;
        if(context_.role==Role::Client||it==o.boundaries.end()){
            o.local.reset();o.local_tick=0;o.local_deadline=0;status_.outcome=Outcome::Confirmed;++status_.confirmed;
        }
    }
    status_.pending=o.local.has_value()||o.captured.has_value();
    if(o.captured)status_.action_uuid=o.captured->action_uuid;
    if(o.local)if(const auto* request=std::get_if<Request>(&o.local->command))status_.action_uuid=request->action_uuid;
}
bool Controller::ordered_capture(Request request,uint64_t now,std::string& error){
    auto& o=*ordered_;
    if(std::this_thread::get_id()!=thread_||!status_.started||status_.locked||!o.queue.state().consuming)
        return fail("Ordered UI capture requires current consuming Controller",error);
    if(o.captured||o.local)return fail("Ordered UI input already retained; new capture was not accepted",error);
    if(!valid_request(request))return fail("Ordered UI capture payload invalid",error);
    if(now<now_||now>UINT64_MAX-timeout_)return fail("Ordered UI capture deadline invalid",error,true);
    // OriginalGo admission has already checked actual Session and native Binding.
    // Sequence assignment and transport remain outside the borrowed Go callback.
    o.captured=std::move(request);o.capture_deadline=now+timeout_;
    status_.pending=true;status_.outcome=Outcome::Pending;status_.ack_mask=0;status_.action_uuid=o.captured->action_uuid;
    error.clear();return true;
}
bool Controller::ordered_submit(tc::Input input,uint64_t now,std::string& error){
    if(!guard(error)||!update_time(now,error))return false;
    auto& o=*ordered_;
    if(o.local)return fail("Ordered input pending; new input was not accepted",error);
    const auto stream=input.command.index();auto& sequence=stream==2?o.snapshot_sequence:stream==1?o.time_sequence:local_sequence_;
    if(sequence==UINT64_MAX||now_>UINT64_MAX-timeout_)return fail("Ordered sequence or deadline exhausted",error,true);
    std::visit([&](auto& request){request.peer=context_.local_peer;request.sequence=sequence+1;},input.command);
    if(auto* request=std::get_if<Request>(&input.command)){
        request->host_revision=status_.host_revision;request->snapshot_uuid=context_.snapshot_uuid;
    }else if(auto* time_request=std::get_if<tc::TimeRequest>(&input.command))time_request->observed_revision=status_.host_revision;
    else std::get<tc::SnapshotRequest>(input.command).observed_revision=status_.host_revision;
    std::vector<uint8_t> bytes;
    if(!tc::wire::encode({context_.peer_mask,input},bytes,error))return fail(error,error);
    // Prove that this input can fit alone before accepting it into retained pending state.
    tc::Batch single{context_.session,1,0,context_.peer_mask,context_.snapshot_uuid,{{1,input}}};
    std::vector<uint8_t> bounded;
    if(!tc::wire::encode({context_.peer_mask,single},bounded,error))return fail(error,error);
    o.local=input;o.local_deadline=now_+timeout_;++sequence;++status_.attempted;
    status_.pending=true;status_.outcome=Outcome::Pending;status_.ack_mask=0;
    if(const auto* request=std::get_if<Request>(&input.command))status_.action_uuid=request->action_uuid;
    if(context_.role==Role::Host){
        if(o.waiting.empty())o.waiting_deadline=now_+timeout_;
        o.waiting.push_back(std::move(input));
    }
    else if(!ordered_send(context_.host_peer,bytes,error))return false;
    error.clear();return true;
}
bool Controller::submit_ordered_time(game::time_session::Intent intent,uint64_t now,std::string& error){
    if(!ordered_)return fail("Ordered time input requires ordered mode",error);
    return ordered_submit({context_.session,context_.snapshot_uuid,tc::TimeRequest{0,0,0,intent}},now,error);
}
bool Controller::submit_ordered_snapshot(const std::string& nonce,uint64_t now,std::string& error,bool periodic){
    if(!ordered_||context_.role!=Role::Host||ordered_->snapshot_hold)return fail("Snapshot preparation requires idle ordered Host",error);
    return ordered_submit({context_.session,context_.snapshot_uuid,tc::SnapshotRequest{0,0,0,nonce,periodic?tc::SnapshotPurpose::HostPeriodic:tc::SnapshotPurpose::Ordinary}},now,error);
}
bool Controller::bind_ordered_snapshot(OrderedSnapshotReceive receive,std::string& error){
    if(!ordered_||ordered_->snapshot_receive||!receive)return fail("Snapshot receiver requires fresh ordered Controller",error);
    ordered_->snapshot_receive=std::move(receive);return true;
}
bool Controller::bind_ordered_periodic(OrderedPeriodicReceive receive,std::string& error){
    if(!ordered_||ordered_->periodic_receive||!receive)return fail("Periodic receiver requires fresh ordered Controller",error);
    ordered_->periodic_receive=std::move(receive);return true;
}
bool Controller::send_ordered_periodic(const tc::wire::PeriodicRequest& value,std::string& error){
    if(!ordered_||!guard(error))return false;std::vector<uint8_t> bytes;
    if(value.session!=context_.session||value.snapshot_uuid!=context_.snapshot_uuid||
       !tc::wire::encode({context_.peer_mask,value},bytes,error))return fail(error.empty()?"Periodic request Session differs":error,error);
    return ordered_send(context_.host_peer,bytes,error);
}
void Controller::hold_ordered_snapshot(bool hold){
    if(!ordered_||ordered_->snapshot_hold==hold)return;auto& o=*ordered_;
    if(hold)o.snapshot_hold_started=now_;
    else {
        // Deadlines measure runnable transport/assignment time, excluding this agreed native hold.
        const auto elapsed=now_-o.snapshot_hold_started;
        const auto shift=[&](uint64_t& deadline){if(deadline){
            if(deadline>UINT64_MAX-elapsed)throw std::overflow_error("Snapshot held deadline exhausted");deadline+=elapsed;}};
        shift(o.local_deadline);shift(o.waiting_deadline);shift(o.capture_deadline);
        for(auto& [tick,boundary]:o.boundaries){(void)tick;shift(boundary.deadline);}
    }
    o.snapshot_hold=hold;
}
bool Controller::send_ordered_snapshot(uint64_t target,const tc::wire::SnapshotControl& value,std::string& error){
    if(!ordered_||!guard(error))return false;std::vector<uint8_t> bytes;
    if(value.session!=context_.session||value.snapshot_uuid!=context_.snapshot_uuid||
       !tc::wire::encode({context_.peer_mask,value},bytes,error))return fail(error.empty()?"Snapshot control Session differs":error,error);
    return ordered_send(target,bytes,error);
}
bool Controller::ordered_receive(uint64_t sender,const uint8_t* bytes,size_t size,std::string& error){
    auto& o=*ordered_;tc::wire::Packet packet;
    if(!tc::wire::decode(bytes,size,packet,error))return fail(error,error,true);
    if(const auto* value=std::get_if<tc::wire::PeriodicRequest>(&packet.value)){
        if(context_.role!=Role::Host||value->session!=context_.session||value->snapshot_uuid!=context_.snapshot_uuid||
           packet.peer_mask!=context_.peer_mask||sender!=value->peer||sender==context_.local_peer||!o.periodic_receive)
            return fail("Periodic request authenticated Session/peer differs",error,true);
        if(!o.periodic_receive(sender,*value,error))return fail(error,error,true);return true;
    }
    if(const auto* value=std::get_if<tc::wire::SnapshotControl>(&packet.value)){
        if(value->session!=context_.session||value->snapshot_uuid!=context_.snapshot_uuid||packet.peer_mask!=context_.peer_mask||
           sender<1||sender>4||!(context_.peer_mask&bit(sender))||sender==context_.local_peer||!o.snapshot_receive)
            return fail("Snapshot control authenticated Session/peer differs",error,true);
        if(!o.snapshot_receive(sender,*value,error))return fail(error,error,true);return true;
    }
    if(const auto* input=std::get_if<tc::Input>(&packet.value)){
        const auto owner=std::visit([](const auto& request){return request.peer;},input->command);
        if(context_.role!=Role::Host||input->session!=context_.session||input->snapshot_uuid!=context_.snapshot_uuid||packet.peer_mask!=context_.peer_mask||owner!=sender)
            return fail("Ordered input authenticated session or owner differs",error,true);
        // One retained input per peer bounds backpressure without discarding accepted input.
        for(const auto& pending:o.waiting)if(std::visit([](const auto& request){return request.peer;},pending.command)==sender){
            if(pending==*input){++status_.duplicates;error.clear();return true;}
            return fail("Ordered peer already has a retained unassigned input",error,true);
        }
        tc::Batch single{context_.session,1,0,context_.peer_mask,context_.snapshot_uuid,{{1,*input}}};
        std::vector<uint8_t> bounded;
        if(!tc::wire::encode({context_.peer_mask,single},bounded,error))return fail(error,error,true);
        if(now_>UINT64_MAX-timeout_)return fail("Ordered deadline exhausted",error,true);
        if(o.waiting.empty())o.waiting_deadline=now_+timeout_;
        o.waiting.push_back(*input);error.clear();return true;
    }
    tc::Assigned ignored;tc::Admission admission=tc::Admission::Rejected;
    if(!invoke([&]{admission=tc::wire::receive(o.queue,session_,context_,sender,bytes,size,ignored,error);return admission!=tc::Admission::Rejected;},error))return fail(error,error,true);
    if(admission==tc::Admission::Duplicate)++status_.duplicates;
    if(const auto* receipt=std::get_if<tc::Receipt>(&packet.value)){
        if(auto it=o.boundaries.find(receipt->tick);it!=o.boundaries.end()){
            it->second.mask|=bit(sender);if(it->second.mask==context_.peer_mask)o.boundaries.erase(it);
        }
    }
    ordered_status();return true;
}
bool Controller::ordered_seal(tc::Batch& batch,uint64_t now,std::string& error){
    if(!guard(error))return false;
    return ordered_seal_after_guard(batch,now,error);
}
bool Controller::ordered_seal_held(tc::Batch& batch,uint64_t now,std::string& error,const std::function<bool(std::string&)>& interruption,bool& interrupted){
    interrupted=false;
    const std::function<bool(std::string&)> classify=[&](std::string& cause){
        std::string proof;
        if(!ordered_||context_.role!=Role::Host||!engine_.current(binding_,context_.snapshot_uuid,proof)){
            cause+="; held seal native proof rejected: "+proof;return false;
        }
        return interruption&&interruption(cause);
    };
    if(!guard(error,&classify,&interrupted))return interrupted;
    return ordered_seal_after_guard(batch,now,error);
}
bool Controller::ordered_seal_after_guard(tc::Batch& batch,uint64_t now,std::string& error){
    if(!update_time(now,error))return false;
    if(!ordered_||context_.role!=Role::Host)return fail("Ordered seal requires ordered Host",error);
    auto& o=*ordered_;
    if(now_>UINT64_MAX-timeout_)return fail("Ordered deadline exhausted",error,true);
    if(o.queue.state().in_flight==tc::Queue::in_flight_limit)return fail("Ordered completion window full; pending input retained",error);
    while(!o.waiting.empty()){
        auto& input=o.waiting.front();tc::Assigned assigned;bool capacity_failed=false;
        tc::Admission admission=tc::Admission::Rejected;
        if(!invoke([&]{admission=o.queue.offer(std::visit([](const auto& r){return r.peer;},input.command),input,assigned,error,
            [&](const tc::Batch& candidate,std::string& cause){std::vector<uint8_t> bytes;const bool fits=tc::wire::encode({context_.peer_mask,candidate},bytes,cause);capacity_failed=!fits;return fits;});return true;},error))return fail(error,error,true);
        if(admission==tc::Admission::Rejected){
            if(capacity_failed&&o.queue.state().queued)break;
            return fail(error,error,true);
        }
        if(admission==tc::Admission::Duplicate)++status_.duplicates;
        const bool snapshot=std::holds_alternative<tc::SnapshotRequest>(input.command);
        o.waiting.pop_front();if(snapshot)break;
    }
    if(o.waiting.empty())o.waiting_deadline=0;
    std::vector<uint8_t> bytes;
    if(!invoke([&]{return tc::wire::seal(o.queue,batch,bytes,error);},error))return fail(error,error,o.queue.state().locked);
    o.boundaries.emplace(batch.tick,Ordered::Boundary{now_+timeout_,0});
    if(context_.peer_mask!=bit(context_.local_peer)&&!ordered_send(0,bytes,error))return false;
    error.clear();return true;
}
bool Controller::ordered_consume(uint64_t tick,tc::Receipt& receipt,uint64_t now,std::string& error){
    if(!guard(error)||!update_time(now,error))return false;
    if(!ordered_)return fail("Ordered consume requires ordered mode",error);
    auto& o=*ordered_;bool consumed=false;std::string placement_action;uint64_t placement_order=0;
    const bool invoked=invoke([&]{consumed=o.queue.consume(tick,[&](std::string& cause){
        const bool current=tc::wire::current(session_,context_,cause)&&engine_.current(binding_,context_.snapshot_uuid,cause);
        if(status_.locked){cause=status_.cause;return false;}return current;
    },[&](const tc::Assigned& assigned,std::string& cause){
        status_.pending=true;status_.host_revision=assigned.order;
        if(const auto* request=std::get_if<Request>(&assigned.input.command))status_.action_uuid=request->action_uuid;
        else status_.action_uuid.clear();
        std::string command_placement;
        if(const auto* request=std::get_if<Request>(&assigned.input.command);request&&request->action==Action::PlaceBuilding){
            command_placement=request->action_uuid;placement_action=command_placement;placement_order=assigned.order;
        }
        const auto outcome=place_trace::run(command_placement,"controller_execute",cause,[&]{return o.execute(o.queue,assigned,cause);},tick,assigned.order);
        if(status_.locked){cause=status_.cause;return Outcome::Unknown;}
        if(outcome==Outcome::Confirmed&&o.local&&*o.local==assigned.input)o.local_tick=tick;
        return outcome;
    },[&](uint64_t current,std::string& cause){
        return place_trace::run(placement_action,"original_go_and_task_drain",cause,[&]{return o.advance(current,cause);},current,placement_order);
    },receipt,error);return true;},error);
    place_trace::event(placement_action,"controller_consume","return",int(invoked&&consumed),0,error,tick,placement_order);
    if(!invoked||!consumed)return fail(error,error,true);
    if(context_.role==Role::Client){
        std::vector<uint8_t> bytes;
        if(!tc::wire::encode({context_.peer_mask,receipt},bytes,error)||!ordered_send(context_.host_peer,bytes,error))return fail(error,error,true);
    }else if(auto it=o.boundaries.find(tick);it!=o.boundaries.end()){
        it->second.mask|=bit(context_.local_peer);if(it->second.mask==context_.peer_mask)o.boundaries.erase(it);
    }
    ordered_status();error.clear();
    place_trace::event(placement_action,"controller_receipt","return",1,0,error,tick,placement_order);return true;
}
bool Controller::ordered_tick(std::string& error){
    auto& o=*ordered_;
    if(o.snapshot_hold){error.clear();return true;}
    if(o.captured){
        if(now_>=o.capture_deadline){o.captured.reset();return fail("Ordered UI capture timeout; input discarded without execution",error,true);}
        if(!o.local){
            auto request=std::move(*o.captured);o.captured.reset();o.capture_deadline=0;
            if(!ordered_submit({context_.session,context_.snapshot_uuid,std::move(request)},now_,error))return false;
        }
    }
    if(!o.waiting.empty()&&now_>=o.waiting_deadline)return fail("Ordered assignment timeout; retained input, no native retry",error,true);
    if(o.local&&now_>=o.local_deadline)return fail("Ordered input completion timeout; retained input, no native retry",error,true);
    for(const auto& [tick,boundary]:o.boundaries)if(now_>=boundary.deadline)return fail("Ordered tick completion timeout tick="+std::to_string(tick)+" actual_mask="+std::to_string(boundary.mask)+" required_mask="+std::to_string(context_.peer_mask)+"; no native retry",error,true);
    error.clear();return true;
}
