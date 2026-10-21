#pragma once
#include "record_graph.h"
#include "release_record_kind.h"
#include <array>

namespace mt2mp::subscriber_lifecycle::first_append {
using Allocate = void*(*)(size_t);
using Release = void(*)(void*);
using Unary = void(*)(void*);
using Binary = void(*)(void*,const void*);
using Integer = void(*)(void*,int32_t);
using Float = void(*)(void*,float);
using StringConstruct = void(*)(void*,const char*,size_t,const void*);
struct DecodeApi {
    Allocate allocate=nullptr;
    Release release=nullptr;
    Unary record_construct=nullptr,record_destroy=nullptr,string_destroy=nullptr;
    Binary add_child=nullptr,set_label=nullptr,set_string=nullptr;
    Integer token_count=nullptr,set_integer=nullptr,set_type=nullptr;
    Float set_float=nullptr;
    StringConstruct string_construct=nullptr;
};
struct DecodeReceipt {
    size_t allocated=0,constructed=0,attached=0,tokens_written=0;
    bool complete=false,cleanup_attempted=false,cleaned=false,unknown=false;
    uint32_t exception=0;
    std::string cause;
};
// Native owners and addresses remain private and never enter a transport DTO.
struct NativeTree { uintptr_t root=0; std::vector<uintptr_t> detached; };
bool validate_records(const std::vector<Record>&,std::string&);
bool decode_records(const DecodeApi&,Memory&,const std::vector<Record>&,NativeTree&,DecodeReceipt&,std::string&);
bool decode_party_record(const DecodeApi&,Memory&,const std::vector<Record>&,NativeTree&,DecodeReceipt&,std::string&);
enum class MarketingRecordKind { Award, Campaign };
// Fresh typed object loads consume the object Record itself, without an array wrapper.
bool decode_marketing_record(const DecodeApi&,Memory&,MarketingRecordKind,const std::vector<Record>&,NativeTree&,DecodeReceipt&,std::string&);
bool decode_logo_record(const DecodeApi&,Memory&,const std::vector<Record>&,NativeTree&,DecodeReceipt&,std::string&);
bool decode_modular_record(const DecodeApi&,Memory&,bool plan,const std::vector<Record>&,NativeTree&,DecodeReceipt&,std::string&);
bool decode_dungeon_fill_record(const DecodeApi&,Memory&,const std::vector<Record>&,NativeTree&,DecodeReceipt&,std::string&);
bool decode_release_record(const DecodeApi&,Memory&,const std::vector<Record>&,NativeTree&,DecodeReceipt&,std::string&,ReleaseRecordKind kind=ReleaseRecordKind::Manager);
bool decode_integer_record(const DecodeApi&,Memory&,const char*,int32_t,NativeTree&,DecodeReceipt&,std::string&);
bool decode_string_record(const DecodeApi&,Memory&,const char*,const std::string&,NativeTree&,DecodeReceipt&,std::string&);
bool destroy_records(const DecodeApi&,NativeTree&,DecodeReceipt&,std::string&);
}
