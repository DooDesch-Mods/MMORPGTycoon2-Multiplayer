namespace {
namespace weapon_records=mt2mp::subscriber_lifecycle::first_append;
using WeaponToon=mt2mp::subscriber_quest_state::Toon;
class WeaponRecordMemory final:public mt2mp::subscriber_lifecycle::Memory {
public:
    bool read(uintptr_t address,void* out,size_t size,std::string& error)override{return memory.copy(address,out,size,error);}
};
bool same_weapon(const WeaponToon& a,const WeaponToon& b){
    return a.weapon_ilvl==b.weapon_ilvl&&a.weapon_category==b.weapon_category&&a.weapon_variant_id==b.weapon_variant_id;
}
bool read_toon_weapon(uintptr_t toon,WeaponToon& out,std::string& error){
    return at(toon,0x4b0,out.weapon_ilvl,error)&&text(toon,0x4b8,max_frame_payload,out.weapon_category,error)&&
           at(toon,0x4d8,out.weapon_variant_id,error);
}
bool weapon_property_api(uintptr_t image,std::string& error){
    struct Property {uintptr_t rva,ap,load,offset;};
    constexpr Property properties[]={{0x13c8b40,0x16d64d0,0xd2c7b0,0x4b0},
        {0x13c8b00,0x16cb410,0xd198b0,0x4b8},{0x13c8ac0,0x16d64d0,0xd2c7b0,0x4d8}};
    for(const auto& property:properties){
        uintptr_t address_point=0,load=0,offset=0;
        if(!at(image,property.rva,address_point,error)||!at(address_point,0x10,load,error)||
           !at(image,property.rva+0x38,offset,error))return false;
        if(address_point!=image+property.ap||load!=image+property.load||offset!=property.offset)
            return fail(error,"Toon weapon scalar property class/Load/offset differs RVA="+std::to_string(property.rva));
    }
    struct Code {uintptr_t rva;std::array<unsigned char,16> bytes;};
    constexpr Code code[]={
        {0xd2c7b0,{0x48,0x89,0xc8,0x4c,0x89,0xc1,0x4d,0x89,0xc8,0x48,0x03,0x50,0x38,0xe9,0x3e,0x34}},
        {0xd198b0,{0x48,0x89,0xc8,0x4c,0x89,0xc1,0x4d,0x89,0xc8,0x48,0x03,0x50,0x38,0xe9,0xde,0x8c}},
        {0x89fc00,{0x53,0x48,0x83,0xec,0x20,0x31,0xc0,0x48,0x89,0xd3,0x8b,0x51,0x40,0x85,0xd2,0x7f}},
        {0x8a25a0,{0x53,0x48,0x83,0xec,0x20,0x31,0xc0,0x48,0x89,0xd3,0x8b,0x51,0x40,0x85,0xd2,0x7f}},
    };
    for(const auto& expected:code){std::array<unsigned char,16> actual{};
        if(!memory.copy(image+expected.rva,actual.data(),actual.size(),error))return false;
        if(actual!=expected.bytes)return fail(error,"Toon weapon original scalar body differs RVA="+std::to_string(expected.rva));
    }
    return true;
}
bool store_toon_weapon(const Pin& pin,uintptr_t toon,const WeaponToon& prior,const WeaponToon& target,
                       const std::function<bool(std::string&)>& fence,std::string& error){
    if(same_weapon(prior,target))return true;
    if(!weapon_property_api(pin.image,error))return false;
    weapon_records::DecodeApi api;WeaponRecordMemory record_memory;
    if(!weapon_records::resolve_record_api(record_memory,pin.image,api,error))return false;
    std::array<weapon_records::NativeTree,3> trees;
    std::array<weapon_records::DecodeReceipt,3> records;
    const bool changed[]={prior.weapon_ilvl!=target.weapon_ilvl,prior.weapon_category!=target.weapon_category,
                          prior.weapon_variant_id!=target.weapon_variant_id};
    const auto cleanup=[&](std::string& cause){
        bool okay=true;
        for(size_t i=0;i<trees.size();++i){
            if(!trees[i].root&&trees[i].detached.empty())continue;
            if(records[i].unknown){cause+=(cause.empty()?"":"; ")+std::string("Toon weapon Record outcome unknown; native owner retained field=")+std::to_string(i);okay=false;continue;}
            std::string failure;
            if(!weapon_records::destroy_records(api,trees[i],records[i],failure)){
                cause+=(cause.empty()?"":"; ")+std::string("Toon weapon Record cleanup field=")+std::to_string(i)+" original="+failure;okay=false;
            }
        }
        return okay;
    };
    bool prepared=true;
    if(changed[0])prepared=weapon_records::decode_integer_record(api,record_memory,"weaponIlvl",target.weapon_ilvl,trees[0],records[0],error);
    if(prepared&&changed[1])prepared=weapon_records::decode_string_record(api,record_memory,"weaponCategory",target.weapon_category,trees[1],records[1],error);
    if(prepared&&changed[2])prepared=weapon_records::decode_integer_record(api,record_memory,"weaponVariantId",target.weapon_variant_id,trees[2],records[2],error);
    if(!prepared){cleanup(error);return false;}
    WeaponToon actual;
    if(!fence(error)||!read_toon_weapon(toon,actual,error)||!same_weapon(actual,prior)){
        if(error.empty())error="Toon weapon prior tuple changed before scalar Property.Load";
        cleanup(error);return false;
    }
    constexpr uintptr_t properties[]={0x13c8b40,0x13c8b00,0x13c8ac0};
    for(size_t i=0;i<trees.size();++i){
        if(!changed[i])continue;
        if(!fence(error)){cleanup(error);return false;}
        mt2mp::subscriber_lifecycle::CallReceipt receipt;
        const auto load=reinterpret_cast<mt2mp::subscriber_lifecycle::PropertyLoad>(pin.image+(i==1?0xd198b0:0xd2c7b0));
        if(!mt2mp::subscriber_lifecycle::load_scalar_property(load,reinterpret_cast<void*>(pin.image+properties[i]),
            reinterpret_cast<void*>(toon),reinterpret_cast<void*>(trees[i].root),receipt,error)){
            if(receipt.attempted&&!receipt.returned)records[i].unknown=true;
            error="Toon weapon scalar Property.Load field="+std::to_string(i)+" original="+error;
            cleanup(error);return false;
        }
    }
    if(!cleanup(error))return false;
    if(!fence(error)||!read_toon_weapon(toon,actual,error))return false;
    if(!same_weapon(actual,target))return fail(error,"Toon weapon complete tuple readback differs after scalar Property.Load");
    return true;
}
}
