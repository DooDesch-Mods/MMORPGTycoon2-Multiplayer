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
    if(!field(memory,state.image,0x1a3c478,data,error)||!field(memory,state.image,0x1a3c480,count,error)||
       !field(memory,state.image,0x1a3c484,capacity,error)||count<0||count>1024||capacity<count||capacity>65536||(count&&!data))
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
        if(ap==state.image+0x173a470){int32_t kind=-1,id=-1;uintptr_t button=0,collection=0,proxy=0,target=0;
            if(!field(memory,pane,0x358,kind,error)||!field(memory,pane,0x35c,id,error))return false;
            if(args.at("kind")==kind&&args.at("id")==id){
                if(out.card||!field(memory,pane,0x340,collection,error)||!collection||!field(memory,pane,0x348,proxy,error)||
                   !proxy||!field(memory,proxy,0,target,error)||target!=collection+0x30||!field(memory,pane,0x390,button,error)||!button)
                    return fail(error,error.empty()?"Character original class card target/button is absent or ambiguous":error);
                uintptr_t button_ap=0;uint8_t disabled=1,clickable=0;
                if(!field(memory,button,0,button_ap,error)||button_ap!=state.image+0x16e3ec0||
                   !field(memory,button,0x46d,disabled,error)||disabled!=0||!field(memory,pane,0x174,clickable,error)||clickable!=1||
                   !offered_control(memory,button,out.window,error)||!offered_control(memory,pane,out.window,error))
                    return fail(error,error.empty()?"Character original class card is not offered":error);
                out.card=pane;out.button=button;
            }
        }
        if(gather==state.image+0xc35730)continue;
        if(gather!=state.image+0x8167c0)return fail(error,"Character original collection GatherAllContents slot differs");
        for(const auto offset:{size_t(0x2c8),size_t(0x2f8)}){std::vector<uintptr_t> children;
            if(!open_array(memory,pane,offset,children,error))return false;
            for(auto child:children){uintptr_t parent=0;if(!field(memory,child,0x278,parent,error)||parent!=pane)return fail(error,"Character original collection child parent differs");panes.push_back(child);}}
    }
    return out.card!=0||fail(error,"Character requested definition has no visible offered original class card");
}
bool open_code(Memory& memory,std::string& error){
    constexpr preview::Pin pins[]{
        {0x681090,373,"8235bd603617deb3262e3e31f9de7d88ce31ffa1bea881078632e9782d10adb8"},
        {0x680b30,144,"d9e25acb5a72f7feadd518f761a98acc49771c48676637ca9a827df5c8f73654"},
        {0x8167c0,592,"bcad314728ab3392b6013ad666b3f69be773c4fcc646b7b9381611f141df981e"},
        {0xc35730,256,"37b604ccc54fb36f6beb2cdde2acc9fc27fcf6ed087c6b7376363af6e7ddb45f"},
        {0x580870,256,"75ca8d29939c1f7021187035246032bbd35bc481f9cff95d9b99eba33e98a361"},
        {0x73d8a0,560,"50dc5302a27257fd9498734068d51c837bc5f9faece0ec2042de5b48f652391c"}};
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
    api.command=reinterpret_cast<Command>(state.image+0x681090);api.destroy_label="clicked";
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
