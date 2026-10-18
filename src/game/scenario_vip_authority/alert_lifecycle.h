#pragma once
#include "../live_host_state/vip_transitions.h"
#include <map>

namespace mt2mp::game::scenario_vip_authority::client_alert {
namespace vip=live_host_state::vip;
struct Owner {
    uintptr_t image=0,game=0,mode=0,state=0,world=0,map=0,hud=0,manager=0,window=0,window_ap=0,list=0;
    uint64_t epoch=0,generation=0;uint32_t thread=0;
    std::string native_uuid,native_sha;
    bool operator==(const Owner&)const=default;
};
struct Subject {
    int32_t id=-1;uintptr_t subscriber=0,toon=0,proxy=0;uint64_t uid=0;
    std::string name;
    bool operator==(const Subject&)const=default;
};
struct Call {
    bool attempted=false,returned=false;uint32_t exception=0;uintptr_t result=0;
};
struct MessageCalls {
    Call create,fire,discard,loc_construct,loc_arguments_destroy,loc_string_destroy;
    bool verified=false,handed_off=false;
    bool attempted()const{return create.attempted||fire.attempted||discard.attempted||loc_construct.attempted||
        loc_arguments_destroy.attempted||loc_string_destroy.attempted;}
};
struct Receipt {
    vip::Transition transition;
    bool attempted=false,confirmed=false,unknown=false,duplicate=false,shown=false,expired_before_presentation=false;
    Call create,show,hide,destroy;
    MessageCalls message;
    uintptr_t alert=0,selector=0;
    std::string cause;
};
struct BatchReceipt {bool attempted=false,confirmed=false,unknown=false;std::string cause;std::vector<Receipt> rows;};
struct Port {
    virtual ~Port()=default;
    virtual bool owner(const Owner* expected,bool teardown,Owner&,std::string&)=0;
    virtual bool subject(const Owner&,int32_t,const std::string&,Subject&,std::string&)=0;
    virtual bool inspect(const Owner&,uintptr_t,const Subject&,bool live_subject,int shown,uintptr_t& selector,std::string&)=0;
    virtual bool absent(const Owner&,uintptr_t alert,uintptr_t selector,std::string&)=0;
    virtual bool create(const Owner&,const Subject&,Call&,std::string&)=0;
    virtual bool show(const Owner&,uintptr_t,bool,Call&,std::string&)=0;
    virtual bool destroy(const Owner&,uintptr_t,Call&,std::string&)=0;
    virtual bool create_message(const Owner&,const std::string&,MessageCalls&,std::string& e){return vip::fail(e,"VIP message constructor unavailable");}
    virtual bool inspect_message(const Owner&,uintptr_t,const std::string&,int,uintptr_t&,std::string& e){return vip::fail(e,"VIP message readback unavailable");}
    virtual bool fire_message(const Owner&,uintptr_t,Call&,std::string& e){return vip::fail(e,"VIP message handoff unavailable");}
    virtual bool discard_message(const Owner&,uintptr_t,Call&,std::string& e){return vip::fail(e,"VIP unhanded message disposal unavailable");}
};
class Lifecycle {
    struct Entry {vip::Transition arrival;Owner owner;Subject subject;uintptr_t alert=0,selector=0;};
    vip::Origin origin_;
    std::map<uint64_t,Entry> entries_;
    std::map<uint64_t,Receipt> receipts_;
    bool unknown_=false,retired_=false;
    std::string cause_;
    Receipt last_;
    BatchReceipt result_;
    bool reject(Receipt& r,const std::string& cause,bool prior,std::string& e){
        r.cause=cause.empty()?"VIP native operation failed without cause":cause;
        r.confirmed=false;
        r.attempted=r.create.attempted||r.show.attempted||r.hide.attempted||r.destroy.attempted||r.message.attempted();
        r.unknown=prior||r.attempted;
        if(r.unknown){unknown_=true;cause_=r.cause;}
        last_=r;receipts_[r.transition.sequence]=r;result_.attempted|=prior||r.attempted;result_.unknown|=r.unknown;result_.cause=r.cause;result_.rows.push_back(r);e=r.cause;return false;
    }
    bool retired_identity(const vip::Transition& row)const{
        bool retired=false;
        for(const auto& [sequence,r]:receipts_){(void)sequence;
            if(r.transition.arrival!=row.arrival)continue;
            if(r.transition.subscriber!=row.subscriber||r.transition.name!=row.name||!r.confirmed)return false;
            if(!r.transition.active){if(r.transition.message)return false;retired=true;}
        }
        return retired;
    }
    bool message(Port& port,Receipt& r,bool prior,std::string& e){
        if(!r.transition.message)return true;
        Owner original,current;uintptr_t selector=0;
        if(!port.owner(nullptr,false,original,e))return reject(r,e,prior,e);
        const auto discard=[&](const std::string& cause){
            std::string cleanup;
            if(r.message.create.returned&&r.message.create.result&&!r.message.fire.attempted){
                if(!port.owner(&original,false,current,cleanup)||!port.discard_message(current,r.message.create.result,r.message.discard,cleanup)||
                   !r.message.discard.attempted||!r.message.discard.returned||
                   !port.owner(&original,false,current,cleanup)||!port.absent(current,r.message.create.result,0,cleanup))
                    return reject(r,cause+"; unhanded message cleanup unconfirmed: "+(cleanup.empty()?"original disposal return/readback receipt missing":cleanup),prior,e);
                r.message.create.result=0;
            }
            return reject(r,cause,prior,e);
        };
        if(!port.create_message(original,*r.transition.message,r.message,e))return discard(e);
        if(!r.message.create.attempted||!r.message.create.returned||!r.message.create.result)
            return discard("VIP message factory lacks original returned receiver");
        if(!port.owner(&original,false,current,e)||!port.inspect_message(current,r.message.create.result,*r.transition.message,0,selector,e))return discard(e);
        r.message.verified=true;
        if(!port.fire_message(current,r.message.create.result,r.message.fire,e))return discard(e);
        if(!r.message.fire.attempted||!r.message.fire.returned)return discard("VIP FireAndForget lacks original return receipt");
        if(!port.owner(&original,false,current,e)||!port.inspect_message(current,r.message.create.result,*r.transition.message,1,selector,e))
            return reject(r,e,prior,e);
        // AlertManager owns the allocation now. Keep only its immutable receipt;
        // no type9 pointer enters the scenario-owned Hide/D0 cleanup map.
        r.message.handed_off=true;r.message.create.result=0;r.attempted=true;r.confirmed=true;return true;
    }
    bool remove(Port& port,Entry& held,Receipt& r,bool teardown,bool prior,std::string& e){
        Owner current;uintptr_t selector=0;
        if(!port.owner(&held.owner,teardown,current,e)||
           !port.inspect(current,held.alert,held.subject,false,-1,selector,e))return reject(r,e,prior,e);
        // A user may already have dismissed this owned alert. Show(false) still
        // follows its original no-op path; no field is forced back to shown.
        if(!port.show(current,held.alert,false,r.hide,e))return reject(r,e,prior,e);
        if(!r.hide.attempted||!r.hide.returned)return reject(r,"VIP Hide lacks original return receipt",prior,e);
        r.attempted=true;
        if(!port.owner(&held.owner,teardown,current,e)||!port.inspect(current,held.alert,held.subject,false,0,selector,e)||
           !port.absent(current,held.alert,held.selector,e))return reject(r,e,true,e);
        if(!port.destroy(current,held.alert,r.destroy,e))return reject(r,e,true,e);
        if(!r.destroy.attempted||!r.destroy.returned)return reject(r,"VIP destructor lacks original return receipt",true,e);
        // After destructor return only live owner arrays may be read.
        const auto freed=held.alert;held.alert=0;
        if(!port.owner(&held.owner,teardown,current,e)||!port.absent(current,freed,held.selector,e))return reject(r,e,true,e);
        r.confirmed=true;r.shown=false;e.clear();return true;
    }
public:
    bool bind(const vip::Origin& origin,std::string& e){
        if(!entries_.empty()||unknown_)return vip::fail(e,"VIP bind requires confirmed empty prior native ownership");
        if(!vip::valid_origin(origin))return vip::fail(e,"VIP native origin invalid");
        origin_=origin;retired_=false;receipts_.clear();last_={};result_={};cause_.clear();e.clear();return true;
    }
    const BatchReceipt& result()const{return result_;}
    bool unknown()const{return unknown_;}
    const std::string& cause()const{return cause_;}
    const Receipt& last()const{return last_;}
    const std::map<uint64_t,Receipt>& receipts()const{return receipts_;}
    size_t owned()const{return entries_.size();}
    bool preflight(Port& port,const vip::Origin& origin,const std::vector<vip::Transition>& work,std::string& e){
        if(unknown_||retired_)return vip::fail(e,unknown_?cause_.c_str():"VIP owner lifetime already retired");
        if(origin!=origin_||!vip::valid_origin(origin))return vip::fail(e,"VIP native origin differs");
        uint64_t previous=0;auto active=entries_;
        for(const auto& row:work){
            if(!row.sequence||(previous&&row.sequence!=previous+1)||!vip::valid({row.sequence-1,{row}},e))
                return vip::fail(e,"VIP native planned transition order/shape differs");
            previous=row.sequence;
            const auto prior=receipts_.find(row.sequence);
            if(prior!=receipts_.end()){
                if(prior->second.transition!=row||!prior->second.confirmed)return vip::fail(e,"VIP native conflicting/unconfirmed repeated transition");
                continue;
            }
            if(row.active){
                if(active.contains(row.arrival))return vip::fail(e,"VIP planned arrival already owned");
                Entry entry;entry.arrival=row;active.emplace(row.arrival,std::move(entry));
            }else{
                const auto it=active.find(row.arrival);
                if(it!=active.end()){
                    if(it->second.arrival.subscriber!=row.subscriber||it->second.arrival.name!=row.name)return vip::fail(e,"VIP planned retirement identity differs");
                    active.erase(it);
                }else{
                    bool earlier=false;
                    for(const auto& prior_row:work)if(prior_row.sequence<row.sequence&&!prior_row.active&&prior_row.arrival==row.arrival&&
                        prior_row.subscriber==row.subscriber&&prior_row.name==row.name&&!prior_row.message)earlier=true;
                    if(!row.message||(!retired_identity(row)&&!earlier))return vip::fail(e,"VIP planned retirement identity differs");
                }
                if(row.message)for(const auto& prior_row:work)if(prior_row.sequence<row.sequence&&prior_row.arrival==row.arrival&&prior_row.message)
                    return vip::fail(e,"VIP planned disconnect already presented");
            }
        }
        for(const auto& row:work){
            if(receipts_.contains(row.sequence))continue;
            if(row.active){
                const bool expired=std::any_of(work.begin(),work.end(),[&](const auto& r){return !r.active&&r.arrival==row.arrival;});
                if(expired)continue;
                Owner owner;Subject subject;
                if(!port.owner(nullptr,false,owner,e)||!port.subject(owner,row.subscriber,row.name,subject,e))return false;
            }else if(const auto it=entries_.find(row.arrival);it!=entries_.end()){
                Owner owner;uintptr_t selector=0;
                if(!port.owner(&it->second.owner,false,owner,e)||!port.inspect(owner,it->second.alert,it->second.subject,false,-1,selector,e))return false;
            }
            if(row.message){Owner current;if(!port.owner(nullptr,false,current,e))return false;}
        }
        e.clear();return true;
    }
    bool apply(Port& port,const vip::Origin& origin,const std::vector<vip::Transition>& work,std::string& e){
        result_={};
        if(!preflight(port,origin,work,e)){result_.cause=e;result_.unknown=unknown_;return false;}
        if(work.empty()){last_={};last_.duplicate=true;last_.confirmed=true;result_.confirmed=true;e.clear();return true;}
        bool attempted=false;Receipt active_receipt;
        try{
            std::map<uint64_t,uint64_t> expired;
            for(const auto& row:work)if(!row.active&&!entries_.contains(row.arrival))
                for(const auto& arrival:work)if(arrival.active&&arrival.sequence==row.arrival)expired.emplace(row.arrival,row.sequence);
            for(const auto& row:work){
                active_receipt={};auto& r=active_receipt;r.transition=row;
                if(const auto it=receipts_.find(row.sequence);it!=receipts_.end()){
                    last_=it->second;last_.duplicate=true;result_.rows.push_back(last_);continue;
                }
                if(expired.contains(row.arrival)){
                    r.confirmed=true;r.expired_before_presentation=true;
                    if(!message(port,r,attempted,e))return false;
                    attempted=attempted||r.message.attempted();result_.attempted|=r.message.attempted();
                    receipts_[row.sequence]=r;last_=r;result_.rows.push_back(r);continue;
                }
                if(row.active){
                    Entry held;held.arrival=row;
                    if(entries_.contains(row.arrival))return reject(r,"VIP local arrival already owns an Alert",attempted,e);
                    if(!port.owner(nullptr,false,held.owner,e)||!port.subject(held.owner,row.subscriber,row.name,held.subject,e))
                        return reject(r,e,attempted,e);
                    // Keep the receiver even when a later readback fails.
                    auto [it,inserted]=entries_.emplace(row.arrival,held);
                    if(!inserted)return reject(r,"VIP arrival owner insertion differs",attempted,e);
                    auto& entry=it->second;
                    if(!port.create(entry.owner,entry.subject,r.create,e)){
                        entry.alert=r.create.result;r.alert=entry.alert;
                        if(!r.create.attempted)entries_.erase(it);
                        return reject(r,e,attempted,e);
                    }
                    entry.alert=r.create.result;r.alert=entry.alert;
                    if(!r.create.attempted||!r.create.returned||!entry.alert)return reject(r,"VIP constructor lacks returned receiver",attempted,e);
                    attempted=true;r.attempted=true;
                    Owner current;uintptr_t selector=0;
                    if(!port.owner(&entry.owner,false,current,e)||!port.inspect(current,entry.alert,entry.subject,true,0,selector,e))return reject(r,e,true,e);
                    if(!port.show(current,entry.alert,true,r.show,e))return reject(r,e,true,e);
                    if(!r.show.attempted||!r.show.returned)return reject(r,"VIP Show lacks original return receipt",true,e);
                    if(!port.owner(&entry.owner,false,current,e)||!port.inspect(current,entry.alert,entry.subject,true,1,entry.selector,e))return reject(r,e,true,e);
                    r.selector=entry.selector;r.shown=true;r.confirmed=true;
                }else{
                    auto it=entries_.find(row.arrival);
                    if(it==entries_.end()){
                        if(!row.message||!retired_identity(row))return reject(r,"VIP retirement lacks owned arrival",attempted,e);
                    }else{
                        r.alert=it->second.alert;r.selector=it->second.selector;
                        if(!remove(port,it->second,r,false,attempted,e))return false;
                        attempted=true;entries_.erase(it);
                    }
                    if(!message(port,r,attempted,e))return false;
                    attempted=attempted||r.message.attempted();
                }
                receipts_[row.sequence]=r;last_=r;result_.attempted|=r.attempted;result_.rows.push_back(r);
            }
            result_.confirmed=true;e.clear();return true;
        }catch(const std::exception& x){
            if(active_receipt.transition.active&&!active_receipt.create.attempted)entries_.erase(active_receipt.transition.arrival);
            return reject(active_receipt,std::string("VIP native lifecycle exception: ")+x.what(),attempted,e);
        }catch(...){
            if(active_receipt.transition.active&&!active_receipt.create.attempted)entries_.erase(active_receipt.transition.arrival);
            return reject(active_receipt,"VIP native lifecycle nonstandard exception",attempted,e);
        }
    }
    bool cleanup(Port& port,uintptr_t mode,std::string& e){
        result_={};
        if(unknown_){result_.unknown=true;result_.cause=cause_;return vip::fail(e,cause_.c_str());}
        if(entries_.empty()){result_.confirmed=true;e.clear();return true;}
        if(retired_){result_.cause="VIP cleanup attempted after owner retirement";return vip::fail(e,result_.cause.c_str());}
        bool attempted=false;Receipt current;
        try{for(auto it=entries_.begin();it!=entries_.end();){
            current={};auto& r=current;r.transition=it->second.arrival;r.alert=it->second.alert;r.selector=it->second.selector;
            if(!mode||mode!=it->second.owner.mode)return reject(r,"VIP cleanup exact Mode receiver differs",attempted,e);
            if(!remove(port,it->second,r,true,attempted,e))return false;
            attempted=true;last_=r;result_.attempted=true;result_.rows.push_back(r);it=entries_.erase(it);
        }result_.confirmed=true;e.clear();return true;
        }catch(const std::exception& x){return reject(current,std::string("VIP cleanup exception: ")+x.what(),attempted,e);}
        catch(...){return reject(current,"VIP cleanup nonstandard exception",attempted,e);}
    }
    void after_deinit(uintptr_t mode){
        for(const auto& [sequence,entry]:entries_){(void)sequence;if(entry.owner.mode==mode){
            unknown_=true;if(cause_.empty())cause_="VIP owner retired with unconfirmed native cleanup; no late destructor";}}
        retired_=true;
    }
};
}
