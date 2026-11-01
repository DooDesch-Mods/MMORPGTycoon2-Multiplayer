bool read_process_cohort(ProcessCohort& out,std::string& error){
    out={};
    if(local()){
        if(!read_local_process_cohort(out.process_nonces,out.session,out.lobby,out.local,error))return false;
        for(size_t slot=1;slot<out.physical_ids.size();++slot)if(out.process_nonces[slot])out.physical_ids[slot]=slot;
        return true;
    }
    try{
        if(!binding(error,false)||!connected()||!local_instance||own<1||own>4||!ids[own]){
            if(error.empty())error="Recovery process observation requires the original acknowledged Steam transport";
            return false;
        }
        out.session=current_session;out.lobby=lobby;out.local=own;
        for(size_t slot=1;slot<ids.size();++slot)if(ids[slot]&&
            std::find(observed_slots.begin()+1,observed_slots.end(),ids[slot])!=observed_slots.end()){
            const auto process=slot==own?local_instance:remote_instances[slot];
            if(!process){error="Recovery process observation lacks a physical handshake process nonce";return false;}
            out.physical_ids[slot]=ids[slot];out.process_nonces[slot]=process;
        }
        error.clear();return true;
    }catch(const std::exception& cause){error=std::string("Recovery process observation: ")+cause.what();return false;}
}
