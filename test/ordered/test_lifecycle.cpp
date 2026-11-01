#include "ordered_proof_owner.h"
#include <iostream>
#include <stdexcept>
using mt2mp::game::ordered_session::ProofOwner;
int checks=0;
void need(bool ok,const char* cause){++checks;if(!ok)throw std::runtime_error(cause);}
struct Port {
    MT2FW_MT2_StepProofReply proof{};
    MT2FW_MT2_SessionProvider provider=nullptr;
    void* user=nullptr;
    int binds=0,unbinds=0;bool busy=false,bad_receipt=false;
    Port(){
        proof.size=sizeof(proof);proof.session=17;proof.thread=13;proof.generation=9;proof.snapshot_bytes=100;
        proof.seed_present=1;proof.frozen_peer_mask=3;proof.phase=MT2FW_MT2_STEP_PROOF_PRELOAD;
        strcpy_s(proof.snapshot_uuid,"11111111-1111-4111-8111-111111111111");
        proof.region_rng_mode=1;proof.region_rng_index=UINT32_MAX;proof.region_rng_sha256[0]=1;
    }
    bool read(MT2FW_MT2_StepProofReply& out,uint32_t& role,std::string&){out=proof;role=MT2FW_MT2_SESSION_HOST;return true;}
    MT2FW_MT2_PatchesV1 service(){
        MT2FW_MT2_PatchesV1 out{};out.size=sizeof(out);out.version=MT2FW_MT2_PATCHES_VERSION;out.context=this;
        out.bind_session_provider=[](void* raw,MT2FW_MT2_SessionProvider fn,void* user,uint64_t* token,MT2FW_MT2_PatchReceipt*){
            auto& self=*static_cast<Port*>(raw);++self.binds;self.provider=fn;self.user=user;*token=42;return MT2FW_MT2_OK;};
        out.unbind_session_provider=[](void* raw,uint64_t token,MT2FW_MT2_PatchReceipt* receipt){
            auto& self=*static_cast<Port*>(raw);++self.unbinds;need(token==42,"retirement token changed");
            if(self.busy){strcpy_s(receipt->cause,"original provider BUSY");return MT2FW_MT2_BUSY;}
            receipt->size=self.bad_receipt?0:sizeof(*receipt);receipt->result=MT2FW_MT2_OK;
            return MT2FW_MT2_OK;};return out;
    }
};
int main()try{
    for(int cycle=0;cycle<2;++cycle){
        Port port;port.proof.session+=uint64_t(cycle);ProofOwner owner([&](auto& out,auto& role,auto& e){return port.read(out,role,e);});std::string e;
        need(!owner.retired_identity(port.proof.session,port.proof.snapshot_uuid,9,e),"unbound owner invented retirement");
        need(owner.bind(port.service(),e)&&owner.token()==42&&port.binds==1,"provider registration failed");
        need(!owner.bind(port.service(),e)&&port.binds==1,"duplicate bind replayed registration");
        MT2FW_MT2_StepProofQuery query{};query.size=sizeof(query);query.version=MT2FW_MT2_STEP_DRAFT_VERSION;
        query.session=port.proof.session;query.thread=13;query.session_provider_token=42;query.region_rng_index=UINT32_MAX;
        auto call=[&]{MT2FW_MT2_StepProofReply out{};out.size=sizeof(out);return ProofOwner::callback()(&owner,&query,&out);};
        need(call()==MT2FW_MT2_OK,"current registered proof unavailable");
        ++query.session;need(call()==MT2FW_MT2_UNAVAILABLE,"foreign session admitted");--query.session;
        ++port.proof.generation;need(call()==MT2FW_MT2_UNAVAILABLE,"changed generation admitted");--port.proof.generation;
        port.busy=true;need(!owner.unbind(e)&&e.find("original provider BUSY")!=std::string::npos&&owner.token()==42,"BUSY dropped owner or original cause");
        need(call()==MT2FW_MT2_UNAVAILABLE,"retiring owner allowed callback");
        need(!owner.retired_identity(port.proof.session,port.proof.snapshot_uuid,9,e),"BUSY invented retirement receipt");
        port.busy=false;port.bad_receipt=true;need(!owner.unbind(e)&&owner.token()==42,"invalid receipt dropped owner");
        port.bad_receipt=false;need(owner.unbind(e)&&!owner.token(),"confirmed retirement retained token");
        need(owner.retired_identity(port.proof.session,port.proof.snapshot_uuid,9,e),"exact retirement identity unavailable");
        need(!owner.retired_identity(port.proof.session+1,port.proof.snapshot_uuid,9,e),"retirement accepted another session");
        const auto calls=port.unbinds;need(owner.unbind(e)&&port.unbinds==calls,"retirement repeated native unbind");
    }
    std::cout<<checks<<" ordered owner lifecycle checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
