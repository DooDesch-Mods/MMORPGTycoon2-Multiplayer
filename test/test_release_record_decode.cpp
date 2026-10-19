#include "subscriber_lifecycle/record_decode.h"
#include "release_graph.h"
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
void native_label(Record& record,const char* value){Text name{};string_construct(&name,value,std::strlen(value),nullptr);label(&record,&name);string_destroy(&name);}
void native_roundtrip(const api::DecodeApi& funcs,Memory& memory){
    namespace graph=mt2mp::game::release_graph;
    const model::Budget budget{4096,32768,262144,4096};
    // These independent names are the original RTTI constructor inputs after
    // __cxa_demangle, not values obtained from the production root selector.
    const char* names[]{"mmoReleaseManager","vsObjectArray<mmoReleaseDemand>","vsObjectArray<mmoTicket>"};
    for(unsigned family=0;family<3;++family)for(unsigned populated=0;populated<2;++populated){
        const auto kind=static_cast<graph::Kind>(family);std::string error;
        Record original;construct(&original);native_label(original,names[family]);
        if(populated){
            auto* first=static_cast<Record*>(allocate(sizeof(Record)));construct(first);native_label(*first,"object");
            auto* second=static_cast<Record*>(allocate(sizeof(Record)));construct(second);native_label(*second,"sibling");
            auto* grandchild=static_cast<Record*>(allocate(sizeof(Record)));construct(grandchild);native_label(*grandchild,"values");
            count(grandchild,4);auto* values=static_cast<Token*>(grandchild->tokens.data);
            integer(&values[0],INT32_MIN);floating(&values[1],-0.0f);type(&values[2],0);
            Text payload{};string_construct(&payload,"literal",7,nullptr);text(&values[3],&payload);string_destroy(&payload);
            add(&original,first);add(&original,second);add(first,grandchild);
        }
        std::array<uintptr_t,4> empty_table{};uintptr_t context=uintptr_t(empty_table.data());
        model::Graph captured;
        check(model::read_graph(memory,uintptr_t(&original),uintptr_t(&context),budget,captured,error),"production reader accepts original owned Record layout");
        graph::Value value;
        const auto unused_key=[](uintptr_t,std::string&,std::string&){return false;};
        check(graph::capture_value(captured,unused_key,value,error,kind),"native RTTI root accepted without producer normalization");
        check(value.records[0].label.type==1&&value.records[0].label.text==names[family],"original root label preserved exactly");
        check(graph::decode(graph::encode(value))==value,"complete captured typed graph wire roundtrip");
        if(populated){
            check(value.records.size()==4&&value.records[1].parent==0&&value.records[2].parent==0&&value.records[2].child_index==1&&value.records[3].parent==1,"native breadth-first ancestry and child order preserved");
            const auto& tokens=value.records[3].tokens;
            check(tokens.size()==4&&tokens[0].bits==0x80000000U&&tokens[1].bits==0x80000000U&&tokens[2].type==0&&tokens[3].text=="literal","signed integer and float bits and full scalar content preserved");
        }
        api::NativeTree decoded;api::DecodeReceipt receipt;
        check(api::decode_release_record(funcs,memory,value.records,decoded,receipt,error,kind),"typed captured graph decoded through original Record operations");
        model::Graph reread;graph::Value reconstructed;
        check(model::read_graph(memory,decoded.root,uintptr_t(&context),budget,reread,error)&&graph::capture_value(reread,unused_key,reconstructed,error,kind)&&reconstructed==value,"full reconstructed native tree equals captured original");
        check(api::destroy_records(funcs,decoded,receipt,error),"reconstructed owning Record tree cleaned once");
        for(unsigned other=0;other<3;++other)if(other!=family){
            auto wrong=value;wrong.kind=static_cast<graph::Kind>(other);const auto before=allocated;
            check(!graph::valid(wrong,error)&&error.find("reason=typed root")!=std::string::npos&&error.find("kind="+std::to_string(other))!=std::string::npos,"another RTTI family rejected with measured kind");
            check(!api::decode_release_record(funcs,memory,value.records,decoded,receipt,error,wrong.kind)&&allocated==before,"another RTTI family rejected before allocation");
        }
        auto wrong=value;wrong.records[0].label.text="";const auto before=allocated;
        check(!graph::valid(wrong,error)&&error.find("label=\"\"")!=std::string::npos,"legacy empty root rejected with measured label");
        check(!api::decode_release_record(funcs,memory,wrong.records,decoded,receipt,error,kind)&&allocated==before,"legacy empty array root rejected before allocation");
        wrong=value;wrong.records[0].label.type=2;
        check(!graph::valid(wrong,error)&&error.find("label_type=2")!=std::string::npos,"String root cannot masquerade as Label");
        if(populated){
            wrong=value;wrong.records[1].parent=1;
            check(!graph::valid(wrong,error)&&error.find("reason=parent must precede child")!=std::string::npos&&error.find("row=1 parent=1")!=std::string::npos,"self parent rejected with actual row and parent");
            wrong=value;wrong.records[2].child_index=0;
            check(!graph::valid(wrong,error)&&error.find("reason=child order")!=std::string::npos&&error.find("expected_child=1")!=std::string::npos,"duplicate sibling ordinal rejected with required ordinal");
            wrong=value;wrong.records[0].label.bits=7;
            check(!graph::valid(wrong,error)&&error.find("reason=label scalar")!=std::string::npos&&error.find("label_bits=7")!=std::string::npos,"inactive label bits remain invalid");
        }
        destroy(&original);check(allocated==released,"original and reconstructed Record owners fully balanced");
    }
}
}
int main(){try{
    const api::DecodeApi funcs{allocate,release,construct,destroy,string_destroy,add,label,text,count,integer,type,floating,string_construct};
    Memory memory;std::string e;
    native_roundtrip(funcs,memory);
    {
        const char* name="mmoReleaseManager";
        std::vector<model::Record> source{{SIZE_MAX,0,{1,0,name},{}},{0,0,{1,0,"name"},{{2,0,"{native_key}"}}},{0,1,{1,0,"day"},{{4,2,{}}}}};
        api::NativeTree tree;api::DecodeReceipt receipt;
        check(api::decode_release_record(funcs,memory,source,tree,receipt,e),"fresh typed Record decode");
        const auto* root=reinterpret_cast<Record*>(tree.root);check(std::string(root->label.text.data)==name&&root->children.count==2,"root must be object, not array wrapper");
        check(tree.detached.size()==1&&receipt.allocated==3&&receipt.attached==2,"native child ownership transferred exactly once");
        const auto child=static_cast<Record**>(root->children.data)[0];check(static_cast<Token*>(child->tokens.data)[0].type==2,"native String token preserved");
        check(!api::decode_release_record(funcs,memory,source,tree,receipt,e),"cannot replace owned tree");
        check(api::destroy_records(funcs,tree,receipt,e)&&allocated==released,"native Record cleanup balanced");
        check(!api::validate_records(source,e),"existing Subscriber validation remains strict");
        auto wrong=source;wrong[0].label.text="mmoSubscriber";const auto before=allocated;
        check(!api::decode_release_record(funcs,memory,wrong,tree,receipt,e)&&allocated==before,"wrong typed object rejected before native allocation");
        wrong=source;wrong[1].parent=1;check(!api::decode_release_record(funcs,memory,wrong,tree,receipt,e)&&allocated==before,"invalid structure rejected before native allocation");
        auto missing=funcs;missing.add_child=nullptr;check(!api::decode_release_record(missing,memory,source,tree,receipt,e)&&allocated==before,"missing native API rejected before allocation");
        source[0].label.text="mmoSubscriber";check(api::decode_records(funcs,memory,source,tree,receipt,e),"existing Subscriber decoder still runs");
        root=reinterpret_cast<Record*>(tree.root);check(std::string(root->label.text.data)=="subscriber"&&root->children.count==1,"Subscriber array wrapper preserved");
        check(api::destroy_records(funcs,tree,receipt,e)&&allocated==released,"Subscriber cleanup remains balanced");
    }
    {
        std::vector<model::Record> prepared{{SIZE_MAX,0,{1,0,"vsObjectArray<mmoReleaseDemand>"},{}},{0,0,{1,0,"mmoReleaseDemand"},{{4,2,{}}}}};
        api::NativeTree tree;api::DecodeReceipt receipt;
        check(api::decode_release_record(funcs,memory,prepared,tree,receipt,e,model::ReleaseRecordKind::PreparedDemands),"prepared demand typed RTTI root uses actual original decoder");
        check(api::destroy_records(funcs,tree,receipt,e)&&allocated==released,"prepared original Record ownership cleanup balanced");
        const auto before=allocated;check(!api::decode_release_record(funcs,memory,prepared,tree,receipt,e)&&allocated==before,"prepared array cannot masquerade as Manager root");
    }
    std::vector<model::Record> source{{SIZE_MAX,0,{1,0,"mmoReleaseManager"},{}},{0,0,{1,0,"day"},{{4,2,{}}}}};
    api::NativeTree tree;api::DecodeReceipt receipt;crash_add=true;
    check(!api::decode_release_record(funcs,memory,source,tree,receipt,e)&&receipt.unknown&&receipt.exception==0xc0000005,"native attach fault retained");
    const auto releases_before=released;check(!api::destroy_records(funcs,tree,receipt,e)&&released==releases_before,"uncertain attachment cannot be destroyed or replayed");
    // The injected failure occurs before fake AddChild transfers either pointer.
    for(auto p:tree.detached){destroy(reinterpret_cast<void*>(p));release(reinterpret_cast<void*>(p));}
    std::cout<<checks<<" actual Release Record decoder assertions passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
