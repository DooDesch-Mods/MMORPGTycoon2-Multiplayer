json tutorial_json(const TutorialIntent& value){
    return {{"asset",value.asset},{"index",value.index},{"step_id",value.step_id},
        {"message_type",value.message_type},{"clear_undo",value.clear_undo}};
}
TutorialIntent tutorial_read(const json& value){
    exact(value,{"asset","index","step_id","message_type","clear_undo"});
    if(!value.at("clear_undo").is_boolean())throw std::runtime_error("Tutorial clear_undo requires boolean");
    TutorialIntent out{value.at("asset").get<std::string>(),value.at("step_id").get<std::string>(),
        i32(value.at("index")),i32(value.at("message_type")),value.at("clear_undo").get<bool>()};
    if(!tutorial_valid(out))throw std::runtime_error("Tutorial acknowledgement identity invalid");
    return out;
}
json tutorial_proof_json(const std::optional<TutorialProof>& value){
    return value?json{{"step",tutorial_json(value->step)},{"granted",value->granted}}:json(nullptr);
}
std::optional<TutorialProof> tutorial_proof_read(const json& value){
    if(value.is_null())return {};
    exact(value,{"step","granted"});
    if(!value.at("granted").is_boolean())throw std::runtime_error("Tutorial grant proof requires boolean");
    return TutorialProof{tutorial_read(value.at("step")),value.at("granted").get<bool>()};
}
