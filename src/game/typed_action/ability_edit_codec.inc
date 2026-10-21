bool valid_ability_record(const AbilityRecord& v){
    for(const auto& text:v.text)if(text.size()>4096||!quest_text(text))return false;
    if(!std::isfinite(v.item_value))return false;
    for(float value:v.color)if(!std::isfinite(value))return false;
    for(float value:v.timing)if(!std::isfinite(value))return false;
    for(float value:v.final_values)if(!std::isfinite(value))return false;
    if(v.costs.size()>64||v.effects.size()>64)return false;
    for(const auto* list:{&v.costs,&v.effects})for(const auto& effect:*list){
        if(effect.integers[0]<0||effect.integers[0]>=8)return false;
        for(float value:effect.values)if(!std::isfinite(value))return false;}
    return true;
}
json ability_record_json(const AbilityRecord& v){json costs=json::array(),effects=json::array();
    for(size_t n=0;n<2;++n)for(const auto& row:n?v.effects:v.costs)(n?effects:costs).push_back({{"integers",row.integers},{"values",row.values},{"count",row.count}});
    return {{"text",v.text},{"item_value",v.item_value},{"item_kind",v.item_kind},{"item_flag",v.item_flag},
        {"selectors",v.selectors},{"color",v.color},{"timing",v.timing},{"flags",v.flags},{"final_values",v.final_values},
        {"level",v.level},{"final_flag",v.final_flag},{"costs",costs},{"effects",effects}};
}
template<class T,size_t N,class F>void ability_array(const json& j,std::array<T,N>& out,F read){
    if(!j.is_array()||j.size()!=N)throw std::runtime_error("Ability fixed record dimension differs");for(size_t i=0;i<N;++i)out[i]=read(j[i]);}
bool ability_bool(const json& j){if(!j.is_boolean())throw std::runtime_error("Ability flag requires bool");return j.get<bool>();}
AbilityRecord ability_record_read(const json& j){
    exact(j,{"text","item_value","item_kind","item_flag","selectors","color","timing","flags","final_values","level","final_flag","costs","effects"});AbilityRecord v;
    ability_array(j.at("text"),v.text,[](const json& s){return s.get<std::string>();});v.item_value=scalar(j.at("item_value"));v.item_kind=i32(j.at("item_kind"));v.item_flag=ability_bool(j.at("item_flag"));
    ability_array(j.at("selectors"),v.selectors,i32);ability_array(j.at("color"),v.color,scalar);ability_array(j.at("timing"),v.timing,scalar);
    ability_array(j.at("flags"),v.flags,ability_bool);ability_array(j.at("final_values"),v.final_values,scalar);v.level=i32(j.at("level"));v.final_flag=ability_bool(j.at("final_flag"));
    for(size_t n=0;n<2;++n){const auto& list=j.at(n?"effects":"costs");if(!list.is_array()||list.size()>64)throw std::runtime_error("Ability owned effect record exceeds bound");
        for(const auto& row:list){exact(row,{"integers","values","count"});AbilityEffectRecord effect;ability_array(row.at("integers"),effect.integers,i32);ability_array(row.at("values"),effect.values,scalar);effect.count=i32(row.at("count"));(n?v.effects:v.costs).push_back(effect);}}
    return v;
}
bool valid_ability_edit_intent(const AbilityEditIntent& v){return v.kind>=0&&v.kind<5&&v.id>=0&&v.id<1000000&&v.slot>=0&&v.slot<10000&&digest(v.before_sha256)&&valid_ability_record(v.record)&&(v.adjustment?(v.adjustment->index>=0&&v.adjustment->index<64&&std::isfinite(v.adjustment->delta)&&v.adjustment->delta!=0):!v.record.effects.empty());}
json ability_edit_intent_json(const AbilityEditIntent& v){return {{"kind",v.kind},{"id",v.id},{"slot",v.slot},{"before_sha256",v.before_sha256},{"record",ability_record_json(v.record)},{"adjustment",v.adjustment?json{{"cost",v.adjustment->cost},{"index",v.adjustment->index},{"delta",v.adjustment->delta}}:json(nullptr)}};}
AbilityEditIntent ability_edit_intent_read(const json& j,bool extended=false){
    if(extended)exact(j,{"kind","id","slot","before_sha256","record","adjustment"});else exact(j,{"kind","id","slot","before_sha256","record"});
    AbilityEditIntent out;out.kind=i32(j.at("kind"));out.id=i32(j.at("id"));out.slot=i32(j.at("slot"));out.before_sha256=j.at("before_sha256").get<std::string>();out.record=ability_record_read(j.at("record"));
    if(extended&&!j.at("adjustment").is_null()){const auto& a=j.at("adjustment");exact(a,{"cost","index","delta"});out.adjustment=AbilityAdjustment{ability_bool(a.at("cost")),i32(a.at("index")),scalar(a.at("delta"))};}return out;
}
bool valid_ability_edit_state(const AbilityEditState& v){return v==AbilityEditState{}||(valid_character_rename_state(v.owner)&&v.owner!=CharacterRenameState{}&&v.count>0&&v.count<=1000000&&digest(v.all_sha256)&&valid_ability_record(v.record));}
json ability_edit_state_json(const AbilityEditState& v){if(v==AbilityEditState{})return nullptr;return {{"owner",character_rename_state_json(v.owner)},{"record",ability_record_json(v.record)},{"all_sha256",v.all_sha256},{"count",v.count}};}
AbilityEditState ability_edit_state_read(const json& j){if(j.is_null())return {};exact(j,{"owner","record","all_sha256","count"});const auto count=u64(j.at("count"));if(count>1000000)throw std::runtime_error("Ability global census exceeds bound");return {character_rename_state_read(j.at("owner")),ability_record_read(j.at("record")),j.at("all_sha256").get<std::string>(),uint32_t(count)};}
