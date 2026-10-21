namespace gizmo_rename_native_fixture {
void run(unsigned& count){std::string error;reset();setup();
    fixture_runtime.memory.put(member.object,fixture_runtime.image+0x1749ab0);
    const std::string name="Changed treasure";Text native_name{name.data(),name.size(),{}};
    fixture_runtime.memory.values[uintptr_t(&native_name)]={reinterpret_cast<const uint8_t*>(&native_name),reinterpret_cast<const uint8_t*>(&native_name)+sizeof native_name};
    fixture_runtime.memory.values[uintptr_t(name.data())]={name.data(),name.data()+name.size()};
    bool handled=false;need(capture_rename(member.object,&native_name,handled,error)&&handled&&captured&&writes==0,error);++count;
    auto ordered_storage=std::make_unique<action::Request>(request());auto& ordered=*ordered_storage;ordered.peer=1;ordered.sequence=1;ordered.canonical_record_key=current.scenes[0].rows[0].key;ordered.prop_text={action::PropTextKind::Name,name,member.scene};
    captured.reset();fixture_controller["pending"]=true;
    need(perform(fixture_binding,ordered,nullptr,error)==action::Outcome::Confirmed&&writes==1,error);++count;
    auto authority_storage=std::make_unique<action::Evidence>();auto& authority=*authority_storage;need(observe(fixture_binding,ordered,authority,error)&&retire(fixture_binding,ordered,error),error);++count;
    need(perform(fixture_binding,ordered,nullptr,error)==action::Outcome::Unknown&&writes==1,"Gizmo Rename cannot replay a confirmed source");++count;
    reset();setup();current.global_random={9,8,7,6};Config::instance().is_host=false;fixture_controller["pending"]=true;
    need(perform(fixture_binding,ordered,&authority,error)==action::Outcome::Confirmed&&writes==1,error);++count;
    reset();setup();owner=false;fixture_controller["pending"]=true;
    need(perform(fixture_binding,ordered,nullptr,error)==action::Outcome::RejectedNoEffect&&writes==0&&error=="actual current canonical Gizmo scope changed","Gizmo Rename lost pre-effect owner cause");++count;
    reset();setup();fail_original=true;fixture_controller["pending"]=true;
    need(perform(fixture_binding,ordered,nullptr,error)==action::Outcome::Unknown&&writes==1&&error=="original Prop Rename SEH=5","Gizmo Rename lost partial original failure");++count;
    need(perform(fixture_binding,ordered,nullptr,error)==action::Outcome::Unknown&&writes==1,"Unknown Gizmo Rename cannot replay");++count;
    reset();setup();corrupt=true;fixture_controller["pending"]=true;
    need(perform(fixture_binding,ordered,nullptr,error)==action::Outcome::Unknown&&writes==1,"Gizmo Rename accepted unrelated inventory effects");++count;
    auto rejected_storage=std::make_unique<action::Evidence>();auto& rejected=*rejected_storage;need(!observe(fixture_binding,ordered,rejected,error),"Unknown Gizmo Rename emitted a confirmed ACK");++count;
}
}
