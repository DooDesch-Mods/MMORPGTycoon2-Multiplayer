#include "subscriber_lifecycle/record_decode.h"
#include <Windows.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace api=mt2mp::subscriber_lifecycle::first_append;
namespace model=mt2mp::subscriber_lifecycle;
namespace {
struct Text {char* data;size_t length;char local[16];};
struct Token {int32_t type,pad;Text text;uint32_t bits,pad2;};
struct Array {uintptr_t ap;void* data;int32_t count,capacity;};
struct Record {Token label;Array tokens,children;uintptr_t last;int32_t a,b;uint8_t stream,pad[7];};
static_assert(sizeof(Record)==0x78&&sizeof(Token)==0x30);
unsigned allocated=0,released=0,checks=0;bool crash_add=false;
void check(bool yes,const char* what){++checks;if(!yes)throw std::runtime_error(what);}
void* allocate(size_t n){++allocated;return std::calloc(1,n);}
void release(void* p){++released;std::free(p);}
void string_construct(void* p,const char* text,size_t size,const void*){auto& t=*static_cast<Text*>(p);t.data=static_cast<char*>(std::malloc(size+1));std::memcpy(t.data,text,size);t.data[size]=0;t.length=size;}
void string_destroy(void* p){std::free(static_cast<Text*>(p)->data);}
void construct(void* p){auto& r=*static_cast<Record*>(p);r={};string_construct(&r.label.text,"",0,nullptr);}
void destroy(void* p){auto& r=*static_cast<Record*>(p);string_destroy(&r.label.text);
    for(int32_t i=0;i<r.tokens.count;++i)string_destroy(&static_cast<Token*>(r.tokens.data)[i].text);
    for(int32_t i=0;i<r.children.count;++i){auto child=static_cast<void**>(r.children.data)[i];destroy(child);release(child);}
    std::free(r.tokens.data);std::free(r.children.data);
}
void assign(void* p,const void* v,int32_t type){auto& t=*static_cast<Token*>(p);const auto& s=*static_cast<const Text*>(v);string_destroy(&t.text);string_construct(&t.text,s.data,s.length,nullptr);t.type=type;}
void label(void* p,const void* s){assign(p,s,1);}
void text(void* p,const void* s){assign(p,s,2);}
void count(void* p,int32_t n){auto& a=static_cast<Record*>(p)->tokens;a.count=a.capacity=n;a.data=std::calloc(size_t(n),sizeof(Token));for(int32_t i=0;i<n;++i)string_construct(&static_cast<Token*>(a.data)[i].text,"",0,nullptr);}
void integer(void* p,int32_t v){auto& t=*static_cast<Token*>(p);t.type=4;std::memcpy(&t.bits,&v,4);}
void type(void* p,int32_t v){static_cast<Token*>(p)->type=v;}
void floating(void* p,float v){auto& t=*static_cast<Token*>(p);t.type=3;std::memcpy(&t.bits,&v,4);}
void add(void* p,const void* c){if(crash_add)RaiseException(0xc0000005,0,0,nullptr);auto& a=static_cast<Record*>(p)->children;
    auto storage=std::realloc(a.data,size_t(a.count+1)*sizeof(void*));if(!storage)throw std::bad_alloc();a.data=storage;static_cast<const void**>(a.data)[a.count++]=c;a.capacity=a.count;
}
class Memory final:public model::Memory {bool read(uintptr_t p,void* out,size_t n,std::string&)override{std::memcpy(out,reinterpret_cast<void*>(p),n);return true;}};
}
int main(){try{
    const api::DecodeApi funcs{allocate,release,construct,destroy,string_destroy,add,label,text,count,integer,type,floating,string_construct};
    Memory memory;std::string error;api::NativeTree tree;api::DecodeReceipt receipt;
    std::vector<model::Record> source{{SIZE_MAX,0,{1,0,"mmoParty"},{}},
        {0,0,{1,0,"linkId"},{{4,1,{}}}},{0,1,{1,0,"ordinary_integer"},{{4,2,{}}}},
        {0,2,{1,0,"native_extra"},{{3,0x3f812345,{}}}},{0,3,{1,0,"members"},{}},
        {4,0,{1,0,"mmoPartyMember"},{}},{5,0,{1,0,"toon"},{{4,2,{}}}},
        {5,1,{1,0,"original_text"},{{2,0,"preserved native text"}}}};
    check(api::decode_party_record(funcs,memory,source,tree,receipt,error),"complete original typed Party Record decode");
    const auto* root=reinterpret_cast<Record*>(tree.root);
    check(std::string(root->label.text.data)=="mmoParty"&&root->children.count==4,"Party native root was relabeled or wrapped as Subscriber");
    check(receipt.allocated==source.size()&&receipt.attached==source.size()-1&&tree.detached.size()==1,"Party native Record ownership differs");
    const auto scalar=static_cast<Record**>(root->children.data)[2];
    check(static_cast<Token*>(scalar->tokens.data)[0].bits==0x3f812345,"Party original float token bits changed");
    const auto members_record=static_cast<Record**>(root->children.data)[3];
    const auto member_record=static_cast<Record**>(members_record->children.data)[0];
    const auto toon_record=static_cast<Record**>(member_record->children.data)[0];
    check(static_cast<Token*>(toon_record->tokens.data)[0].bits==2,"Party original Toon reference token changed");
    const auto text_record=static_cast<Record**>(member_record->children.data)[1];
    check(std::string(static_cast<Token*>(text_record->tokens.data)[0].text.data)=="preserved native text","Party original nested text was omitted");
    check(api::destroy_records(funcs,tree,receipt,error)&&allocated==released,"Party Record cleanup not balanced");
    const auto before=allocated;
    check(!api::validate_records(source,error),"Existing Subscriber root guard widened for Party");
    auto wrong=source;wrong[0].label.text="mmoSubscriber";
    check(!api::decode_party_record(funcs,memory,wrong,tree,receipt,error)&&allocated==before,"Wrong Party root allocated native memory");
    wrong=source;wrong[1].child_index=1;
    check(!api::decode_party_record(funcs,memory,wrong,tree,receipt,error)&&allocated==before,"Incomplete original child order allocated native memory");
    wrong=source;wrong[5].parent=5;
    check(!api::decode_party_record(funcs,memory,wrong,tree,receipt,error)&&allocated==before,"Invalid owned member parent allocated native memory");
    auto missing=funcs;missing.set_float=nullptr;
    check(!api::decode_party_record(missing,memory,source,tree,receipt,error)&&allocated==before,"Incomplete native decoder API allocated native memory");
    source[0].label.text="mmoSubscriber";
    check(api::decode_records(funcs,memory,source,tree,receipt,error),"Existing Subscriber typed decoder failed");
    root=reinterpret_cast<Record*>(tree.root);
    check(std::string(root->label.text.data)=="subscriber"&&root->children.count==1,"Existing Subscriber wrapper was changed");
    check(api::destroy_records(funcs,tree,receipt,error)&&allocated==released,"Existing Subscriber cleanup changed");
    source[0].label.text="mmoParty";crash_add=true;
    check(!api::decode_party_record(funcs,memory,source,tree,receipt,error)&&receipt.unknown&&receipt.exception==0xc0000005,
          "Party original native attach fault lost its Unknown cause");
    const auto released_before=released;
    check(!api::destroy_records(funcs,tree,receipt,error)&&released==released_before,"Unknown Party attachment was destroyed or replayed");
    // This private fixture fault occurs before fake AddChild transfers ownership.
    for(auto pointer:tree.detached){destroy(reinterpret_cast<void*>(pointer));release(reinterpret_cast<void*>(pointer));}
    std::cout<<checks<<" actual Party typed native Record decoder and retained Subscriber guards passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
