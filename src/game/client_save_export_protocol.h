#pragma once
#include "client_save_export.h"
#include "mt2mp/net/snapshot_sync.h"

namespace mt2mp::game::client_save_export {
enum class Publication {Pending,Confirmed,Failed};
class Protocol {
public:
    enum class Phase {Idle,Requested,Accepted,Capturing,Sending,Receiving,Verified,Publishing,Transferred,Published,Complete,Finished,Failed};
    struct Ports {
        Current current;
        std::function<bool(uint64_t,const Json&)> control;
        std::function<bool(uint64_t,const Identity&,const Chunk&)> chunk;
        // Queue one shared capture with exactly identity.nonce. No Client path
        // or Client native pointer is available at this Host boundary.
        std::function<bool(const Identity&,uint32_t,bool,std::string&)> capture;
        // Called from held service. Pending preserves the original publication
        // and cache task owner; it must never start a second native Save.
        std::function<Publication(const Receiver&,std::string&)> publish;
        std::function<bool(const Identity&,std::string&)> accept;
    };
    Protocol(uint64_t session,uint64_t world,uint64_t local,uint8_t mask,Ports ports):session_(session),world_(world),local_(local),mask_(mask),ports_(std::move(ports)){
        require(session&&world&&local>=1&&local<=4&&mask<16&&(mask&1)&&(mask&(1u<<(local-1)))&&ports_.current&&ports_.control&&ports_.chunk,
            "protocol bound owner/ports invalid");
    }
    void request(const std::string& nonce,const fs::path& destination,const fs::path& staging,uint32_t kind,bool option){
        guard();require(local_!=1&&phase_==Phase::Idle&&(kind==1||kind==2),"Client request requires idle original Save owner");
        identity_={session_,world_,local_,mask_,nonce};valid(identity_);destination_=destination;staging_=staging;kind_=kind;option_=option;
        phase_=Phase::Requested;emit(1,{{"type","save_export_request"},{"identity",identity_wire()},{"kind",kind},{"option",option}});
    }
    void captured(const std::string& nonce,const fs::path& native){
        guard();require(local_==1&&phase_==Phase::Capturing&&nonce==identity_.nonce,"Host file does not belong to the one accepted capture nonce");
        offer_=source_.begin(identity_,native,ports_.current);phase_=Phase::Sending;
        emit(identity_.requester,{{"type","save_export_offer"},{"offer",wire(offer_)}});
    }
    void control(uint64_t sender,const Json& value)try{
        guard();progress_=Clock::now();require(value.is_object()&&value.contains("type"),"export control shape absent");const auto type=value.at("type").get<std::string>();
        if(type=="save_export_request"){
            require(local_==1&&phase_==Phase::Idle&&value.size()==4&&ports_.capture,"Host request lacks idle capture owner");
            auto request=parse_identity(value.at("identity"));require(sender==request.requester,"request physical peer differs");
            require(value.at("kind").is_number_unsigned()&&value.at("option").is_boolean(),"request original options shape differs");
            kind_=value.at("kind").get<uint32_t>();option_=value.at("option").get<bool>();require(kind_==1||kind_==2,"request original Save kind differs");
            std::string admission;
            require(ports_.accept&&ports_.accept(request,admission),admission.empty()?"Host export request owner did not admit its nonce":admission);
            identity_=request;phase_=Phase::Accepted;emit(sender,{{"type","save_export_accepted"},{"identity",identity_wire()}});return;
        }
        const bool from_host=sender==1&&local_==identity_.requester;
        const bool from_client=local_==1&&sender==identity_.requester;
        require(from_host||from_client,"control physical owner differs");
        if(type=="save_export_accepted"){
            require(from_host&&phase_==Phase::Requested&&value.size()==2&&parse_identity(value.at("identity"))==identity_,"Host accepted a different request");
            phase_=Phase::Accepted;emit(1,{{"type","save_export_accept_ack"},{"identity",identity_wire()}});return;
        }
        if(type=="save_export_accept_ack"){
            require(from_client&&phase_==Phase::Accepted&&value.size()==2&&parse_identity(value.at("identity"))==identity_,"accepted nonce acknowledgement differs");
            // Consume before the callback, even if native capture admission fails.
            phase_=Phase::Capturing;std::string error;require(ports_.capture(identity_,kind_,option_,error),
                error.empty()?"Host capture callback failed without original cause":error);return;
        }
        if(type=="save_export_offer"){
            require(from_host&&phase_==Phase::Accepted&&value.size()==2,"file offer arrived outside accepted request");
            offer_=parse(value.at("offer"),identity_);receiver_.begin(sender,identity_,value.at("offer"),destination_,staging_,ports_.current);
            phase_=Phase::Receiving;emit(1,{{"type","save_export_ready"},{"identity",identity_wire()}});return;
        }
        if(type=="save_export_ready"){
            require(from_client&&phase_==Phase::Sending&&!ready_&&value.size()==2&&parse_identity(value.at("identity"))==identity_,"receiver readiness differs");ready_=true;return;
        }
        if(type=="save_export_chunk_ack"){
            require(from_client&&phase_==Phase::Sending&&value.size()==3&&parse_identity(value.at("identity"))==identity_&&value.at("next").is_number_unsigned(),"chunk ACK identity/schema differs");
            const auto next=value.at("next").get<uint64_t>();require(next<=UINT32_MAX,"chunk ACK index overflow");source_.acknowledge(sender,identity_,uint32_t(next));return;
        }
        if(type=="save_export_verified"){
            require(from_client&&phase_==Phase::Sending&&value.size()==2,"verification arrived outside transfer");source_.verified(sender,value.at("offer"));phase_=Phase::Transferred;return;
        }
        if(type=="save_export_published"){
            require(from_client&&phase_==Phase::Transferred&&source_.complete()&&value.size()==2&&parse(value.at("offer"),identity_)==offer_,"publication acknowledgement differs from transferred files");
            phase_=Phase::Published;emit(sender,{{"type","save_export_publication_ack"},{"offer",wire(offer_)}});return;
        }
        if(type=="save_export_publication_ack"){
            require(from_host&&phase_==Phase::Published&&value.size()==2&&parse(value.at("offer"),identity_)==offer_,"Host final publication acknowledgement differs");phase_=Phase::Complete;return;
        }
        if(type=="save_export_complete"){
            require(from_host&&phase_==Phase::Complete&&value.size()==2&&parse(value.at("offer"),identity_)==offer_,
                "Host completion differs from local publication receipt");phase_=Phase::Finished;return;
        }
        throw std::runtime_error("Client Save export control type not admitted: "+type);
    }catch(const std::exception& error){failed(error.what());throw;}
    void chunk(uint64_t sender,const Identity& identity,const Chunk& value)try{
        guard();require(local_!=1&&phase_==Phase::Receiving,"bytes arrived outside Client transfer");
        const auto next=receiver_.receive(sender,identity,value);emit(1,{{"type","save_export_chunk_ack"},{"identity",identity_wire()},{"next",next}});
        if(receiver_.verified()){
            phase_=Phase::Verified;emit(1,{{"type","save_export_verified"},{"offer",receiver_.receipt()}});
        }
    }catch(const std::exception& error){failed(error.what());throw;}
    void tick()try{
        if(phase_==Phase::Idle||phase_==Phase::Finished)return;
        guard();if(local_==1&&phase_==Phase::Published)return;
        const auto limit=phase_==Phase::Accepted||phase_==Phase::Capturing?net::snapshot_native_capture_timeout_seconds:net::snapshot_data_idle_timeout_seconds;
        require(Clock::now()-progress_<std::chrono::seconds(limit),"export progress timeout phase="+std::to_string(int(phase_))+" limit_seconds="+std::to_string(limit));
        if(local_==1&&phase_==Phase::Sending&&ready_){
            for(unsigned count=0;count<4;++count){Chunk chunk;if(!source_.next(chunk))break;
                require(ports_.chunk(identity_.requester,identity_,chunk),"export chunk queue rejected");}
        }
        if(local_!=1&&(phase_==Phase::Verified||phase_==Phase::Publishing)){
            require(bool(ports_.publish),"Client local publication/cache callback unavailable");phase_=Phase::Publishing;std::string error;
            const auto result=ports_.publish(receiver_,error);
            require(result!=Publication::Failed,error.empty()?"Client publication failed without original cause":error);
            if(result==Publication::Confirmed){phase_=Phase::Published;emit(1,{{"type","save_export_published"},{"offer",receiver_.receipt()}});}
        }
    }catch(const std::exception& error){failed(error.what());throw;}
    void released(){
        guard();require(local_==1&&phase_==Phase::Published,"Host completion requires final Client publication ACK");
        phase_=Phase::Finished;emit(identity_.requester,{{"type","save_export_complete"},{"offer",wire(offer_)}});
    }
    bool release_confirmed()const{return phase_==Phase::Finished;}
    Phase phase()const{return phase_;}
    const std::string& cause()const{return cause_;}
    const Identity& identity()const{return identity_;}
    bool publication_confirmed()const{return phase_==Phase::Finished||(local_==1?phase_==Phase::Published:phase_==Phase::Complete);}
    void failed(const std::string& cause){if(cause_.empty())cause_=cause.empty()?"export failed without original cause":cause;phase_=Phase::Failed;}
    bool retire(std::string& error){
        const bool first=source_.retire(error);std::string second;const bool last=receiver_.retire(second);
        if(!last){if(!error.empty())error+="; ";error+=second;}return first&&last;
    }
private:
    using Clock=std::chrono::steady_clock;
    uint64_t session_,world_,local_;uint8_t mask_;Ports ports_;Identity identity_;Offer offer_;
    fs::path destination_,staging_;uint32_t kind_=0;bool option_=false,ready_=false;
    Phase phase_=Phase::Idle;Clock::time_point progress_=Clock::now();std::string cause_;Source source_;Receiver receiver_;
    void guard(){require(phase_!=Phase::Failed,cause_);current(ports_.current);}
    Json identity_wire()const{return {{"session",identity_.session},{"world",identity_.world},{"requester",identity_.requester},{"mask",identity_.mask},{"nonce",identity_.nonce}};}
    Identity parse_identity(const Json& value)const{
        require(value.is_object()&&value.size()==5,"request identity exact schema differs");
        for(const char* key:{"session","world","requester","mask"})require(value.at(key).is_number_unsigned(),"request identity field is not unsigned");
        const auto mask=value.at("mask").get<uint64_t>();require(mask<16,"request identity mask overflow");
        Identity result{value.at("session").get<uint64_t>(),value.at("world").get<uint64_t>(),value.at("requester").get<uint64_t>(),uint8_t(mask),value.at("nonce").get<std::string>()};
        valid(result);require(result.session==session_&&result.world==world_&&result.mask==mask_,"request identity differs from bound world");return result;
    }
    void emit(uint64_t target,const Json& value){require(ports_.control(target,value),"export control queue rejected");progress_=Clock::now();}
};
}
