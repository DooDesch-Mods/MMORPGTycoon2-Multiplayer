#include "record_decode.h"
#include <algorithm>
#include <bit>
#include <climits>
#include <exception>
#include <limits>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace mt2mp::subscriber_lifecycle::first_append {
namespace {
enum class Op {Allocate,Release,Construct,Destroy,StringConstruct,StringDestroy,Add,Label,String,Count,Int,Type,Float};
struct Invocation {Op op;void* target=nullptr;const void* input=nullptr;size_t length=0;int32_t number=0;float value=0;void* result=nullptr;};
uint32_t invoke(const DecodeApi* a,Invocation* c){
    __try {
        switch(c->op){
        case Op::Allocate:c->result=a->allocate(c->length);break;
        case Op::Release:a->release(c->target);break;
        case Op::Construct:a->record_construct(c->target);break;
        case Op::Destroy:a->record_destroy(c->target);break;
        case Op::StringConstruct:a->string_construct(c->target,static_cast<const char*>(c->input),c->length,nullptr);break;
        case Op::StringDestroy:a->string_destroy(c->target);break;
        case Op::Add:a->add_child(c->target,c->input);break;
        case Op::Label:a->set_label(c->target,c->input);break;
        case Op::String:a->set_string(c->target,c->input);break;
        case Op::Count:a->token_count(c->target,c->number);break;
        case Op::Int:a->set_integer(c->target,c->number);break;
        case Op::Type:a->set_type(c->target,c->number);break;
        case Op::Float:a->set_float(c->target,c->value);break;
        }
    } __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    return 0;
}
bool call(const DecodeApi& a,Invocation& op,DecodeReceipt& r,std::string& e){
    try {r.exception=invoke(&a,&op);if(!r.exception)return true;e="native Record operation SEH="+std::to_string(r.exception);}
    catch(const std::exception& ex){e="native Record operation C++: "+std::string(ex.what());}
    catch(...){e="native Record operation unknown C++ exception";}
    r.unknown=true;r.cause=e;return false;
}
bool fail(std::string& e,const char* why){e=why;return false;}
bool token_valid(const Token& t){return t.type>=0&&t.type<=4&&((t.type==1||t.type==2)||t.text.empty());}
bool set_token(const DecodeApi& a,uintptr_t token,const Token& source,DecodeReceipt& r,std::string& e){
    Invocation op{Op::Type,reinterpret_cast<void*>(token)};
    if(source.type==1||source.type==2){
        alignas(8) std::array<unsigned char,32> text{};
        op={Op::StringConstruct,text.data(),source.text.data(),source.text.size()};
        if(!call(a,op,r,e))return false;
        op={source.type==1?Op::Label:Op::String,reinterpret_cast<void*>(token),text.data()};
        const bool copied=call(a,op,r,e);
        const std::string primary=e;
        op={Op::StringDestroy,text.data()};
        std::string cleanup;
        const bool cleaned=call(a,op,r,cleanup);
        if(!copied||!cleaned){e=primary;if(!cleaned)e+=(e.empty()?"":"; ")+cleanup;r.cause=e;return false;}
    }else{
        op.op=source.type==3?Op::Float:(source.type==4?Op::Int:Op::Type);
        op.number=source.type==4?std::bit_cast<int32_t>(source.bits):source.type;
        op.value=std::bit_cast<float>(source.bits);
        if(!call(a,op,r,e))return false;
    }
    ++r.tokens_written;return true;
}
bool create(const DecodeApi& a,Memory& m,const Record& source,uintptr_t& out,NativeTree& tree,DecodeReceipt& r,std::string& e){
    Invocation op{Op::Allocate};op.length=0x78;
    if(!call(a,op,r,e))return false;
    if(!op.result)return fail(e,"native Record allocation returned null");
    out=reinterpret_cast<uintptr_t>(op.result);++r.allocated;
    tree.detached.push_back(out);
    op={Op::Construct,reinterpret_cast<void*>(out)};
    if(!call(a,op,r,e))return false;
    ++r.constructed;
    if(!set_token(a,out,source.label,r,e))return false;
    op={Op::Count,reinterpret_cast<void*>(out)};op.number=static_cast<int32_t>(source.tokens.size());
    if(!call(a,op,r,e))return false;
    uintptr_t tokens=0;int32_t count=0;
    if(!m.read(out+0x38,&tokens,sizeof tokens,e)||!m.read(out+0x40,&count,sizeof count,e))return false;
    if(count!=op.number||(!tokens&&count))return fail(e,"native Record allocated token count differs");
    for(size_t i=0;i<source.tokens.size();++i){
        if(i>(UINTPTR_MAX-tokens)/0x30)return fail(e,"native Record token address overflow");
        if(!set_token(a,tokens+i*0x30,source.tokens[i],r,e))return false;
    }
    return true;
}
bool attach(const DecodeApi& a,uintptr_t parent,uintptr_t child,NativeTree& tree,DecodeReceipt& r,std::string& e){
    Invocation op{Op::Add,reinterpret_cast<void*>(parent),reinterpret_cast<void*>(child)};
    if(!call(a,op,r,e))return false;
    auto it=std::find(tree.detached.begin(),tree.detached.end(),child);
    if(it==tree.detached.end()){r.unknown=true;return fail(e,"native Record child ownership tracking missing");}
    tree.detached.erase(it);++r.attached;return true;
}
}
static bool validate_record_tree(const std::vector<Record>& source,std::string& e){
    if(source.empty()||source.size()>static_cast<size_t>(INT_MAX))return fail(e,"Subscriber Record count outside native int range");
    std::vector<size_t> children(source.size(),0);
    for(size_t i=0;i<source.size();++i){
        const auto& r=source[i];
        if((i==0&&(r.parent!=SIZE_MAX||r.child_index!=0))||(i!=0&&(r.parent>=i||r.child_index!=children[r.parent]++)))
            return fail(e,"Subscriber Record parent/child order invalid");
        if(!token_valid(r.label)||r.tokens.size()>static_cast<size_t>(INT_MAX))return fail(e,"Subscriber Record label/token count invalid");
        for(const auto& token:r.tokens)if(!token_valid(token))return fail(e,"Subscriber Record token type invalid");
    }
    e.clear();return true;
}
bool validate_records(const std::vector<Record>& source,std::string& e){
    if(!validate_record_tree(source,e))return false;
    if(source[0].label.type!=1||source[0].label.text!="mmoSubscriber")return fail(e,"Subscriber Record root type differs");
    e.clear();return true;
}
bool decode_party_record(const DecodeApi& a,Memory& memory,const std::vector<Record>& source,NativeTree& tree,DecodeReceipt& r,std::string& e){
    r={};if(tree.root||!tree.detached.empty())return fail(e,"Party Record output already owns native objects");
    if(!validate_record_tree(source,e))return false;
    if(source.size()>100000||source[0].label.type!=1||source[0].label.text!="mmoParty")return fail(e,"Party Record typed root or capture budget differs");
    if(!a.allocate||!a.release||!a.record_construct||!a.record_destroy||!a.string_destroy||!a.add_child||!a.set_label||!a.set_string||
       !a.token_count||!a.set_integer||!a.set_type||!a.set_float||!a.string_construct)return fail(e,"Party Record native API incomplete");
    std::vector<uintptr_t> objects(source.size(),0);tree.detached.reserve(source.size());
    for(size_t i=0;i<source.size();++i){
        if(!create(a,memory,source[i],objects[i],tree,r,e))return false;
        if(!i)tree.root=objects[i];else if(!attach(a,objects[source[i].parent],objects[i],tree,r,e))return false;
    }
    r.complete=true;e.clear();return true;
}
bool decode_records(const DecodeApi& a,Memory& memory,const std::vector<Record>& source,NativeTree& tree,DecodeReceipt& r,std::string& e){
    r={};
    if(tree.root||!tree.detached.empty())return fail(e,"native Record output already owns objects");
    if(!validate_records(source,e))return false;
    if(!a.allocate||!a.release||!a.record_construct||!a.record_destroy||!a.string_destroy||!a.add_child||!a.set_label||!a.set_string||
       !a.token_count||!a.set_integer||!a.set_type||!a.set_float||!a.string_construct)return fail(e,"native Record API incomplete");
    std::vector<uintptr_t> objects(source.size(),0);
    tree.detached.reserve(source.size()+1);
    Record wrapper;wrapper.label={1,0,"subscriber"};
    if(!create(a,memory,wrapper,tree.root,tree,r,e))return false;
    for(size_t i=0;i<source.size();++i){
        if(!create(a,memory,source[i],objects[i],tree,r,e))return false;
        const auto parent=i?objects[source[i].parent]:tree.root;
        if(!attach(a,parent,objects[i],tree,r,e))return false;
    }
    r.complete=true;e.clear();return true;
}
bool destroy_records(const DecodeApi& a,NativeTree& tree,DecodeReceipt& r,std::string& e){
    if(r.unknown)return fail(e,"native Record outcome unknown; owners retained without cleanup replay");
    if(!a.record_destroy||!a.release)return fail(e,"native Record cleanup API missing");
    r.cleanup_attempted=true;
    while(!tree.detached.empty()){
        const auto object=tree.detached.back();
        Invocation op{Op::Destroy,reinterpret_cast<void*>(object)};
        if(!call(a,op,r,e))return false;
        op={Op::Release,reinterpret_cast<void*>(object)};
        if(!call(a,op,r,e))return false;
        tree.detached.pop_back();
    }
    tree.root=0;r.cleaned=true;e.clear();return true;
}
bool decode_marketing_record(const DecodeApi& a,Memory& memory,MarketingRecordKind kind,const std::vector<Record>& source,NativeTree& tree,DecodeReceipt& r,std::string& e){
    r={};
    if(tree.root||!tree.detached.empty())return fail(e,"Marketing Record output already owns objects");
    const char* expected=kind==MarketingRecordKind::Award?"mmoAward":kind==MarketingRecordKind::Campaign?"mmoAdCampaign":nullptr;
    if(!expected||source.empty()||source.size()>32||source[0].label.type!=1||source[0].label.text!=expected)
        return fail(e,"Marketing Record typed root differs");
    // Share structural validation while preserving the strict Subscriber entry point.
    auto structural=source;structural[0].label.text="mmoSubscriber";
    if(!validate_records(structural,e))return false;
    if(!a.allocate||!a.release||!a.record_construct||!a.record_destroy||!a.string_destroy||!a.add_child||!a.set_label||!a.set_string||
       !a.token_count||!a.set_integer||!a.set_type||!a.set_float||!a.string_construct)return fail(e,"Marketing Record API incomplete");
    std::vector<uintptr_t> objects(source.size(),0);tree.detached.reserve(source.size());
    for(size_t i=0;i<source.size();++i){
        if(!create(a,memory,source[i],objects[i],tree,r,e))return false;
        if(!i)tree.root=objects[i];
        else if(!attach(a,objects[source[i].parent],objects[i],tree,r,e))return false;
    }
    r.complete=true;e.clear();return true;
}
bool decode_logo_record(const DecodeApi& a,Memory& memory,const std::vector<Record>& source,NativeTree& tree,DecodeReceipt& r,std::string& e){
    r={};if(tree.root||!tree.detached.empty())return fail(e,"Logo Record output already owns objects");
    if(source.empty()||source.size()>2048||source[0].label.type!=1||source[0].label.text!="mmoLogo")return fail(e,"Logo Record typed root differs");
    auto structural=source;structural[0].label.text="mmoSubscriber";if(!validate_records(structural,e))return false;
    if(!a.allocate||!a.release||!a.record_construct||!a.record_destroy||!a.string_destroy||!a.add_child||!a.set_label||!a.set_string||
       !a.token_count||!a.set_integer||!a.set_type||!a.set_float||!a.string_construct)return fail(e,"Logo Record API incomplete");
    std::vector<uintptr_t> objects(source.size(),0);tree.detached.reserve(source.size());
    for(size_t i=0;i<source.size();++i){if(!create(a,memory,source[i],objects[i],tree,r,e))return false;
        if(!i)tree.root=objects[i];else if(!attach(a,objects[source[i].parent],objects[i],tree,r,e))return false;}
    r.complete=true;e.clear();return true;
}
bool decode_release_record(const DecodeApi& a,Memory& memory,const std::vector<Record>& source,NativeTree& tree,DecodeReceipt& r,std::string& e,ReleaseRecordKind kind){
    r={};if(tree.root||!tree.detached.empty())return fail(e,"Release Record output already owns objects");
    const auto expected=release_record_root(kind);
    if(!expected||source.empty()||source.size()>4096||source[0].label.type!=1||source[0].label.text!=expected)return fail(e,"Release Record typed root differs");
    auto structural=source;structural[0].label.text="mmoSubscriber";if(!validate_records(structural,e))return false;
    if(!a.allocate||!a.release||!a.record_construct||!a.record_destroy||!a.string_destroy||!a.add_child||!a.set_label||!a.set_string||
       !a.token_count||!a.set_integer||!a.set_type||!a.set_float||!a.string_construct)return fail(e,"Release Record API incomplete");
    std::vector<uintptr_t> objects(source.size(),0);tree.detached.reserve(source.size());
    for(size_t i=0;i<source.size();++i){if(!create(a,memory,source[i],objects[i],tree,r,e))return false;
        if(!i)tree.root=objects[i];else if(!attach(a,objects[source[i].parent],objects[i],tree,r,e))return false;}
    r.complete=true;e.clear();return true;
}
bool decode_integer_record(const DecodeApi& a,Memory& memory,const char* label,int32_t value,NativeTree& tree,DecodeReceipt& r,std::string& e){
    r={};
    if(tree.root||!tree.detached.empty())return fail(e,"integer Record output already owns objects");
    if(!label||!a.allocate||!a.release||!a.record_construct||!a.record_destroy||!a.string_destroy||!a.set_label||
       !a.token_count||!a.set_integer||!a.string_construct)return fail(e,"integer Record API incomplete");
    tree.detached.reserve(1);Record record;record.label={1,0,label};record.tokens.push_back({4,std::bit_cast<uint32_t>(value),{}});
    if(!create(a,memory,record,tree.root,tree,r,e))return false;
    r.complete=true;e.clear();return true;
}
bool decode_string_record(const DecodeApi& a,Memory& memory,const char* label,const std::string& value,NativeTree& tree,DecodeReceipt& r,std::string& e){
    r={};
    if(tree.root||!tree.detached.empty())return fail(e,"string Record output already owns objects");
    if(!label||!a.allocate||!a.release||!a.record_construct||!a.record_destroy||!a.string_destroy||!a.set_label||
       !a.token_count||!a.set_string||!a.string_construct)return fail(e,"string Record API incomplete");
    tree.detached.reserve(1);Record record;record.label={1,0,label};record.tokens.push_back({2,0,value});
    if(!create(a,memory,record,tree.root,tree,r,e))return false;
    r.complete=true;e.clear();return true;
}
}
