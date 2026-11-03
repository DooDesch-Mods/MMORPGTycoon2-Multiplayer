#define CHARACTER_RENAME_NO_MAIN
#include "test_character_rename_runtime.cpp"
#include <native_services.h>
uintptr_t native_caller=0;
void* fixture_return_address(){return reinterpret_cast<void*>(native_caller);}
#define _ReturnAddress fixture_return_address
bool preview_code_valid=true,preview_seed_valid=true;
unsigned preview_seeds=0,preview_selects=0,preview_attachments=0;
std::array<uint64_t,5> shared_random{},other_random{};
std::vector<uintptr_t> random_receivers;
namespace character_definition_identity {
struct Definition {uintptr_t object=0;bool deleted=false;uint64_t uid=0;};
struct Census {std::vector<Definition> definitions;};
}
namespace character_membership_reader {struct Users {};}
namespace character_actor_records {
bool read_current(const a::Context&,const a::Binding&,character_definition_identity::Census& out,character_membership_reader::Users&,std::string&){
    out.definitions={{0x400,false,77}};return true;
}}
namespace preview {
#include "character_rename_preview_types_actual.inc"
bool code(Memory&,std::string& error){return preview_code_valid||fail(error,"Preview exact code differs");}
uint32_t seed_private(void* object,uint64_t seed){++preview_seeds;if(!preview_seed_valid)return 1;
    std::memcpy(object,&seed,sizeof seed);return 0;}
uint32_t call_attach(Attach function,void* pane){try{function(pane);return 0;}catch(const std::exception&){return 0xe0000001;}}
uint32_t call_select(Select function,void* view,void* selected){try{function(view,selected);return 0;}catch(const std::exception&){return 0xe0000001;}}
#include "character_rename_preview_context_actual.inc"
#include "character_rename_preview_router_actual.inc"
#include "character_rename_preview_actor_actual.inc"
#include "character_rename_preview_lifetime_actual.inc"
}
#include "character_rename_preview_public_actual.inc"
int32_t native_random(void* receiver,int32_t maximum){
    random_receivers.push_back(uintptr_t(receiver));uint64_t value=0;std::memcpy(&value,receiver,8);++value;std::memcpy(receiver,&value,8);
    return maximum>0?int32_t(value%uint64_t(maximum)):0;
}
int32_t framework_random(void* receiver,int32_t maximum){
    MT2FW_MT2_CharacterPreviewIntCall call{sizeof(call),0,uint64_t(native_caller-state.image),GetCurrentThreadId(),receiver,maximum,0};
    MT2FW_MT2_CharacterPreviewIntSelection selection{};
    const auto result=preview::select_int(&preview::service,&call,&selection);
    if(result==MT2FW_MT2_OK){need(selection.source_bytes==40&&selection.size==sizeof(selection),"Invalid selector private source span");return native_random(selection.source,maximum);}
    if(result==MT2FW_MT2_UNAVAILABLE)return native_random(receiver,maximum);
    throw std::runtime_error(selection.cause);
}
void native_attach(void*){++preview_attachments;native_caller=state.image+0x321ac0;framework_random(shared_random.data(),7);}
void native_select(void*,void* selected){++preview_selects;ui_selected=uintptr_t(selected);
    native_caller=state.image+0x9061c;framework_random(shared_random.data(),5);
    preview::attach_entry(reinterpret_cast<void*>(0x700));
}
void reset_preview(){
    reset();native_values.clear();shared_random={1,2,3,4,5};other_random={6,7,8,9,10};random_receivers.clear();
    preview_code_valid=preview_seed_valid=true;preview_seeds=preview_selects=preview_attachments=0;
    preview::enabled=true;preview::active=nullptr;preview::serial=preview::scopes=preview::redirected=0;
    preview::selector_token=1;preview::ingress_fault=false;preview::original_attach=&native_attach;
    native_values[{state.image,0x164b250}]=uintptr_t(shared_random.data());
    native_values[{0x700,0}]=state.image+0x1712310;native_values[{state.image+0x1705990,0xb8}]=state.image+0x973d0;
}
int main()try{
    std::string error;
    for(bool is_host:{true,false}){
        reset_preview();Config::instance().is_host=is_host;session.role=is_host?a::Role::Host:a::Role::Client;session.local_peer=is_host?1:2;
        const auto before_random=shared_random;bool handled=false;
        need(preview_presentation(reinterpret_cast<void*>(0x100),reinterpret_cast<void*>(0x400),&native_select,handled,error)&&handled,error);
        need(preview_selects==1&&preview_attachments==1&&preview_seeds==1&&preview::redirected==2&&!preview::active,"Original view/preview scope count differs");
        need(shared_random==before_random&&random_receivers.size()==2&&random_receivers[0]==random_receivers[1]&&random_receivers[0]!=uintptr_t(shared_random.data()),"Preview drew from or restored shared RNG");
        reset_preview();const auto standalone=shared_random;preview::attach_entry(reinterpret_cast<void*>(0x700));
        need(preview_attachments==1&&preview::redirected==1&&preview_seeds==1&&shared_random==standalone&&!preview::active,"Standalone class-card preview escaped its private source");
    }
    reset_preview();native_caller=state.image+0x9061c;framework_random(shared_random.data(),5);
    need(shared_random[0]==2&&!preview_seeds&&!preview::redirected,"Out-of-scope world RNG was intercepted");
    reset_preview();preview::Frame frame;Memory memory;need(preview::prepare(frame,memory,0x100,error),error);
    {
        preview::Scope scope(frame,true);const auto before_random=shared_random;
        native_caller=state.image+0x321ac0;framework_random(shared_random.data(),7);
        need(shared_random[0]==before_random[0]+1&&!preview::redirected,"Variant without ActorOrbitPane owner was intercepted");
        const auto before_other=other_random;native_caller=state.image+0x9061c;framework_random(other_random.data(),5);
        need(other_random[0]==before_other[0]+1&&!preview::redirected,"Unrelated RNG receiver was intercepted");
        native_caller=state.image+0x12345;framework_random(shared_random.data(),5);
        need(shared_random[0]==before_random[0]+2&&!preview::redirected,"Unrelated RNG callsite was intercepted");
    }
    need(!preview::active,"Preview scope escaped return");
    reset_preview();preview_code_valid=false;bool handled=false;
    need(!preview_presentation(reinterpret_cast<void*>(0x100),reinterpret_cast<void*>(0x400),&native_select,handled,error)&&handled&&!preview_selects&&!preview_seeds,"Foreign native code entered original");
    reset_preview();preview_seed_valid=false;
    need(!preview_presentation(reinterpret_cast<void*>(0x100),reinterpret_cast<void*>(0x400),&native_select,handled,error)&&!preview_selects,"Failed private native seed entered original");
    reset_preview();native_values[{0x700,0}]=state.image+1;preview::attach_entry(reinterpret_cast<void*>(0x700));
    need(fault&&!preview_attachments&&!preview::active,"World/foreign actor was accepted as UI preview");
    reset_preview();need(preview::prepare(frame,memory,0x100,error),error);
    {preview::Scope scope(frame,true);native_caller=state.image+0x9061c;
        bool failed=false;try{framework_random(shared_random.data(),0);}catch(const std::runtime_error&){failed=true;}
        need(failed&&frame.failed&&fault&&shared_random[0]==1,"Invalid private RNG range returned a success-shaped value");}
    const auto previous_calls=preview_selects;handled=false;
    need(!preview_presentation(reinterpret_cast<void*>(0x100),reinterpret_cast<void*>(0x400),&native_select,handled,error)&&handled&&preview_selects==previous_calls,"Retained ingress failure fell back to original view");
    reset_preview();MT2FW_MT2_CharacterPreviewIntCall call{sizeof(call),0,0x9061c,7,shared_random.data(),5,0};
    MT2FW_MT2_CharacterPreviewIntSelection selection{};
    need(preview::select_int(nullptr,&call,&selection)==MT2FW_MT2_FAILED&&selection.cause[0]&&preview::ingress_fault,"Foreign callback owner lacked a retained ABI failure");
    reset_preview();preview::selector_token=0;preview::service={};
    MT2FW_MT2_ServiceV2 api{};api.v1.size=sizeof(api);api.v1.version=MT2FW_MT2_PREVIEW_SERVICE_VERSION;api.v1.context=&api;
    api.character_preview_int_bind=[](void*,MT2FW_MT2_CharacterPreviewIntSelector select,void* user,uint64_t* token){
        if(select!=&preview::select_int||user!=&preview::service)return MT2FW_MT2_INVALID;*token=42;return MT2FW_MT2_OK;};
    api.character_preview_int_unbind=[](void*,uint64_t token){return token==42?MT2FW_MT2_OK:MT2FW_MT2_INVALID;};
    need(preview::bind(api,error)&&preview::selector_token==42,error);
    need(!preview::bind(api,error)&&preview::selector_token==42,"Second selector registration replaced the owned token");
    {preview::Scope scope(frame,true);need(!preview::retire(error)&&preview::selector_token==42,"Borrowed private source retired while in scope");}
    need(preview::retire(error)&&!preview::selector_token&&!preview::service.v1.context,error);
    std::cout<<checks<<" actual Character preview owner/private native RNG/passthrough checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
