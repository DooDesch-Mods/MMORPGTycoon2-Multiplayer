bool valid_duplicate_history_effect(const Request& r,const Evidence& e){return valid_request(r)&&duplicate_history_effect(r,e);}
bool duplicate_history_wire_fits(const Request& r,const DuplicateState& before,const DuplicateState& after,
                                 const std::vector<DuplicateCash>& cash,const Ledger& money,std::string& error){
    try{Message m;m.kind=Kind::ApplyAck;m.session=UINT64_MAX;m.revision=UINT64_MAX;m.required_mask=15;m.ack_peer=4;m.outcome=Outcome::Confirmed;m.request=r;
        m.request.sequence=UINT64_MAX;m.request.host_revision=UINT64_MAX;m.evidence.before=m.evidence.after=money;
        m.evidence.retirement_confirmed=m.evidence.undo_confirmed=true;m.evidence.duplicate_before=before;m.evidence.duplicate_after=after;m.evidence.duplicate_cash=cash;
        auto add=[&](int32_t& value,int32_t amount){const auto next=int64_t(value)+amount;if(next<INT32_MIN||next>INT32_MAX)throw std::runtime_error("Duplicate history cash overflow");value=int32_t(next);};
        for(const auto& row:cash){auto& v=m.evidence.after;add(v.cash,row.amount);add(v.record_cash,row.amount);add(v.record_income,row.amount);add(v.line_counts[6],1);add(v.line_income[6],row.amount);}
        std::vector<uint8_t> bytes;if(!encode(m,bytes,error))return false;
        // Restored native pose/Region integers can use more JSON digits than
        // the previous live sample. Keys, member lists and SHA lengths stay
        // fixed. Reserve the full decimal width for all eleven row scalars.
        const size_t reserve=1024+after.created.size()*121;
        if(reserve>max_payload||bytes.size()>max_payload-reserve){error="Duplicate history complete result exceeds transport budget before original graph";return false;}return true;
    }catch(const std::exception& ex){error=std::string("Duplicate history result preflight: ")+ex.what();return false;}
}
