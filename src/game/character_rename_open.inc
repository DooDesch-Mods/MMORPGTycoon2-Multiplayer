struct OfferedCard {uintptr_t window=0,card=0,button=0;};
bool open_array(Memory& memory,uintptr_t pane,size_t offset,std::vector<uintptr_t>& rows,std::string& error){
    uintptr_t data=0;int32_t count=0,capacity=0;
    if(!field(memory,pane,offset+8,data,error)||!field(memory,pane,offset+16,count,error)||
       !field(memory,pane,offset+20,capacity,error)||count<0||count>4096||capacity<count||capacity>65536||(count&&!data))
        return fail(error,error.empty()?"Character original collection pane array bounds differ":error);
    rows.clear();for(int32_t i=0;i<count;++i){uintptr_t value=0;if(!field(memory,data,size_t(i)*8,value,error)||!value)return false;rows.push_back(value);}return true;
}
#include "character_rename_menu.inc"
bool offered_card(Memory& memory,const json& args,OfferedCard& out,std::string& error){
    out={};uintptr_t data=0;int32_t count=0,capacity=0;
    if(!field(memory,state.image,0x1a43478,data,error)||!field(memory,state.image,0x1a43480,count,error)||
       !field(memory,state.image,0x1a43484,capacity,error)||count<0||count>1024||capacity<count||capacity>65536||(count&&!data))
        return fail(error,error.empty()?"Character original collection window registry differs":error);
    for(int32_t i=0;i<count;++i){uintptr_t window=0;std::string name;uint8_t shown=0;
        if(!field(memory,data,size_t(i)*8,window,error)||!npc_quest_records::read_string(memory,window,0x1b0,"Character collection window",name,error))return false;
        if(name!="collection_classes")continue;
        if(!field(memory,window,0x45f,shown,error)||shown>1)return fail(error,"Character collection visibility differs");
        if(!shown)continue;if(out.window)return fail(error,"Character original class collection is ambiguous");out.window=window;
    }
    if(!out.window)return fail(error,"Character original class collection must be visible through its offered Design menu");
    std::vector<uintptr_t> panes{out.window};std::set<uintptr_t> seen;
    while(!panes.empty()){const auto pane=panes.back();panes.pop_back();uintptr_t ap=0,gather=0;
        if(!seen.insert(pane).second||seen.size()>4096||!field(memory,pane,0,ap,error)||!field(memory,ap,0x178,gather,error))
            return fail(error,error.empty()?"Character original collection pane cycle or class differs":error);
        if(pane!=out.window){uintptr_t owner=0,parent=0;
            if(!field(memory,pane,0x280,owner,error)||owner!=out.window||!field(memory,pane,0x278,parent,error)||!seen.contains(parent))
                return fail(error,error.empty()?"Character original collection pane ancestry differs":error);}
        if(ap==state.image+0x1740810){int32_t kind=-1,id=-1;uintptr_t button=0,collection=0,proxy=0,target=0;
            if(!field(memory,pane,0x358,kind,error)||!field(memory,pane,0x35c,id,error))return false;
            if(args.at("kind")==kind&&args.at("id")==id){
                if(out.card||!field(memory,pane,0x340,collection,error)||!collection||!field(memory,pane,0x348,proxy,error)||
                   !proxy||!field(memory,proxy,0,target,error)||target!=collection+0x30||!field(memory,pane,0x390,button,error)||!button)
                    return fail(error,error.empty()?"Character original class card target/button is absent or ambiguous":error);
                uintptr_t button_ap=0;uint8_t disabled=1,clickable=0;
                if(!field(memory,button,0,button_ap,error)||button_ap!=state.image+0x16ea260||
                   !field(memory,button,0x46d,disabled,error)||disabled!=0||!field(memory,pane,0x174,clickable,error)||clickable!=1||
                   !offered_control(memory,button,out.window,error)||!offered_control(memory,pane,out.window,error))
                    return fail(error,error.empty()?"Character original class card is not offered":error);
                out.card=pane;out.button=button;
            }
        }
        if(gather==state.image+0xc41fe0)continue;
        if(gather!=state.image+0x81ed30)return fail(error,"Character original collection GatherAllContents slot differs");
        for(const auto offset:{size_t(0x2c8),size_t(0x2f8)}){std::vector<uintptr_t> children;
            if(!open_array(memory,pane,offset,children,error))return false;
            for(auto child:children){uintptr_t parent=0;if(!field(memory,child,0x278,parent,error)||parent!=pane)return fail(error,"Character original collection child parent differs");panes.push_back(child);}}
    }
    return out.card!=0||fail(error,"Character requested definition has no visible offered original class card");
}
bool open_code(Memory& memory,std::string& error){
    constexpr preview::Pin pins[]{
        {0x6882a0,373,"ce4675ae6935feda176bbcf3858666aa796fec7549b17956cbed36fa1330d08b"},
        {0x687d40,144,"e58bb7335a2990250cf3bfb09595c256727be65692e7ee93ef5ce5fa5a963cd0"},
        {0x81ed30,592,"1af40a14bbeeb6087a3aad7c6264abac9cc91d4bf1f021f9d1cc0fc1f28d7b0e"},
        {0xc41fe0,256,"ed7d767eaa9ee779da2d0b72da3c7e06cd49d3a3bea8c2a329c333c006f994b8"},
        {0x587030,256,"ccdb828ec334546d27abccdc0d62f658d360786c7b4000d10204a616eb9afc94"},
        {0x7451f0,560,"3d2313caeab6d519714cc0ebe947de7099f126f3892771341adbf693713d7498"}};
    for(const auto& pin:pins){std::vector<unsigned char> bytes(pin.bytes);
        if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error)||
           !hooks::framework::verify_original_hash(state.image+pin.rva,bytes.data(),bytes.size(),pin.sha,error))
            return fail(error,"Character original collection code differs RVA="+std::to_string(pin.rva)+" cause="+error);}
    return true;
}
bool execute_open(UiJob& job,std::string& error){
    Memory memory;OfferedCard card;a::CharacterRenameIntent intent;Sample before,after;
    intent.kind=job.arguments.at("kind").get<int32_t>();intent.id=job.arguments.at("id").get<int32_t>();
    if(!code(memory,error)||!open_code(memory,error)||!sample(memory,job.binding,job.context.snapshot_uuid,intent,before,error))return false;
    job.result["before"]=diagnostic_sample(before);
    if(!execute_menu(job,before,intent,error)||!offered_card(memory,job.arguments,card,error))return false;
    a::ui_diagnostic::Api api;if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error))return false;
    api.command=reinterpret_cast<Command>(state.image+0x6882a0);api.destroy_label="clicked";
    a::ui_diagnostic::Window control;control.window=card.card;a::ui_diagnostic::Receipt receipt;
    job.attempted=true;
    if(!a::ui_diagnostic::process_calls().command(api,control,receipt,error)||!receipt.ui_command_returned||!receipt.record_cleanup_confirmed)
        return fail(error,error.empty()?"Character original class-card click lacks its complete native receipt":error);
    uintptr_t view=0,editor=0,outer=0;
    if(fault||!window(memory,before.selected,view,editor,outer,error)||!offered_control(memory,editor,outer,error)||
       !sample(memory,job.binding,job.context.snapshot_uuid,intent,after,error)||!same_local(before,after)||before.value!=after.value)
        return fail(error,error.empty()?"Character original class-card open changed complete gameplay/ArtStore/RNG proof":error);
    job.result["original_class_card_open"]=true;job.result["open_complete_sha256"]=after.value.all_sha256;
    job.result["open_others_sha256"]=after.value.others_sha256;job.result["after_card"]=diagnostic_sample(after);return true;
}
