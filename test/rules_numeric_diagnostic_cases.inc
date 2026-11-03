void diagnostic_run(){
    const auto setup=[](){reset();go=true;fixture_ui_fields={{{10000,0},fixture_runtime.image+0x1701d20},{{10000,0x450},30000},{{10000,0x458},20000},{{30000,0},fixture_runtime.image+0x17496b0},{{30000,8},fixture_pin.design+0x1c0},{{30000,0x38},fixture_pin.design+0x1d8},{{20000,0},fixture_runtime.image+0x1703290},{{20000,0x4a0},0},{{fixture_runtime.image,0x13df9e0},fixture_pin.design}};fixture_ui_text={{30000+0x18,"seconds"},{20000+0x2d0,"61"}};fixture_state.state.respawn_seconds=60;};
    bool matched=true;auto* input=native_command_fixture::record("ValueEdited",{"61"});
    need(input->arguments.size()==1&&gr::native::command(native_command_fixture::image(),input,"ValueEdited",matched)==0&&matched,"argument-bearing native ValueEdited label did not match");
    input=native_command_fixture::record("Other",{"ValueEdited","61"});
    need(gr::native::command(native_command_fixture::image(),input,"ValueEdited",matched)==0&&!matched,"argument token was mistaken for command label");
    for(const char* label:{"ValueEditedExtra","ValueEdite","valueedited",""}){
        input=native_command_fixture::record(label,{"61"});matched=true;
        need(gr::native::command(native_command_fixture::image(),input,"ValueEdited",matched)==0&&!matched,"nonexact command label matched");
    }
    const char exact[11]={'V','a','l','u','e','E','d','i','t','e','d'};
    input=native_command_fixture::record("ignored",{"61"});input->view={exact,sizeof exact,{}};
    need(gr::native::command(native_command_fixture::image(),input,"ValueEdited",matched)==0&&matched,"length-bounded native label required a terminator");
    input->view={nullptr,11,{}};matched=true;
    need(gr::native::command(native_command_fixture::image(),input,"ValueEdited",matched)==ERROR_INVALID_DATA&&!matched,"null label storage lost error or retained stale match");
    input->view={nullptr,SIZE_MAX,{}};matched=true;
    need(gr::native::command(native_command_fixture::image(),input,"ValueEdited",matched)==0&&!matched,"unmatched extent dereferenced native string storage");
    void* inaccessible=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);need(inaccessible!=nullptr,"fixture inaccessible page allocation failed");
    input->view={static_cast<const char*>(inaccessible),11,{}};matched=true;
    const auto access_fault=gr::native::command(native_command_fixture::image(),input,"ValueEdited",matched);
    need(VirtualFree(inaccessible,0,MEM_RELEASE)!=0,"fixture inaccessible page release failed");
    need(access_fault!=0&&!matched,"unreadable native label lost SEH or retained stale match");
    json trace;need(rules_numeric_trace==nullptr,"Numeric trace escaped prior request");
    {
        numeric_diagnostics::Scope scope(rules_numeric_trace,trace,true);
        setup();need(rules_property_entry(reinterpret_cast<void*>(10000),native_command_fixture::record("ValueEdited",{"61"}))&&trace["route"]["state"]=="queued"&&trace["route"]["desired"]==61&&trace["route"]["current"]==60&&design_rules.pending&&fixture_runtime.captures==1&&native_calls==0,"Numeric scoped route failed to retain genuine changed integer capture");
        setup();fixture_controller["started"]=false;rules_property_entry(reinterpret_cast<void*>(10000),native_command_fixture::record("ValueEdited",{"61"}));need(trace["route"]["state"]=="forwarded_inactive"&&trace["route"]["controller_started"]==false&&fixture_forwards==1&&!design_rules.pending,"Inactive capture route reason lost");
        setup();rules_property_entry(reinterpret_cast<void*>(10000),native_command_fixture::record("Other"));need(trace["route"]["state"]=="forwarded_other_command"&&trace["route"]["matched_value_edited"]==false&&fixture_forwards==1,"Other command capture route reason lost");
        setup();fixture_ui_fields[{30000,8}]++;rules_property_entry(reinterpret_cast<void*>(10000),native_command_fixture::record("ValueEdited",{"61"}));need(trace["route"]["state"]=="forwarded_foreign_model"&&!design_rules.pending,"Foreign model capture route reason lost");
        setup();fixture_ui_text[20000+0x2d0]="60";rules_property_entry(reinterpret_cast<void*>(10000),native_command_fixture::record("ValueEdited",{"61"}));need(trace["route"]["state"]=="forwarded_unchanged_value"&&trace["route"]["desired"]==60&&!design_rules.pending,"Unchanged numeric capture route reason lost");
        setup();fixture_parse_valid=false;rules_property_entry(reinterpret_cast<void*>(10000),native_command_fixture::record("ValueEdited",{"61"}));need(trace["route"]["state"]=="forwarded_unparsed_integer"&&trace["route"]["parsed"]==false&&!design_rules.pending,"Unparsed numeric capture route reason lost");
        setup();fixture_ui_fields[{20000,0x4a0}]=1;rules_property_entry(reinterpret_cast<void*>(10000),native_command_fixture::record("ValueEdited",{"61"}));need(trace["route"]["state"]=="rejected_binding"&&!trace["route"]["cause"].get<std::string>().empty()&&fixture_runtime.rejected==1&&!design_rules.pending,"Numeric binding failure lost its original cause");
        setup();fixture_controller["pending"]=true;rules_property_entry(reinterpret_cast<void*>(10000),native_command_fixture::record("ValueEdited",{"61"}));need(trace["route"]["state"]=="rejected_queue"&&!trace["route"]["cause"].get<std::string>().empty()&&fixture_runtime.rejected==1&&!design_rules.pending,"Numeric queue failure lost its original cause");
        need(trace["route"]["calls"]==8,"Scoped route call count lost pass-through entries");
        setup();fixture_ui_fields[{10,0x3c0}]=0;fixture_ui_fields[{20000,0x280}]=10;fixture_ui_fields[{20000,0x278}]=10000;fixture_ui_fields[{fixture_runtime.image+0x1703290,0x100}]=fixture_runtime.image+0x7f1d90;fixture_ui_fields[{20000,0x3d8}]=1;fixture_ui_fields[{20000,0x584}]=0;fixture_ui_text[20000+0x2d0]=fixture_ui_text[20000+0x4d8]="60";
        const auto phase=[&](const char* name){numeric_diagnostics::phase(rules_numeric_trace,name,fixture_runtime.image,10,20000,[](auto object,auto offset,auto& value,auto& e){return at(fixture_runtime,object,offset,value,e);},[](auto address,auto& value,auto& e){return text(fixture_runtime,address,value,128,e);});};
        phase("before_focus");fixture_ui_fields[{10,0x3c0}]=20000;fixture_ui_fields[{20000,0x584}]=1;phase("after_focus");fixture_ui_text[20000+0x2d0]="61";phase("after_set_text");fixture_ui_fields[{10,0x3c0}]=0;fixture_ui_fields[{20000,0x584}]=0;fixture_ui_text[20000+0x4d8]="61";phase("after_drop_focus");
        const auto& phases=trace["phases"];need(phases.size()==4&&!phases[0]["focus_matches_edit"].get<bool>()&&phases[1]["focus_matches_edit"].get<bool>()&&!phases[1]["text_differs_from_baseline"].get<bool>()&&phases[2]["text_differs_from_baseline"].get<bool>()&&!phases[3]["focus_matches_edit"].get<bool>()&&!phases[3]["text_differs_from_baseline"].get<bool>(),"Numeric focus/text/baseline phase evidence lost discriminator values");
        fixture_ui_text.erase(20000+0x4d8);phase("failed_baseline");need(trace["phases"].back()["state"]=="failed"&&trace["phases"].back()["field"]=="baseline"&&trace["phases"].back()["cause"]=="Native UI text absent","Numeric phase failure became success-shaped evidence");
    }
    need(rules_numeric_trace==nullptr,"Numeric scoped trace leaked after request");
    {json unused;numeric_diagnostics::Scope scope(rules_numeric_trace,unused,false);need(rules_numeric_trace==nullptr&&unused.is_null(),"Read/show operation enabled numeric tracing");}
}
