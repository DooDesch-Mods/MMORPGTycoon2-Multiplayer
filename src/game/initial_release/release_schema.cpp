#include "release_fixture.h"
#include <set>
namespace mt2mp::game::initial_release {
namespace {
bool uuid(const std::string& s){if(s.size()!=36)return false;for(size_t i=0;i<s.size();++i){if(i==8||i==13||i==18||i==23){if(s[i]!='-')return false;}
    else if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;}return true;}
bool integer(const nlohmann::json& v){if(v.is_number_unsigned())return v.get<uint64_t>()<=2147483647;
    return v.is_number_integer()&&v.get<int64_t>()>=-2147483648LL&&v.get<int64_t>()<=2147483647;}
}
bool validate_fixture_args(const nlohmann::json& args,std::string& e){
    e.clear();auto fail=[&](const char* cause){e=cause;return false;};
    if(!args.is_object()||args.size()!=3||!args.contains("token")||!args.contains("native_origin_uuid")||!args.contains("expected"))return fail("release fixture requires exact token/native_origin_uuid/expected fields");
    for(const char* key:{"token","native_origin_uuid"})if(!args.at(key).is_string()||!uuid(args.at(key).get<std::string>()))return fail("release fixture requires canonical token/Origin UUIDs");
    const auto& s=args.at("expected");if(!s.is_object()||s.size()!=10)return fail("release expected state fields differ");
    for(const char* key:{"version","planned","last_changelog"}){if(!s.contains(key)||!s.at(key).is_array()||s.at(key).size()!=3)return fail("release expected native version requires three int32 fields");
        for(const auto& n:s.at(key))if(!integer(n))return fail("release expected native version requires exact int32");}
    for(const char* key:{"release_type","changes","changelogs","last_changelog_type","server_current","server_target","map_state"})
        if(!s.contains(key)||!integer(s.at(key)))return fail("release expected state requires exact int32 fields");return true;
}
}
