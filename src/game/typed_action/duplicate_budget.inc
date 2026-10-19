bool valid_duplicate(const DuplicateIntent& v){return duplicate_intent_valid(v);}
bool valid_duplicate_effect(const Request& r,const Evidence& e){return duplicate_effect(r,e);}
bool duplicate_wire_fits(const Request& r,const DuplicateState& before,const Ledger& money,std::string& error){
    try{
        Message m;m.kind=Kind::ApplyAck;m.session=UINT64_MAX;m.revision=UINT64_MAX;m.required_mask=15;m.ack_peer=4;m.outcome=Outcome::Confirmed;m.request=r;
        m.request.sequence=UINT64_MAX;m.request.host_revision=UINT64_MAX;m.evidence.before=m.evidence.after=money;m.evidence.duplicate_before=before;
        auto after=before;after.random.fill(UINT64_MAX);int64_t total=0;bool building=false,scenery=false;
        for(const auto& row:r.duplicate.rows){DuplicateObject top;top.family=row.family;top.pose=row.after;top.record_sha=std::string(64,'f');top.region=INT32_MAX;
            const bool group=row.family==DuplicateFamily::Group;top.key=group?"group:"+row.created_keys.front():row.created_keys.front();
            if(group)top.members=row.created_keys;after.created.push_back(top);
            if(group)for(const auto& key_value:row.created_keys){DuplicateObject child;child.key=key_value;child.parent=top.key;child.record_sha=std::string(64,'f');
                child.family=DuplicateFamily::Scenery;child.region=INT32_MAX;
                child.pose={0xbf000000,0xbf000000,0xbf000000,0xbf000000,0xff7fffff,0xff7fffff,0xff7fffff,0x7f7fffff,0x7f7fffff,0x7f7fffff};after.created.push_back(std::move(child));}
            total+=row.cost;building|=row.family==DuplicateFamily::Building;scenery|=row.family==DuplicateFamily::Scenery;
        }
        auto add=[](int32_t& field,int64_t delta){const auto n=int64_t(field)+delta;if(n<INT32_MIN||n>INT32_MAX)throw std::runtime_error("Duplicate original counter/payment overflow");field=int32_t(n);};
        if(building)add(after.release[1],1);if(scenery)add(after.release[2],1);
        if(total){auto& v=m.evidence.after;add(v.cash,-total);add(v.record_cash,-total);add(v.record_expense,total);add(v.line_counts[6],1);add(v.line_income[6],-total);}
        m.evidence.duplicate_after=std::move(after);m.evidence.undo_confirmed=m.evidence.selection_confirmed=m.evidence.retirement_confirmed=true;
        std::vector<uint8_t> bytes;if(!encode(m,bytes,error))return false;
        if(bytes.size()>max_payload-256){error="Duplicate complete result exceeds transport budget before mutation";return false;}return true;
    }catch(const std::exception& ex){error=std::string("Duplicate result preflight: ")+ex.what();return false;}
}
