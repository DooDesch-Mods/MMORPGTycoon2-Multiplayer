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
    Memory memory;std::string e;
    for(const auto kind:{api::MarketingRecordKind::Award,api::MarketingRecordKind::Campaign}){
        const char* name=kind==api::MarketingRecordKind::Award?"mmoAward":"mmoAdCampaign";
        std::vector<model::Record> source{{SIZE_MAX,0,{1,0,name},{}},{0,0,{1,0,"name"},{{2,0,"{native_key}"}}},{0,1,{1,0,"day"},{{4,2,{}}}}};
        api::NativeTree tree;api::DecodeReceipt receipt;
        check(api::decode_marketing_record(funcs,memory,kind,source,tree,receipt,e),"fresh typed Record decode");
        const auto* root=reinterpret_cast<Record*>(tree.root);check(std::string(root->label.text.data)==name&&root->children.count==2,"root must be object, not array wrapper");
        check(tree.detached.size()==1&&receipt.allocated==3&&receipt.attached==2,"native child ownership transferred exactly once");
        const auto child=static_cast<Record**>(root->children.data)[0];check(static_cast<Token*>(child->tokens.data)[0].type==2,"native String token preserved");
        check(!api::decode_marketing_record(funcs,memory,kind,source,tree,receipt,e),"cannot replace owned tree");
        check(api::destroy_records(funcs,tree,receipt,e)&&allocated==released,"native Record cleanup balanced");
        check(!api::validate_records(source,e),"existing Subscriber validation remains strict");
        auto wrong=source;wrong[0].label.text="mmoSubscriber";const auto before=allocated;
        check(!api::decode_marketing_record(funcs,memory,kind,wrong,tree,receipt,e)&&allocated==before,"wrong typed object rejected before native allocation");
        wrong=source;wrong[1].parent=1;check(!api::decode_marketing_record(funcs,memory,kind,wrong,tree,receipt,e)&&allocated==before,"invalid structure rejected before native allocation");
        auto missing=funcs;missing.add_child=nullptr;check(!api::decode_marketing_record(missing,memory,kind,source,tree,receipt,e)&&allocated==before,"missing native API rejected before allocation");
        source[0].label.text="mmoSubscriber";check(api::decode_records(funcs,memory,source,tree,receipt,e),"existing Subscriber decoder still runs");
        root=reinterpret_cast<Record*>(tree.root);check(std::string(root->label.text.data)=="subscriber"&&root->children.count==1,"Subscriber array wrapper preserved");
        check(api::destroy_records(funcs,tree,receipt,e)&&allocated==released,"Subscriber cleanup remains balanced");
    }
    std::vector<model::Record> source{{SIZE_MAX,0,{1,0,"mmoAward"},{}},{0,0,{1,0,"day"},{{4,2,{}}}}};
    api::NativeTree tree;api::DecodeReceipt receipt;crash_add=true;
    check(!api::decode_marketing_record(funcs,memory,api::MarketingRecordKind::Award,source,tree,receipt,e)&&receipt.unknown&&receipt.exception==0xc0000005,"native attach fault retained");
    const auto releases_before=released;check(!api::destroy_records(funcs,tree,receipt,e)&&released==releases_before,"uncertain attachment cannot be destroyed or replayed");
    // The injected failure occurs before fake AddChild transfers either pointer.
    for(auto p:tree.detached){destroy(reinterpret_cast<void*>(p));release(reinterpret_cast<void*>(p));}
    std::cout<<checks<<" actual Marketing Record decoder assertions passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
