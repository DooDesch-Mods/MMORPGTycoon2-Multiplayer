bool valid_group_history_effect(const Request& r,const Evidence& e){return valid_request(r)&&group_history_effect(r,e);}
bool group_history_wire_fits(const Request& r,const GroupState& before,const GroupState& projected,const Ledger& money,std::string& error){
    try{Message m;m.kind=Kind::ApplyAck;m.session=UINT64_MAX;m.revision=UINT64_MAX;m.required_mask=15;m.ack_peer=4;m.outcome=Outcome::Confirmed;m.request=r;
        m.request.sequence=UINT64_MAX;m.request.host_revision=UINT64_MAX;m.evidence.before=m.evidence.after=money;
        m.evidence.retirement_confirmed=m.evidence.undo_confirmed=true;m.evidence.group_before=before;m.evidence.group_after=projected;
        for(auto& group:m.evidence.group_after->groups){
            const auto prior=std::find_if(before.groups.begin(),before.groups.end(),[&](const auto& row){return row.key==group.key;});
            if(prior==before.groups.end()||group.members.size()>prior->members.size()){
                group.pose.fill(0xff7fffff);for(auto& member:group.members)member.relative.fill(0xff7fffff);}}
        std::vector<uint8_t> bytes;if(!encode(m,bytes,error))return false;
        if(bytes.size()>max_payload-256){error="Group history complete result exceeds transport budget before original graph";return false;}return true;
    }catch(const std::exception& ex){error=std::string("Group history result preflight: ")+ex.what();return false;}
}
