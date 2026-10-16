namespace bugfix=game::bug_authority;
bool bug_fix_intent_valid(const BugFixIntent& intent){
    std::string error;
    return bugfix::work_codec::identity(intent.bug,error)&&
        (intent.schedule?!intent.expected_job:(intent.expected_job&&bugfix::work_codec::birth(*intent.expected_job,error)));
}
json bug_fix_birth_json(const bugfix::Birth& b){return json::array({int(b.kind),b.source,b.sequence,b.occurrence});}
bugfix::Birth bug_fix_birth_read(const json& j){
    if(!j.is_array()||j.size()!=4)throw std::runtime_error("Bug Fix Job birth requires exact4 fields");
    bugfix::Birth b{static_cast<bugfix::BirthKind>(i32(j[0])),j[1].get<std::string>(),bugfix::codec_detail::u64(j[2]),bugfix::codec_detail::u64(j[3])};
    std::string error;if(!bugfix::work_codec::birth(b,error))throw std::runtime_error(error);return b;
}
json bug_fix_json(const BugFixIntent& intent){
    const auto& b=intent.bug.birth;
    return {{"bug",json::array({int(intent.bug.parent_kind),intent.bug.parent_key,int(b.kind),b.source,b.sequence,b.occurrence})},
        {"schedule",intent.schedule},{"expected_job",intent.expected_job?bug_fix_birth_json(*intent.expected_job):json(nullptr)}};
}
BugFixIntent bug_fix_read(const json& j){
    exact(j,{"bug","schedule","expected_job"});const auto& row=j.at("bug");
    if(!row.is_array()||row.size()!=6)throw std::runtime_error("Bug Fix identity requires exact6 fields");
    BugFixIntent out;out.bug={static_cast<bugfix::ParentKind>(i32(row[0])),row[1].get<std::string>(),
        {static_cast<bugfix::BirthKind>(i32(row[2])),row[3].get<std::string>(),bugfix::codec_detail::u64(row[4]),bugfix::codec_detail::u64(row[5])}};
    out.schedule=bugfix::codec_detail::boolean(j.at("schedule"));
    if(!j.at("expected_job").is_null())out.expected_job=bug_fix_birth_read(j.at("expected_job"));
    if(!bug_fix_intent_valid(out))throw std::runtime_error("Bug Fix identity/schedule/Job intent invalid");return out;
}
json bug_fix_row_json(const std::optional<bugfix::Row>& row){return row?bugfix::encode(bugfix::State{{*row}}).at(0):json(nullptr);}
void bug_fix_row_read(const json& j,std::optional<bugfix::Row>& out){
    if(j.is_null())return;bugfix::State state;std::string error;
    if(!bugfix::decode(json::array({j}),state,error))throw std::runtime_error(error);out=state.rows.front();
}
bool bug_fix_effect(const Request& request,const Evidence& evidence){
    const auto& intent=request.bug_fix;
    if(!evidence.bug_before||!evidence.bug_after||!evidence.developers_before||!evidence.developers_after)return false;
    Evidence allowed;allowed.before=evidence.before;allowed.after=evidence.after;allowed.retirement_confirmed=evidence.retirement_confirmed;
    allowed.bug_before=evidence.bug_before;allowed.bug_after=evidence.bug_after;
    allowed.developers_before=evidence.developers_before;allowed.developers_after=evidence.developers_after;
    if(allowed!=evidence)return false;
    std::string error;
    if(!bugfix::valid(bugfix::State{{*evidence.bug_before}},error)||!bugfix::valid(bugfix::State{{*evidence.bug_after}},error))return false;
    const auto& before=*evidence.bug_before;auto expected=before;expected.value.manual=true;
    if(!before.value.known||expected!=*evidence.bug_after||
       bugfix::BugIdentity{before.parent_kind,before.parent_key,before.birth}!=intent.bug)return false;
    const auto& pool=*evidence.developers_before;const auto& after=*evidence.developers_after;
    if(!game::developer_staff::valid(pool,error)||!game::developer_staff::valid(after,error)||
       !pool.construction||!after.construction||!pool.construction->work||!after.construction->work)return false;
    auto expected_pool=pool;expected_pool.construction->work=after.construction->work;
    if(expected_pool!=after)return false;
    const auto& old=*pool.construction->work;const auto& next=*after.construction->work;
    auto created=old.created;
    if(intent.schedule){
        if(created.value.today==INT32_MAX||created.value.total==INT32_MAX)return false;
        ++created.value.today;++created.value.total;created.highest=std::max(created.highest,created.value.total);
    }
    if(created!=next.created||old.fixed!=next.fixed)return false;
    auto jobs=old.jobs;
    if(intent.schedule){
        for(const auto& job:jobs)if(job.bug==intent.bug&&job.type==7&&job.valid)return false;
        if(next.jobs.size()!=jobs.size()+1)return false;
        const auto& added=next.jobs.back();
        if(added.bug!=intent.bug||added.birth!=bugfix::Birth{bugfix::BirthKind::Action,request.action_uuid,0,1}||
           added.type!=7||!added.valid||added.developer_id!=-1)return false;
        jobs.push_back(added);
    }else{
        if(!intent.expected_job)return false;
        auto found=std::find_if(jobs.begin(),jobs.end(),[&](const auto& job){return job.birth==*intent.expected_job;});
        if(found==jobs.end()||found->bug!=intent.bug||found->type!=7||!found->valid)return false;
        if(found->developer_id<0)jobs.erase(found);else{found->type=0;found->valid=false;}
    }
    if(jobs!=next.jobs)return false;
    const auto unrelated=[&](const auto& parents){auto out=parents;
        std::erase_if(out,[&](const auto& p){return p.kind==intent.bug.parent_kind&&p.key==intent.bug.parent_key;});return out;};
    if(unrelated(old.parents)!=unrelated(next.parents))return false;
    const auto selected=[&](const auto& p){return p.kind==intent.bug.parent_kind&&p.key==intent.bug.parent_key;};
    const auto prior=std::find_if(old.parents.begin(),old.parents.end(),selected);
    const auto current=std::find_if(next.parents.begin(),next.parents.end(),selected);
    if(prior!=old.parents.end()&&current!=next.parents.end()&&*prior!=*current)return false;
    if(intent.schedule){if(current==next.parents.end())return false;}
    else if(prior==old.parents.end()||old.parents!=next.parents)return false;
    return evidence.before==evidence.after&&!evidence.target_retired&&!evidence.undo_confirmed&&!evidence.selection_confirmed&&
        evidence.object_before==BuildingState{}&&evidence.object_after==BuildingState{};
}
