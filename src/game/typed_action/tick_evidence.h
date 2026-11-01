#pragma once
#include "tick_execution.h"
#include "mt2mp/core/hash.h"

namespace mt2mp::action_session::tick_commands {
inline bool canonical_evidence(const Context& context,const Assigned& command,const ExecutionReceipt& receipt,std::string& digest,std::string& error){
    digest.clear();
    if(command.input.session!=context.session||command.input.snapshot_uuid!=context.snapshot_uuid||
       !requires_evidence(command.input)||receipt.command!=command||receipt.outcome!=Outcome::Confirmed||!receipt.evidence){
        error="Canonical evidence requires the exact confirmed command execution";return false;
    }
    auto message=std::make_unique<Message>();message->kind=Kind::HostResult;
    message->session=context.session;message->revision=command.order;message->required_mask=context.peer_mask;
    message->request=std::get<Request>(command.input.command);message->outcome=Outcome::Confirmed;message->evidence=*receipt.evidence;
    std::vector<uint8_t> bytes;
    if(!action_session::encode(*message,bytes,error))return false;
    // The existing strict codec owns canonical field order and domain checks.
    // Its round trip must retain the entire evidence, including unused fields.
    auto decoded=std::make_unique<Message>();
    if(!action_session::decode(bytes.data(),bytes.size(),*decoded,error))return false;
    if(*decoded!=*message){error="Canonical evidence codec omitted or changed a field";return false;}
    try{digest=mt2mp::sha256(bytes);}catch(const std::exception& cause){error=cause.what();return false;}
    if(!evidence_digest(digest)){error="Canonical evidence SHA256 invalid";return false;}
    error.clear();return true;
}
}
