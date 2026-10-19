#include "character_definition_identity.h"
#include <iostream>
#include <map>
#include <stdexcept>
namespace definitions=mt2mp::game::character_definition_identity;
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;uintptr_t changing=0;unsigned hits=0;
    template<class T>void put(uintptr_t address,const T& value){const auto* source=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=source[i];}
    template<class T>bool read(uintptr_t address,size_t offset,T& value,std::string& error){auto* out=reinterpret_cast<unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i){const auto found=bytes.find(address+offset+i);if(found==bytes.end()){error="original Character definition field read failure";return false;}out[i]=found->second;}
        if(changing==address+offset&&++hits==2)out[0]^=1;return true;}
};
int main()try{
    constexpr uintptr_t image=0x10000000,native_state=0x20000000,manager=0x21000000,main_data=0x22000000,base_type=0x30000000,variant=0x31000000,second=0x32000000;
    constexpr uintptr_t collection=0x40000000,record_data=0x41000000,first_record=0x42000000,second_record=0x43000000;
    constexpr uint64_t base_uid=0x100000012,variant_uid=0x200000012;
    Memory memory;std::string error;size_t checks=0;const auto check=[&](bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause+" comparison="+std::to_string(checks));};
    auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return memory.read(object,offset,value,cause);};
    const auto type=[&](uintptr_t object,int32_t id,uint64_t uid,uintptr_t parent){memory.put(object,image+0x1705990);memory.put(object+0x18,uid);memory.put(object+0x2c,int32_t(1));memory.put(object+0x30,id);
        memory.put(object+0x498,parent?collection:uintptr_t(0x60000000));memory.put(object+0x4a0,parent);memory.put(object+0x2c8,parent?uintptr_t(0):collection);
        for(uintptr_t flag:{0x490,0x491,0x492})memory.put(object+flag,uint8_t(0));};
    const auto setup=[&]{memory={};memory.put(image+0x13df7a0,manager);memory.put(native_state+0x168,manager);memory.put(manager,image+0x1732430);memory.put(collection,image+0x173a6f0);
        for(size_t kind=0;kind<5;++kind){const auto main_array=manager+0x80+kind*0x50;memory.put(main_array+8,kind==1?main_data:uintptr_t(0));memory.put(main_array+0x10,int32_t(kind==1?2:0));memory.put(main_array+0x14,int32_t(kind==1?2:0));
            const auto records_array=collection+0x40+kind*0x30;memory.put(records_array,image+0x16eef80);memory.put(records_array+8,kind==1?record_data:uintptr_t(0));memory.put(records_array+0x10,int32_t(kind==1?1:0));memory.put(records_array+0x14,int32_t(kind==1?1:0));}
        memory.put(main_data,base_type);memory.put(main_data+8,variant);type(base_type,0,base_uid,0);type(variant,1,variant_uid,base_type);
        memory.put(record_data,first_record);memory.put(first_record,image+0x1740fa0);memory.put(first_record+0x18,int32_t(1));memory.put(first_record+0x1c,int32_t(1));};
    definitions::Census census;setup();check(definitions::read(at,image,native_state,manager,census,error),error);
    check(census.definitions.size()==2&&census.collections.size()==1,"Complete base/variant/global/private census lost");const definitions::Definition* owner=nullptr;
    check(definitions::find(census,variant,variant_uid,owner,error)&&owner->key=="character:1:1"&&owner->parent==base_type&&owner->parent_uid==base_uid&&owner->record==first_record&&owner->variant_slot==0,"Variant current global kind/id key or exact parent membership lost");
    check(!definitions::find(census,variant,base_uid,owner,error),"Same low32 UID admitted as complete identity");
    for(int mode=0;mode<16;++mode){setup();
        if(mode==0)memory.put(native_state+0x168,manager+8);if(mode==1)memory.put(manager,image+0x1732431);if(mode==2)memory.put(variant+0x18,base_uid);if(mode==3)memory.put(main_data+8,base_type);
        if(mode==4)memory.put(variant+0x30,int32_t(0));if(mode==5)memory.put(variant+0x4a0,uintptr_t(0xdeadbeef));if(mode==6)memory.put(variant+0x498,collection+8);if(mode==7)memory.put(collection,image+0x173a6f1);
        if(mode==8)memory.put(collection+0x70,image+0x16eef81);if(mode==9)memory.put(first_record,image+0x1740fa1);if(mode==10)memory.put(first_record+0x1c,int32_t(0));if(mode==11)memory.put(first_record+0x18,int32_t(2));
        if(mode==12)memory.put(variant+0x2c8,collection);if(mode==13)memory.put(collection+0x80,int32_t(0));if(mode==14)memory.put(variant+0x490,uint8_t(2));if(mode==15)memory.changing=manager+0xe0;
        error.clear();check(!definitions::read(at,image,native_state,manager,census,error)&&!error.empty()&&census.definitions.empty(),"Foreign/incomplete native definition census accepted mode="+std::to_string(mode));
    }
    setup();memory.bytes.erase(variant+0x18);check(!definitions::read(at,image,native_state,manager,census,error)&&error=="original Character definition field read failure","Original native field read cause replaced");
    setup();memory.put(variant+0x490,uint8_t(1));memory.put(variant+0x18,uint64_t(0));memory.put(collection+0x80,int32_t(0));check(definitions::read(at,image,native_state,manager,census,error),error);
    check(definitions::find(census,variant,0,owner,error)&&owner->deleted&&owner->key=="character:1:1"&&owner->parent_uid==0,"Native destroyed definition lost its global tombstone or assigned a current parent lifetime");
    memory.put(variant+0x498,collection+8);check(definitions::read(at,image,native_state,manager,census,error),"Native UID0 tombstone dereferenced an opaque former Collection");
    memory.put(variant+0x4a0,uintptr_t(0xdeadbeef));check(definitions::read(at,image,native_state,manager,census,error),"Native UID0 tombstone required a destroyed/reused global parent address");
    memory.put(variant+0x2c8,collection);check(!definitions::read(at,image,native_state,manager,census,error),"Native UID0 tombstone retained an impossible private variant Collection");
    setup();memory.put(variant+0x18,uint64_t(0));check(!definitions::read(at,image,native_state,manager,census,error),"Live variant accepted the zero UID of an original destroyed definition");
    setup();memory.put(base_type+0x490,uint8_t(1));memory.put(base_type+0x18,uint64_t(0));memory.put(base_type+0x2c8,uintptr_t(0));
    memory.put(variant+0x490,uint8_t(1));memory.put(variant+0x18,uint64_t(0));
    for(auto it=memory.bytes.begin();it!=memory.bytes.end();){if(it->first>=collection&&it->first<collection+0x1000)it=memory.bytes.erase(it);else ++it;}
    check(definitions::read(at,image,native_state,manager,census,error),error);
    check(census.collections.empty()&&census.definitions.size()==2&&definitions::find(census,variant,0,owner,error)&&owner->parent_uid==0,"Original base destruction lost two zero-UID global tombstones or dereferenced the freed private Collection");
    memory.put(variant+0x490,uint8_t(0));memory.put(variant+0x18,variant_uid);check(!definitions::read(at,image,native_state,manager,census,error)&&error.find("destroyed base")!=std::string::npos,"Live variant admitted after its base Collection was destroyed");
    setup();memory.put(manager+0xe0,int32_t(3));memory.put(manager+0xe4,int32_t(3));memory.put(main_data+16,second);type(second,2,0x300000012,base_type);
    memory.put(collection+0x80,int32_t(2));memory.put(collection+0x84,int32_t(2));memory.put(record_data+8,second_record);memory.put(second_record,image+0x1740fa0);memory.put(second_record+0x18,int32_t(1));memory.put(second_record+0x1c,int32_t(2));
    check(definitions::read(at,image,native_state,manager,census,error),error);check(definitions::find(census,variant,variant_uid,owner,error)&&owner->variant_slot==0,error);
    memory.put(record_data,second_record);memory.put(record_data+8,first_record);check(definitions::read(at,image,native_state,manager,census,error),error);
    check(definitions::find(census,variant,variant_uid,owner,error)&&owner->key=="character:1:1"&&owner->variant_slot==1,"Private collection ordinal changed canonical global identity");
    setup();memory.put(variant+0x490,uint8_t(1));memory.put(variant+0x18,uint64_t(0));memory.put(variant+0x498,uintptr_t(0xdeadbeef));
    memory.put(main_data,second);type(second,0,0x900000012,0);memory.put(second+0x2c8,uintptr_t(0));
    check(definitions::read(at,image,native_state,manager,census,error),"Native reused first global tombstone slot kept a false old parent dependency");
    check(definitions::find(census,second,0x900000012,owner,error)&&owner->key=="character:1:0","Fresh native slot reuse lost its new full UID/global identity");
    memory.put(variant+0x490,uint8_t(0));memory.put(variant+0x18,variant_uid);
    check(!definitions::read(at,image,native_state,manager,census,error),"Live variant was allowed to use a freed old parent after slot reuse");
    std::cout<<checks<<" Character base/variant global ID/full UID/private Collection/retirement comparisons passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
