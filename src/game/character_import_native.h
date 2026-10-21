#pragma once
#include "character_import_payload.h"
#include <array>
namespace mt2mp::game::character_import_native {
struct Receipt {
    bool decoder_attempted=false,decoder_returned=false,decoder_cleaned=false;
    bool payload_complete=false,image_constructed=false,image_encoded=false,image_roundtrip=false,image_cleaned=false;
    bool borrowed_constructed=false,borrowed_cleaned=false;
    bool unknown=false;uint32_t exception=0;std::string cause;
};
struct Image {alignas(8) std::array<unsigned char,0x20> storage{};bool constructed=false,unknown=false;};
bool verify(uintptr_t image,subscriber_lifecycle::Memory&,std::string&);
bool read(uintptr_t image,subscriber_lifecycle::Memory&,uintptr_t original_image,std::string& compressed_hex,Receipt&,std::string&);
bool validate(uintptr_t image,subscriber_lifecycle::Memory&,const std::string&,Receipt&,std::string&);
bool create(uintptr_t image,subscriber_lifecycle::Memory&,const std::string&,Image&,Receipt&,std::string&);
bool discard(uintptr_t image,Image&,Receipt&,std::string&);
}
