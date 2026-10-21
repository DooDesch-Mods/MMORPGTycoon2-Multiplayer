bool valid_costume_pose(const CostumePose& v){
    for(float x:v.rotation)if(!std::isfinite(x))return false;
    for(float x:v.position)if(!std::isfinite(x))return false;
    for(float x:v.scale)if(!std::isfinite(x))return false;return true;
}
bool valid_costume_record(const CostumeRecord& v){
    if(v.name.size()>4096||v.skeleton.size()>4096||!quest_text(v.name)||!quest_text(v.skeleton)||v.parts.size()>128)return false;
    for(const auto& part:v.parts){
        if(part.bone.size()>4096||part.model.size()>4096||!quest_text(part.bone)||!quest_text(part.model)||!valid_costume_pose(part.pose)||!std::isfinite(part.model_scale)||part.attachments.size()>128||part.colors.size()>128)return false;
        for(const auto& attachment:part.attachments)if(attachment.name.size()>4096||!quest_text(attachment.name)||!valid_costume_pose(attachment.pose))return false;
        for(const auto& color:part.colors)for(float x:color)if(!std::isfinite(x))return false;
    }return true;
}
json costume_pose_json(const CostumePose& v){return {{"rotation",v.rotation},{"position",v.position},{"scale",v.scale}};}
CostumePose costume_pose_read(const json& j){exact(j,{"rotation","position","scale"});CostumePose v;ability_array(j.at("rotation"),v.rotation,scalar);ability_array(j.at("position"),v.position,scalar);ability_array(j.at("scale"),v.scale,scalar);return v;}
json costume_record_json(const CostumeRecord& v){json parts=json::array();for(const auto& p:v.parts){json attachments=json::array();for(const auto& row:p.attachments)attachments.push_back({{"name",row.name},{"pose",costume_pose_json(row.pose)}});
    parts.push_back({{"bone",p.bone},{"model",p.model},{"pose",costume_pose_json(p.pose)},{"model_scale",p.model_scale},{"attachments",attachments},{"colors",p.colors}});}return {{"name",v.name},{"skeleton",v.skeleton},{"parts",parts},{"flag",v.flag}};}
CostumeRecord costume_record_read(const json& j){exact(j,{"name","skeleton","parts","flag"});CostumeRecord v;v.name=j.at("name").get<std::string>();v.skeleton=j.at("skeleton").get<std::string>();v.flag=ability_bool(j.at("flag"));
    const auto& parts=j.at("parts");if(!parts.is_array()||parts.size()>128)throw std::runtime_error("Costume part count exceeds bound");
    for(const auto& row:parts){exact(row,{"bone","model","pose","model_scale","attachments","colors"});CostumePart p;p.bone=row.at("bone").get<std::string>();p.model=row.at("model").get<std::string>();p.pose=costume_pose_read(row.at("pose"));p.model_scale=scalar(row.at("model_scale"));
        const auto& attachments=row.at("attachments");const auto& colors=row.at("colors");if(!attachments.is_array()||attachments.size()>128||!colors.is_array()||colors.size()>128)throw std::runtime_error("Costume attachment/color count exceeds bound");
        for(const auto& a:attachments){exact(a,{"name","pose"});p.attachments.push_back({a.at("name").get<std::string>(),costume_pose_read(a.at("pose"))});}
        for(const auto& color:colors){std::array<float,4> c;ability_array(color,c,scalar);p.colors.push_back(c);}v.parts.push_back(std::move(p));}return v;
}
bool valid_costume_categories(const std::vector<std::string>& values){std::set<std::string> unique;
    if(values.size()>4096)return false;for(const auto& value:values)if(value.empty()||value.size()>4096||!quest_text(value)||!unique.insert(value).second)return false;return true;}
bool valid_costume_edit_intent(const CostumeEditIntent& v){return v.kind>=0&&v.kind<5&&v.id>=0&&v.id<1000000&&digest(v.before_sha256)&&valid_costume_record(v.record)&&
    (!v.weapon_category||(!v.weapon_category->category.empty()&&v.weapon_category->category.size()<=4096&&quest_text(v.weapon_category->category)));}
json costume_edit_intent_json(const CostumeEditIntent& v){json result={{"kind",v.kind},{"id",v.id},{"before_sha256",v.before_sha256},{"record",costume_record_json(v.record)}};
    result["weapon_category"]=v.weapon_category?json{{"category",v.weapon_category->category},{"enabled",v.weapon_category->enabled}}:json(nullptr);return result;}
CostumeEditIntent costume_edit_intent_read(const json& j,bool extended=false){
    if(extended)exact(j,{"kind","id","before_sha256","record","weapon_category"});else exact(j,{"kind","id","before_sha256","record"});
    CostumeEditIntent v{i32(j.at("kind")),i32(j.at("id")),j.at("before_sha256").get<std::string>(),costume_record_read(j.at("record"))};
    if(extended&&!j.at("weapon_category").is_null()){const auto& command=j.at("weapon_category");exact(command,{"category","enabled"});v.weapon_category=CostumeWeaponIntent{command.at("category").get<std::string>(),ability_bool(command.at("enabled"))};}return v;}
bool valid_costume_edit_state(const CostumeEditState& v){return v==CostumeEditState{}||(valid_character_rename_state(v.owner)&&v.owner!=CharacterRenameState{}&&v.count>0&&v.count<=1000000&&digest(v.all_sha256)&&valid_costume_record(v.record)&&valid_costume_categories(v.weapon_categories));}
json costume_edit_state_json(const CostumeEditState& v){if(v==CostumeEditState{})return nullptr;return {{"owner",character_rename_state_json(v.owner)},{"record",costume_record_json(v.record)},{"all_sha256",v.all_sha256},{"count",v.count},{"weapon_categories",v.weapon_categories}};}
CostumeEditState costume_edit_state_read(const json& j,bool extended=false){if(j.is_null())return {};if(extended)exact(j,{"owner","record","all_sha256","count","weapon_categories"});else exact(j,{"owner","record","all_sha256","count"});
    const auto count=u64(j.at("count"));if(count>1000000)throw std::runtime_error("Costume census exceeds bound");CostumeEditState v{character_rename_state_read(j.at("owner")),costume_record_read(j.at("record")),j.at("all_sha256").get<std::string>(),uint32_t(count)};
    if(extended){const auto& values=j.at("weapon_categories");if(!values.is_array()||values.size()>4096)throw std::runtime_error("Costume weapon categories exceed bound");for(const auto& value:values)v.weapon_categories.push_back(value.get<std::string>());}return v;}
