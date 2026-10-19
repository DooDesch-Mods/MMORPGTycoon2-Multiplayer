template<size_t N>std::array<float,N> appearance_array(const json& j){
    if(!j.is_array()||j.size()!=N)throw std::runtime_error("region appearance array size differs");
    std::array<float,N> out{};for(size_t i=0;i<N;++i)out[i]=scalar(j[i]);return out;
}
template<size_t N>bool appearance_finite(const std::array<float,N>& a){
    return std::all_of(a.begin(),a.end(),[](float x){return std::isfinite(x);});
}
bool valid_appearance(const RegionAppearance& v){
    return appearance_finite(v.tint)&&appearance_finite(v.light)&&appearance_finite(v.ambient)&&
        appearance_finite(v.sky)&&appearance_finite(v.particle_color)&&v.particles>=0&&v.particles<=6&&
        v.particles_enabled==(v.particles!=0);
}
bool valid_appearance_intent(const RegionPropertyIntent& v){
    if(!appearance_kind(v.kind)||v.id<0||!v.name.empty()||v.type!=-1||
       v.hit!=std::array<float,3>{}||v.levels!=std::array<int32_t,2>{-1,-1}||!appearance_finite(v.appearance_value))return false;
    const size_t used=v.kind==RegionPropertyKind::Sky?16:v.kind==RegionPropertyKind::Particles?1:4;
    for(size_t i=used;i<16;++i)if(v.appearance_value[i]!=0)return false;
    const auto p=v.appearance_value[0];
    return v.kind!=RegionPropertyKind::Particles||(p>=0&&p<=6&&p==std::floor(p));
}
bool appearance_expected(const RegionPropertyIntent& v,const RegionAppearance& before,RegionAppearance& after){
    if(!valid_appearance_intent(v)||!valid_appearance(before))return false;
    after=before;
    switch(v.kind){
    case RegionPropertyKind::Tint:std::copy_n(v.appearance_value.begin(),4,after.tint.begin());break;
    case RegionPropertyKind::Light:std::copy_n(v.appearance_value.begin(),4,after.light.begin());break;
    case RegionPropertyKind::Ambient:std::copy_n(v.appearance_value.begin(),4,after.ambient.begin());break;
    case RegionPropertyKind::Sky:after.sky=v.appearance_value;break;
    case RegionPropertyKind::Particles:after.particles=int32_t(v.appearance_value[0]);after.particles_enabled=after.particles!=0;break;
    case RegionPropertyKind::ParticleColor:std::copy_n(v.appearance_value.begin(),4,after.particle_color.begin());break;
    default:return false;
    }
    return valid_appearance(after)&&after!=before;
}
json appearance_json(const RegionAppearance& v){
    return {{"tint",v.tint},{"light",v.light},{"ambient",v.ambient},{"sky",v.sky},
        {"particles",v.particles},{"particles_enabled",v.particles_enabled},{"particle_color",v.particle_color}};
}
RegionAppearance appearance_read(const json& j){
    exact(j,{"tint","light","ambient","sky","particles","particles_enabled","particle_color"});
    RegionAppearance v;v.tint=appearance_array<4>(j.at("tint"));v.light=appearance_array<4>(j.at("light"));
    v.ambient=appearance_array<4>(j.at("ambient"));v.sky=appearance_array<16>(j.at("sky"));
    v.particle_color=appearance_array<4>(j.at("particle_color"));v.particles=i32(j.at("particles"));
    if(!j.at("particles_enabled").is_boolean())throw std::runtime_error("region particles enabled requires Boolean");
    v.particles_enabled=j.at("particles_enabled").get<bool>();
    if(!valid_appearance(v))throw std::runtime_error("region appearance outside native domain");return v;
}
