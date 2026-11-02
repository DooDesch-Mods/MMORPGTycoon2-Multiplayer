#include "tick_commands.h"
#include <algorithm>
#include <limits>

namespace mt2mp::action_session::tick_commands {
namespace {
uint8_t bit(uint64_t peer){return uint8_t(1u<<(peer-1));}
uint64_t peer(const Input& in){return std::visit([](const auto& r){return r.peer;},in.command);}
uint64_t sequence(const Input& in){return std::visit([](const auto& r){return r.sequence;},in.command);}
uint64_t observed(const Input& in){if(const auto* r=std::get_if<Request>(&in.command))return r->host_revision;if(const auto* r=std::get_if<TimeRequest>(&in.command))return r->observed_revision;return std::get<SnapshotRequest>(in.command).observed_revision;}
bool same_key(const Input& a,const Input& b){return a.command.index()==b.command.index()&&peer(a)==peer(b)&&sequence(a)==sequence(b);}
uint64_t last(const Batch& batch){return batch.commands.empty()?batch.previous_order:batch.commands.back().order;}
std::string evidence_difference(const std::vector<CommandEvidence>& expected,const std::vector<CommandEvidence>& actual){
    if(expected.size()!=actual.size())return " count_actual="+std::to_string(actual.size())+" count_required="+std::to_string(expected.size());
    for(size_t i=0;i<expected.size();++i)if(expected[i]!=actual[i])return " order_actual="+std::to_string(actual[i].order)+
        " order_required="+std::to_string(expected[i].order)+" sha256_actual="+actual[i].sha256+" sha256_required="+expected[i].sha256;
    return {};
}
struct Busy {bool& value;explicit Busy(bool& b):value(b){value=true;}~Busy(){value=false;}};
std::string detail(const std::string& phase,const std::string& cause){return phase+": "+(cause.empty()?"callback returned failure without a cause":cause);}
}
Queue::Queue(Config config,Validate validate):config_(std::move(config)),validate_(std::move(validate)),thread_(std::this_thread::get_id()){
    const auto& c=config_.context;
    if(!c.session||c.snapshot_uuid.empty()||c.host_peer!=1||c.local_peer<1||c.local_peer>4||!c.peer_mask||c.peer_mask>15||
       !(c.peer_mask&bit(c.host_peer))||!(c.peer_mask&bit(c.local_peer))||
       (c.role!=Role::Host&&c.role!=Role::Client)||(c.role==Role::Host)!=(c.local_peer==c.host_peer)||!config_.first_tick||!validate_)
        throw std::invalid_argument("Tick command queue requires a loaded immutable session, role and validator");
    state_.next_tick=state_.next_assignment_tick=config_.first_tick;state_.assigned_order=state_.sealed_order=state_.completed_order=config_.initial_order;
    queued_.reserve(batch_limit);
}
bool Queue::reject(const std::string& cause,std::string& out,bool lock){
    if(lock&&!state_.locked){state_.locked=true;state_.cause=cause;}
    out=state_.locked?state_.cause:cause;return false;
}
bool Queue::guard(std::string& cause){
    if(std::this_thread::get_id()!=thread_)return reject("Tick command queue requires its bound game thread",cause);
    if(busy_)return reject("Tick command queue reentered an admission or native callback",cause,true);
    if(state_.locked)return reject(state_.cause,cause);
    return true;
}
bool Queue::input(const Input& in,std::string& cause){
    const auto& c=config_.context;const auto sender=peer(in);
    if(in.session!=c.session||in.snapshot_uuid!=c.snapshot_uuid)return reject("Tick command session or snapshot differs",cause);
    if(sender<1||sender>4||!(c.peer_mask&bit(sender))||!sequence(in))return reject("Tick command peer or sequence invalid",cause);
    if(const auto* request=std::get_if<Request>(&in.command)){
        if(request->snapshot_uuid!=c.snapshot_uuid||request->action_uuid.empty())return reject("Gameplay command snapshot or action UUID invalid",cause);
     }else if(const auto* time=std::get_if<TimeRequest>(&in.command)){
        if(uint8_t(time->intent)>uint8_t(game::time_session::Intent::TogglePauseNormal))return reject("Time command intent invalid",cause);
    }else {
        const auto& save=std::get<SnapshotRequest>(in.command);
        if(save.peer!=c.host_peer||save.nonce.size()!=36||uint8_t(save.purpose)>1)return reject("Snapshot preparation requires Host, exact nonce and valid purpose",cause);
    }
    try{if(!validate_(in,cause))return reject(detail("Tick command validation",cause),cause);}
    catch(const std::exception& e){return reject(std::string("Tick command validator exception: ")+e.what(),cause,true);}
    catch(...){return reject("Tick command validator threw an unknown exception",cause,true);}
    if(state_.locked)return reject(state_.cause,cause);
    return true;
}
void Queue::remember(const Assigned& item){history_.push_back(item);while(history_.size()>history_limit)history_.pop_front();}
bool Queue::capacity(const Capacity& check,const Batch& batch,std::string& cause){
    if(!check)return true;
    try{if(!check(batch,cause))return reject(detail("Tick command capacity",cause),cause);}
    catch(const std::exception& e){return reject(std::string("Tick command capacity exception: ")+e.what(),cause,true);}
    catch(...){return reject("Tick command capacity threw an unknown exception",cause,true);}
    if(state_.locked)return reject(state_.cause,cause);
    return true;
}
Admission Queue::offer(uint64_t sender,const Input& in,Assigned& result,std::string& cause,const Capacity& check){
    if(!guard(cause))return Admission::Rejected;
    Busy busy(busy_);
    if(config_.context.role!=Role::Host){reject("Only the Host assigns tick commands",cause);return Admission::Rejected;}
    if(sender!=peer(in)||sender<1||sender>4||!(config_.context.peer_mask&bit(sender))){reject("Tick command authenticated sender differs",cause);return Admission::Rejected;}
    auto match=std::find_if(history_.begin(),history_.end(),[&](const Assigned& a){return same_key(a.input,in);});
    if(match!=history_.end()){
        if(match->input!=in){reject("Conflicting duplicate tick command",cause,true);return Admission::Rejected;}
        result=*match;cause.clear();return Admission::Duplicate;
    }
    const auto high=config_.sequences[in.command.index()][sender];
    if(high==UINT64_MAX||sequence(in)!=high+1){reject("Tick command sequence gap, expired or reordered",cause);return Admission::Rejected;}
    if(!queued_.empty()&&std::holds_alternative<SnapshotRequest>(queued_.back().input.command)){reject("Snapshot preparation must end its batch",cause);return Admission::Rejected;}
    if(queued_.size()==batch_limit){reject("Tick command queue is full; request was not assigned",cause);return Admission::Rejected;}
    if(state_.assigned_order==UINT64_MAX){reject("Tick command Host order exhausted",cause,true);return Admission::Rejected;}
    if(observed(in)>state_.assigned_order){reject("Tick command observes a future Host order",cause);return Admission::Rejected;}
    if(!input(in,cause))return Admission::Rejected;
    if(const auto* r=std::get_if<Request>(&in.command))for(const auto& old:history_)if(const auto* previous=std::get_if<Request>(&old.input.command);previous&&previous->action_uuid==r->action_uuid){reject("Gameplay action UUID was already assigned",cause,true);return Admission::Rejected;}
    Assigned item{state_.assigned_order+1,in};
    if(check){
        const auto& c=config_.context;
        Batch prospective{c.session,state_.next_assignment_tick,state_.sealed_order,c.peer_mask,c.snapshot_uuid,queued_};
        prospective.commands.push_back(item);
        if(!capacity(check,prospective,cause))return Admission::Rejected;
    }
    queued_.push_back(item);remember(item);state_.assigned_order=item.order;config_.sequences[in.command.index()][sender]=sequence(in);
    result=std::move(item);cause.clear();return Admission::Accepted;
}
Queue::Slot* Queue::find(uint64_t tick){
    for(auto& slot:in_flight_)if(slot.batch.tick==tick)return &slot;
    for(auto& slot:completed_)if(slot.batch.tick==tick)return &slot;
    return nullptr;
}
void Queue::retire(){
    while(!in_flight_.empty()){
        const auto& first=in_flight_.front();
        if(!first.receipt||!first.receipt->tick_completed||
           (config_.context.role==Role::Host&&first.acknowledgements!=config_.context.peer_mask))break;
        if(!first.receipt->command_evidence.empty()){
            state_.evidence_tick=first.receipt->tick;state_.evidence_order=first.receipt->command_evidence.back().order;
            state_.evidence_ack_mask=first.acknowledgements;
        }
        // Keep command completion independently of empty simulation ticks and later UI input.
        // Host retirement requires every peer ACK; Clients retain their authenticated batch completion.
        for(const auto& command:first.batch.commands)if(const auto* request=std::get_if<Request>(&command.input.command)){
            completed_actions_.push_back({request->action_uuid,first.batch.snapshot_uuid,first.batch.session,
                command.order,first.batch.tick,request->peer,request->sequence,first.batch.peer_mask,first.acknowledgements});
            while(completed_actions_.size()>history_limit)completed_actions_.pop_front();
        }
        completed_.push_back(std::move(in_flight_.front()));in_flight_.pop_front();
        while(completed_.size()>history_limit)completed_.pop_front();
    }
}
bool Queue::seal(Batch& out,std::string& cause,const Capacity& check){
    if(!guard(cause))return false;
    Busy busy(busy_);
    if(config_.context.role!=Role::Host)return reject("Only the Host seals a tick batch",cause);
    if(in_flight_.size()==in_flight_limit)return reject("Tick admission window is full; waiting for retained completions",cause);
    if(state_.next_assignment_tick==UINT64_MAX)return reject("Tick command boundary exhausted",cause,true);
    const auto& c=config_.context;
    Batch batch{c.session,state_.next_assignment_tick,state_.sealed_order,c.peer_mask,c.snapshot_uuid,queued_};
    if(!capacity(check,batch,cause))return false;
    in_flight_.push_back(Slot{batch});state_.sealed_order=last(batch);++state_.next_assignment_tick;
    queued_.clear();out=std::move(batch);cause.clear();return true;
}
Admission Queue::admit(uint64_t sender,const Batch& batch,std::string& cause){
    if(!guard(cause))return Admission::Rejected;
    Busy busy(busy_);
    const auto& c=config_.context;
    if(c.role!=Role::Client||sender!=c.host_peer){reject("Tick batch requires the authenticated Host on a Client",cause);return Admission::Rejected;}
    if(const auto* retained=find(batch.tick)){
        if(retained->batch!=batch){reject("Conflicting duplicate sealed tick batch",cause,true);return Admission::Rejected;}
        cause.clear();return Admission::Duplicate;
    }
    if(in_flight_.size()==in_flight_limit){reject("Tick admission window is full; request was not admitted",cause);return Admission::Rejected;}
    if(batch.session!=c.session||batch.snapshot_uuid!=c.snapshot_uuid||batch.peer_mask!=c.peer_mask||batch.tick!=state_.next_assignment_tick||batch.tick==UINT64_MAX||batch.previous_order!=state_.sealed_order||batch.commands.size()>batch_limit){reject("Tick batch session, membership, tick, previous order or bound differs",cause);return Admission::Rejected;}
    auto sequences=config_.sequences;uint64_t order=batch.previous_order;
    std::vector<std::string> ids;
    for(const auto& item:batch.commands){
        if(std::holds_alternative<SnapshotRequest>(item.input.command)&& &item!=&batch.commands.back()){reject("Snapshot preparation is not final in batch",cause);return Admission::Rejected;}
        if(order==UINT64_MAX||item.order!=order+1||observed(item.input)>order){reject("Tick batch command order invalid",cause);return Admission::Rejected;}
        if(!input(item.input,cause))return Admission::Rejected;
        const auto p=peer(item.input),s=sequence(item.input);auto& high=sequences[item.input.command.index()][p];
        if(high==UINT64_MAX||s!=high+1){reject("Tick batch command sequence gap or duplicate",cause);return Admission::Rejected;}
        if(const auto* r=std::get_if<Request>(&item.input.command)){
            bool old_id=std::any_of(history_.begin(),history_.end(),[&](const Assigned& old){const auto* old_r=std::get_if<Request>(&old.input.command);return old_r&&old_r->action_uuid==r->action_uuid;});
            if(old_id||std::find(ids.begin(),ids.end(),r->action_uuid)!=ids.end()){reject("Tick batch reuses a gameplay action UUID",cause);return Admission::Rejected;}
            ids.push_back(r->action_uuid);
        }
        high=s;order=item.order;
    }
    in_flight_.push_back(Slot{batch});config_.sequences=sequences;state_.assigned_order=state_.sealed_order=order;++state_.next_assignment_tick;
    for(const auto& item:batch.commands)remember(item);
    cause.clear();return Admission::Accepted;
}
bool Queue::consume(uint64_t tick,const Current& current,const Execute& execute,const Advance& advance,Receipt& out,std::string& cause){
    if(std::this_thread::get_id()!=thread_)return reject("Tick command consumption requires its bound game thread",cause);
    if(busy_)return reject("Tick command consumption reentered",cause,true);
    auto* slot=find(tick);
    if(slot&&slot->receipt){out=*slot->receipt;cause=out.cause;return out.tick_completed;}
    if(!guard(cause))return false;
    if(!slot||tick!=state_.next_tick)return reject("No sealed command batch for the next simulation boundary",cause);
    if(!current||!execute||!advance)return reject("Tick command execution callbacks incomplete",cause);
    Busy busy(busy_);state_.consuming=true;
    Receipt receipt{config_.context.session,tick,last(slot->batch),config_.context.snapshot_uuid};
    // Retain before entering native code. A failure cannot make this batch retryable.
    slot->receipt=receipt;
    auto failed=[&](Outcome outcome,const std::string& phase,const std::string& error){
        receipt.outcome=outcome==Outcome::RejectedNoEffect&&!receipt.completed_commands&&!receipt.tick_attempted?outcome:Outcome::Unknown;
        receipt.cause=detail(phase,error);reject(receipt.cause,cause,true);receipt.cause=cause;slot->receipt=receipt;out=receipt;state_.consuming=false;return false;
    };
    std::string phase="tick world admission",error;
    try{
        if(!current(error)||state_.locked)return failed(Outcome::RejectedNoEffect,phase,state_.locked?state_.cause:error);
        for(const auto& item:slot->batch.commands){
            phase="command world admission order="+std::to_string(item.order);error.clear();
            if(!current(error)||state_.locked)return failed(Outcome::RejectedNoEffect,phase,state_.locked?state_.cause:error);
            phase="native command order="+std::to_string(item.order);error.clear();
            Outcome outcome=Outcome::Unknown;
            {
                active_execution_=&item;active_tick_=tick;active_receipt_=&receipt;
                struct ClearExecution {Queue& queue;~ClearExecution(){queue.active_execution_=nullptr;queue.active_tick_=0;queue.active_receipt_=nullptr;}} clear{*this};
                outcome=execute(item,error);
            }
            if(outcome!=Outcome::Confirmed||state_.locked)return failed(outcome,phase,state_.locked?state_.cause:error);
            if(requires_evidence(item.input)&&(receipt.command_evidence.empty()||receipt.command_evidence.back().order!=item.order))
                return failed(Outcome::Unknown,phase,"Required canonical command evidence was not bound");
            ++receipt.completed_commands;
        }
        for(size_t peer=1;peer<slot->peer_receipts.size();++peer)if(slot->peer_receipts[peer]&&
            slot->peer_receipts[peer]->command_evidence!=receipt.command_evidence)
            return failed(Outcome::Unknown,"peer canonical command evidence","peer="+std::to_string(peer)+" tick="+std::to_string(tick)+" differs before local simulation advance"+evidence_difference(receipt.command_evidence,slot->peer_receipts[peer]->command_evidence));
        phase="simulation boundary admission";error.clear();
        if(!current(error)||state_.locked)return failed(Outcome::RejectedNoEffect,phase,state_.locked?state_.cause:error);
        phase="original simulation tick";error.clear();receipt.tick_attempted=true;
        const auto outcome=advance(tick,error);
        if(outcome!=Outcome::Confirmed||state_.locked)return failed(outcome,phase,state_.locked?state_.cause:error);
        phase="completed tick world admission";error.clear();
        if(!current(error)||state_.locked)return failed(Outcome::Unknown,phase,state_.locked?state_.cause:error);
    }catch(const std::exception& e){return failed(Outcome::Unknown,phase,e.what());}
    catch(...){return failed(Outcome::Unknown,phase,"unknown native callback exception");}
    receipt.outcome=Outcome::Confirmed;receipt.tick_completed=true;slot->receipt=receipt;out=receipt;state_.completed_order=receipt.last_order;
    ++state_.next_tick;slot->acknowledgements|=bit(config_.context.local_peer);state_.consuming=false;retire();cause.clear();return true;
}
bool Queue::acknowledge(uint64_t sender,const Receipt& receipt,std::string& cause){
    if(!guard(cause))return false;
    const auto& c=config_.context;
    if(c.role!=Role::Host||sender==c.local_peer||sender<1||sender>4||!(c.peer_mask&bit(sender)))return reject("Tick completion requires an authenticated remote peer on Host",cause);
    auto* slot=find(receipt.tick);
    if(!slot||receipt.session!=c.session||receipt.snapshot_uuid!=c.snapshot_uuid||receipt.last_order!=last(slot->batch))return reject("Tick completion identity differs from a retained sealed batch",cause);
    if(slot->peer_receipts[sender]){
        if(*slot->peer_receipts[sender]!=receipt)return reject("Conflicting duplicate tick completion",cause,true);
        cause.clear();return true;
    }
    if(receipt.outcome!=Outcome::Confirmed||!receipt.tick_attempted||!receipt.tick_completed||receipt.completed_commands!=slot->batch.commands.size()||!receipt.cause.empty())return reject(detail("Peer "+std::to_string(sender)+" did not complete tick "+std::to_string(receipt.tick),receipt.cause),cause,true);
    size_t evidence_index=0;
    for(const auto& command:slot->batch.commands)if(requires_evidence(command.input)){
        if(evidence_index>=receipt.command_evidence.size()||receipt.command_evidence[evidence_index].order!=command.order||
           !evidence_digest(receipt.command_evidence[evidence_index].sha256))
            return reject("Peer canonical command evidence missing or differs from required order="+std::to_string(command.order),cause,true);
        ++evidence_index;
    }
    if(evidence_index!=receipt.command_evidence.size())return reject("Peer canonical command evidence has unrelated entries",cause,true);
    if(slot->receipt&&slot->receipt->tick_completed&&slot->receipt->command_evidence!=receipt.command_evidence)
        return reject("Peer canonical command evidence differs peer="+std::to_string(sender)+" tick="+std::to_string(receipt.tick)+evidence_difference(slot->receipt->command_evidence,receipt.command_evidence),cause,true);
    slot->peer_receipts[sender]=receipt;slot->acknowledgements|=bit(sender);retire();cause.clear();return true;
}

bool Queue::bind_evidence(const Assigned& command,const std::string& sha256,std::string& cause){
    Context context;uint64_t tick=0;
    if(!current_execution(command,context,tick,cause)||!active_receipt_)return false;
    if(!requires_evidence(command.input)||!evidence_digest(sha256)||
       (!active_receipt_->command_evidence.empty()&&active_receipt_->command_evidence.back().order>=command.order))
        return reject("Canonical command evidence requires one exact active command and SHA256",cause,true);
    active_receipt_->command_evidence.push_back({command.order,sha256});cause.clear();return true;
}

bool Queue::current_execution(const Assigned& command,Context& context,uint64_t& tick,std::string& cause)const {
    if(std::this_thread::get_id()!=thread_||!busy_||!state_.consuming||state_.locked||
       !active_execution_||*active_execution_!=command||active_tick_!=state_.next_tick){
        cause="No matching authenticated command is executing at this tick boundary";return false;
    }
    context=config_.context;tick=active_tick_;cause.clear();return true;
}
std::string Queue::retirement_diagnostic()const{
    if(std::this_thread::get_id()!=thread_)return "{\"owner_thread\":false}";
    size_t commands=0,empty=0,absent=0,attempted=0,completed=0,unknown=0;
    auto slots=nlohmann::json::array();
    for(const auto& slot:in_flight_){
        commands+=slot.batch.commands.size();empty+=slot.batch.commands.empty();absent+=!slot.receipt;
        if(slot.receipt){attempted+=slot.receipt->tick_attempted;completed+=slot.receipt->tick_completed;
            unknown+=slot.receipt->outcome==Outcome::Unknown;}
        if(slots.size()<16)slots.push_back({{"tick",slot.batch.tick},{"commands",slot.batch.commands.size()},
            {"ack_mask",slot.acknowledgements},{"required_mask",slot.batch.peer_mask},
            {"local_receipt",slot.receipt.has_value()},
            {"tick_attempted",slot.receipt&&slot.receipt->tick_attempted},
            {"tick_completed",slot.receipt&&slot.receipt->tick_completed},
            {"outcome",slot.receipt?int(slot.receipt->outcome):-1}});
    }
    return nlohmann::json{{"owner_thread",true},{"busy",busy_},{"locked",state_.locked},{"consuming",state_.consuming},
        {"active_execution",active_execution_!=nullptr},{"active_tick",active_tick_},{"active_receipt",active_receipt_!=nullptr},
        {"next_tick",state_.next_tick},{"next_assignment_tick",state_.next_assignment_tick},
        {"assigned_order",state_.assigned_order},{"sealed_order",state_.sealed_order},{"completed_order",state_.completed_order},
        {"queued",queued_.size()},{"in_flight",in_flight_.size()},{"in_flight_commands",commands},{"empty_batches",empty},
        {"receipt_absent",absent},{"tick_attempted",attempted},{"tick_completed",completed},{"unknown",unknown},
        {"cause",state_.cause},{"slots_omitted",in_flight_.size()>16?in_flight_.size()-16:0},{"slots",std::move(slots)}}.dump();
}
// Recovery abandons this interrupted session only after a new Host snapshot.
// These reads retain every old receipt and ACK; ordinary retirement stays strict.
bool Queue::can_retire_recovery(uint64_t completed_tick)const{
    if(std::this_thread::get_id()!=thread_||busy_||state_.locked||state_.consuming||
       active_execution_||active_tick_||active_receipt_||!state_.cause.empty()||!queued_.empty()||
       completed_tick==UINT64_MAX||state_.next_tick!=completed_tick+1||
       state_.next_assignment_tick!=state_.next_tick||state_.assigned_order!=state_.sealed_order||
       state_.sealed_order!=state_.completed_order)return false;
    const auto& c=config_.context;
    uint64_t previous=0;
    for(const auto& slot:in_flight_){
        const auto& b=slot.batch;
        if(!b.commands.empty()||!slot.receipt||b.session!=c.session||b.snapshot_uuid!=c.snapshot_uuid||
           b.peer_mask!=c.peer_mask||!b.tick||b.tick>completed_tick||(previous&&b.tick!=previous+1)||
           b.previous_order!=state_.completed_order||!(slot.acknowledgements&bit(c.local_peer))||
           (slot.acknowledgements&~c.peer_mask))return false;
        const auto& r=*slot.receipt;
        if(r.session!=b.session||r.snapshot_uuid!=b.snapshot_uuid||r.tick!=b.tick||r.last_order!=b.previous_order||
           r.completed_commands||r.outcome!=Outcome::Confirmed||!r.tick_attempted||!r.tick_completed||
           !r.cause.empty()||!r.command_evidence.empty())return false;
        for(size_t peer=1;peer<slot.peer_receipts.size();++peer){
            if(peer==c.local_peer){if(slot.peer_receipts[peer])return false;continue;}
            if(bool(slot.peer_receipts[peer])!=bool(slot.acknowledgements&bit(peer))||
               (slot.peer_receipts[peer]&&*slot.peer_receipts[peer]!=r))return false;
        }
        previous=b.tick;
    }
    return in_flight_.empty()||previous==completed_tick;
}
bool Queue::recovery_boundary_matches(uint64_t tick,uint8_t mask)const{
    if(std::this_thread::get_id()!=thread_)return false;
    for(const auto& slot:in_flight_)if(slot.batch.tick==tick)return slot.acknowledgements==mask;
    return false;
}
State Queue::state()const{auto out=state_;out.queued=queued_.size();out.in_flight=in_flight_.size();return out;}
}
