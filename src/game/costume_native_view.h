#pragma once
#include "typed_action/costume_edit_action.h"
#include <cstring>
#include <vector>
namespace mt2mp::game::costume_native_view {
struct Borrowed {
    std::array<unsigned char,0xc8> costume{};
    std::vector<std::array<unsigned char,0x110>> parts;
    std::vector<uintptr_t> part_pointers;
    std::vector<std::vector<std::array<unsigned char,0xa8>>> attachments;
    std::vector<std::vector<uintptr_t>> attachment_pointers;
    template<class T>static void put(unsigned char* base,size_t offset,const T& value){std::memcpy(base+offset,&value,sizeof value);}
    static void text(unsigned char* base,size_t offset,const std::string& value){const auto pointer=value.c_str();const auto length=value.size();put(base,offset,pointer);put(base,offset+8,length);put(base,offset+16,length);}
    static void array(unsigned char* base,size_t offset,uintptr_t pointer,int32_t count){put(base,offset,pointer);put(base,offset+8,count);put(base,offset+12,count);}
    template<class Transform>void build(uintptr_t image,const action_session::CostumeRecord& value,Transform& transform){
        costume.fill(0);parts.resize(value.parts.size());part_pointers.resize(parts.size());attachments.resize(parts.size());attachment_pointers.resize(parts.size());
        put(costume.data(),0,image+0x16c0210);text(costume.data(),0x18,value.name);text(costume.data(),0x38,value.skeleton);costume[0xc0]=value.flag?1:0;
        const float scale=1;put(costume.data(),0xbc,scale);
        for(size_t i=0;i<parts.size();++i){auto* part=parts[i].data();std::memset(part,0,parts[i].size());const auto& source=value.parts[i];
            put(part,0,image+0x1736c70);text(part,0x18,source.bone);text(part,0x38,source.model);transform(part+0x58,source.pose);put(part,0xc4,source.model_scale);
            attachments[i].resize(source.attachments.size());attachment_pointers[i].resize(source.attachments.size());
            for(size_t n=0;n<source.attachments.size();++n){auto* attachment=attachments[i][n].data();std::memset(attachment,0,attachments[i][n].size());put(attachment,0,image+0x17229b0);text(attachment,0x18,source.attachments[n].name);transform(attachment+0x38,source.attachments[n].pose);attachment_pointers[i][n]=uintptr_t(attachment);}
            array(part,0xd0,uintptr_t(attachment_pointers[i].data()),int32_t(attachment_pointers[i].size()));array(part,0x100,uintptr_t(source.colors.data()),int32_t(source.colors.size()));part_pointers[i]=uintptr_t(part);}
        array(costume.data(),0x60,uintptr_t(part_pointers.data()),int32_t(part_pointers.size()));
    }
};
}
