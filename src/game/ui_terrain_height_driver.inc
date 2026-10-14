namespace {
uint32_t height_ui_sample(uintptr_t image,uintptr_t map,std::array<float,3>& point){
    __try{point[1]=reinterpret_cast<float(*)(void*,const void*)>(image+0x32ab80)(reinterpret_cast<void*>(map),point.data());return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t height_ui_input(uintptr_t image,unsigned step,const placement_ui_probe::Binding& b,const std::array<float,3>& point){
    __try{
        if(step==0)reinterpret_cast<Unary>(image+0x580870)(reinterpret_cast<void*>(b.button));
        // Native Press resets the cell cache and tail-calls virtual Drag once.
        else if(step==1)reinterpret_cast<Press>(image+0x5e1260)(reinterpret_cast<void*>(b.behaviour),reinterpret_cast<void*>(b.cursor),point.data());
        else if(step==2)reinterpret_cast<Press>(image+0xbf6480)(reinterpret_cast<void*>(b.behaviour),reinterpret_cast<void*>(b.cursor),point.data());
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t height_ui_borrow(uintptr_t cursor,uintptr_t model,const std::array<float,3>& point,float radius,std::array<float,3>& prior,float& prior_radius,bool& borrowed){
    __try{
        std::memcpy(prior.data(),reinterpret_cast<const void*>(model+0xe0),12);prior_radius=*reinterpret_cast<float*>(cursor+0x170);borrowed=true;
        std::memcpy(reinterpret_cast<void*>(model+0xe0),point.data(),12);*reinterpret_cast<float*>(cursor+0x170)=radius;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t height_ui_restore(uintptr_t cursor,uintptr_t model,const std::array<float,3>& prior,float radius){
    __try{std::memcpy(reinterpret_cast<void*>(model+0xe0),prior.data(),12);*reinterpret_cast<float*>(cursor+0x170)=radius;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool height_ui_code(region::Runtime& r,std::string& e){
    for(const auto& code:height_ui_codes){std::vector<uint8_t> actual(code.size);if(!r.memory.copy(r.image+code.rva,actual.data(),actual.size(),e))return false;if(std::memcmp(actual.data(),code.bytes,code.size))return fail(e,"height UI original body differs RVA="+std::to_string(code.rva));}return true;
}
}
bool native_height(const json& args,json& out,std::string& e){
    out=json::object();auto* r=runtime(e);if(!r)return false;
    if(!terrain.ready||unresolved()||placement_ui_diagnostics::unresolved())return fail(e,"height actual UI requires ready idle capture and no unresolved UI job");
    const auto controller=typed_action_runtime::status();
    if(!net::session_wire::connected()||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))return fail(e,"height actual UI requires connected idle Controller");
    if(!args.is_object()||args.size()!=5||!args.contains("token")||!args.at("token").is_string()||!args.contains("id")||!args.at("id").is_number_integer()||!args.contains("cell")||!args.at("cell").is_number_integer()||!args.contains("up")||!args.at("up").is_boolean()||!args.contains("radius")||!args.at("radius").is_number())return fail(e,"height native UI requires token, id, cell, up, radius");
    const auto token=args.at("token").get<std::string>();const auto id=args.at("id").get<int64_t>(),cell=args.at("cell").get<int64_t>();const auto up=args.at("up").get<bool>();const auto radius=args.at("radius").get<double>();
    bool uuid=token.size()==36;for(size_t i=0;uuid&&i<token.size();++i)uuid=(i==8||i==13||i==18||i==23)?token[i]=='-':(token[i]>='0'&&token[i]<='9')||(token[i]>='a'&&token[i]<='f');
    if(!uuid||terrain.ui_jobs.contains(token)||id<0||id>INT32_MAX||cell<0||cell>=1000000||!std::isfinite(radius)||radius<1||radius>1024)return fail(e,"height native UI token/id/cell/radius invalid or already attempted");
    if(!native_code(*r,true,e)||!height_code(*r,true,e)||!height_ui_code(*r,e))return false;
    region::LocalView before;terrain_height::Grid grid;std::string digest;uint8_t owned=0;
    if(!region::identity(*r,int32_t(id),0,before,e)||!region::at(*r,before.region,0x850,owned,e)||owned!=1)return fail(e,e.empty()?"height native UI requires owned Region":e);
    if(!height_grid(*r,before.region,grid,digest,e)||size_t(cell)>=grid.values.size())return fail(e,e.empty()?"height native UI cell outside actual grid":e);
    std::array<float,3> point{grid.origin[0]+(float(cell%grid.width)+0.5f)*grid.cell_size,0,grid.origin[2]+(float(cell/grid.width)+0.5f)*grid.cell_size};
    uintptr_t map=0,ap=0,receiver=0;if(!region::at(*r,r->image,0x13df8c0,map,e)||!region::at(*r,map,0,ap,e)||ap!=r->image+0x1742860)return fail(e,e.empty()?"height native UI actual Map owner differs":e);
    if(!native_error(height_ui_sample(r->image,map,point),"height UI Map.GetHeightAt",e))return false;
    int32_t actual_cell=-1;for(float value:point)if(!std::isfinite(value))return fail(e,"height native UI cell center not finite");
    if(point==std::array<float,3>{}||!terrain_height::index(grid,point,actual_cell,e)||actual_cell!=cell||!height_region_at(*r,point,before.region,receiver,e))return fail(e,e.empty()?"height native UI cell center/Region differs":e);
    placement_ui_probe::Context context{r->image,uintptr_t(before.world.origin.state),before.world.origin.world_epoch,before.world.origin.generation,r->thread};
    placement_ui_probe::Request request;request.token=token;request.family=placement_ui_probe::Family::TerrainHeight;request.terrain_style=up?1:0;
    placement_ui_probe::ProcessMemory memory;placement_ui_probe::Binding binding;uintptr_t cursor=0,model=0;
    if(!placement_ui_probe::read_existing_tool(memory,context,request,binding,e)||!height_cursor(*r,binding.cursor,cursor,model,e))return false;
    auto& job=terrain.ui_jobs[token];job={{"token",token},{"id",id},{"cell",cell},{"up",up},{"radius",float(radius)},{"picked",point},{"before_sha256",digest},{"before_bits",std::bit_cast<uint32_t>(grid.values[size_t(cell)])},{"attempted",0},{"returned",0},{"captured",false},{"submitted",false},{"action_uuid",""},{"cause",""},{"input_restored",false}};
    terrain.ui_token=token;struct Reset{~Reset(){terrain.ui_token.clear();}} reset;const auto captured=terrain.captured;
    auto call=[&](unsigned step){job["attempted"]=job.at("attempted").get<unsigned>()+1;const auto seh=height_ui_input(r->image,step,binding,point);if(seh){e="height original UI callback SEH="+std::to_string(seh)+" step="+std::to_string(step);return false;}job["returned"]=job.at("returned").get<unsigned>()+1;return true;};
    bool ok=call(0);uintptr_t active=0,drag=0,release=0;uint8_t actual_up=0;
    if(ok)ok=region::at(*r,cursor,0x190,active,e)&&active==binding.behaviour&&region::at(*r,active,0,ap,e)&&ap==r->image+0x1740010&&region::at(*r,active,0x148,actual_up,e)&&actual_up==(up?1:0)&&region::at(*r,ap,0xe0,drag,e)&&drag==r->image+0x5e1860&&region::at(*r,ap,0xe8,release,e)&&release==r->image+0xbf6480;
    job["selection_confirmed"]=ok;
    if(!ok&&e.empty())e="height UI active native owner/direction/Drag/Release differs; measured_up="+std::to_string(actual_up)+" required_up="+std::to_string(up?1:0);
    std::array<float,3> prior{};float prior_radius=0;bool borrowed=false;
    if(ok)ok=native_error(height_ui_borrow(cursor,model,point,float(radius),prior,prior_radius,borrowed),"height UI borrow",e);
    if(ok)ok=call(1);
    // Release is native no-op; it closes this input sequence even if capture failed.
    if(borrowed){const auto original=e;const bool released=call(2);if(!original.empty())e=original+(released?"":"; "+e);ok=ok&&released;const auto restore=height_ui_restore(cursor,model,prior,prior_radius);job["input_restored"]=restore==0;if(restore){e+="; height UI restoration SEH="+std::to_string(restore);ok=false;}}
    std::array<float,3> measured{};float measured_radius=0;
    if(borrowed&&job.at("input_restored").get<bool>()){std::string read_error;if(!region::at(*r,model,0xe0,measured,read_error)||!region::at(*r,cursor,0x170,measured_radius,read_error)||measured!=prior||measured_radius!=prior_radius){ok=false;job["input_restored"]=false;if(read_error.empty())read_error="height UI restored native input readback differs";e=e.empty()?read_error:e+"; "+read_error;}}
    const bool capture=terrain.captured==captured+1&&terrain.pending&&terrain.pending->ui_token==token&&terrain.pending->intent.id==id&&terrain.pending->intent.height&&terrain.pending->intent.height->cell==cell&&terrain.pending->intent.height->up==up&&terrain.pending->intent.height->picked==point&&terrain.pending->intent.height->hit==point&&terrain.pending->intent.height->radius==float(radius)&&terrain.pending->intent.height->previous==std::array<float,3>{}&&terrain.pending->intent.height->before_sha256==digest;
    job["captured"]=capture;job["drag_captured"]=capture;
    if(!ok||!capture){if(e.empty())e="height original Press/virtual Drag returned without one exact captured cell: "+terrain.cause;job["cause"]=e;out=job;return false;}
    out=job;return true;
}
