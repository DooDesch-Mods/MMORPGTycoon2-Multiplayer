namespace display_native {
struct Cell {int32_t id=0,row=0,job=0;uint8_t employed=0,on_map=0;uintptr_t developer=0,field=0;bool operator==(const Cell&)const=default;};
struct UI {uintptr_t window=0,summary=0,summary_value=0,project=0,grid=0;std::vector<Cell> cells;bool operator==(const UI&)const=default;};
bool array(uintptr_t owner,size_t offset,uintptr_t& data,int32_t& count,int32_t limit,std::string& e){
    int32_t capacity=0;if(!at(owner,offset+8,data,e)||!at(owner,offset+0x10,count,e)||!at(owner,offset+0x14,capacity,e))return false;
    return data&&count>=0&&count<=limit&&capacity>=count&&capacity<=limit*2||fail(e,"Staff display native cell array bounds differ");
}
bool ancestry(uintptr_t child,uintptr_t window,uintptr_t required,std::string& e){
    bool has_required=!required;std::set<uintptr_t> seen;
    for(unsigned i=0;child&&child!=window&&i<32;++i){uintptr_t owner=0;if(!seen.insert(child).second||!at(child,0x280,owner,e)||!at(child,0x278,child,e))return fail(e,e.empty()?"Staff display parent cycle":e);
        if(owner!=window)return fail(e,"Staff display child belongs to another Window");has_required|=child==required;
    }return child==window&&has_required||fail(e,"Staff display child does not descend from active owned page");
}
bool page(uintptr_t view,uintptr_t window,const char* name,uintptr_t& active,std::string& e){
    int32_t transition=0;std::string requested,current;
    if(!type(view,0x16dfe90,e)||!ancestry(view,window,0,e)||!at(view,0x410,active,e)||!at(view,0x408,transition,e)||!str(view,0x3e8,requested,e)||!str(active,0x1b0,current,e))return false;
    return requested==name&&current==name&&transition==0||fail(e,"Staff display page not ready: requested="+requested+" active="+current+" transition="+std::to_string(transition));
}
bool ui_pin(const Cache& bound,uintptr_t window,UI& out,std::string& e){
    out={};World current;if(!origin(bound.context,bound.binding,current,e)||!same_world(current,bound.world))return fail(e,e.empty()?"Staff display full local Origin changed":e);
    native::ClockState clock;if(!native::read_clock(clock,e))return false;if(clock.time_scale!=0)return fail(e,"Staff display Client actual Clock is not0");
    uintptr_t lounge=0,project=0;int32_t kind=0;std::string key;
    if(!at(reinterpret_cast<uintptr_t>(current.state),0x180,lounge,e)||!at(lounge,0xa8,project,e)||lounge!=bound.owners.snapshot.lounge||project!=bound.project)
        return fail(e,e.empty()?"Staff display current Lounge/project owner changed":e);
    if(!type(project,0x16e48c0,e)||!at(project,0x18,kind,e)||!str(project,0x20,key,e))return false;
    if(kind||!bound.owners.snapshot.creation||!bound.owners.snapshot.creation->current||
       key!=bound.owners.snapshot.creation->current->key||(key!="startingpoint"&&key!="bridges"))return fail(e,"Staff display current project kind/key differs");
    const auto mode=reinterpret_cast<uintptr_t>(current.mode);uintptr_t windows=0;int32_t count=0,capacity=0;unsigned matches=0;
    if(!at(mode,0xa0,windows,e)||!at(mode,0xa8,count,e)||!at(mode,0xac,capacity,e))return false;
    if(!windows||count<1||capacity<count||capacity>512)return fail(e,"Staff display Mode window array bounds differ");
    for(int32_t i=0;i<count;++i){uintptr_t candidate=0;if(!at(windows,size_t(i)*8,candidate,e))return false;if(candidate==window)++matches;}
    uint8_t visible=0,pending=0;uintptr_t logic=0;std::string name;
    if(matches!=1||!type(window,0x1722e60,e)||!str(window,0x1b0,name,e)||name!="development")return fail(e,e.empty()?"Staff display UpdateUI receiver is not actual owned DevelopmentWindow":e);
    if(!at(window,0x45f,visible,e)||!at(window,0x460,pending,e)||!at(window,0x318,logic,e))return false;
    if(visible!=1||pending||logic)return fail(e,"Staff display Window hidden/layout pending/foreign logic");
    uintptr_t tabs=0,view=0,staff=0,project_view=0,dev=0;
    if(!at(window,0x540,tabs,e)||!type(tabs,0x17015b0,e)||!ancestry(tabs,window,0,e)||!at(tabs,0x3b8,view,e)||!page(view,window,"{dev_tab_staff}",staff,e)||
       !at(window,0x568,project_view,e)||!page(project_view,window,"dev",dev,e))return false;
    out.window=window;
    if(!at(window,0x508,out.summary,e)||!type(out.summary,0x1700630,e)||!ancestry(out.summary,window,staff,e)||
       !at(window,0x5c8,out.project,e)||!type(out.project,0x16dc680,e)||!ancestry(out.project,window,dev,e)||
       !at(window,0x510,out.grid,e)||!type(out.grid,0x16dade0,e)||!ancestry(out.grid,window,staff,e))return false;
    uintptr_t rows=0;int32_t row_count=0;if(!array(out.summary,0x3d8,rows,row_count,64,e))return false;
    if(row_count<=1||rows>UINTPTR_MAX-0x20)return fail(e,"Staff display summary row1 absent/overflow");
    uintptr_t fields=0;int32_t field_count=0;if(!array(rows+0x20,0,fields,field_count,64,e))return false;
    if(field_count<=1||fields>UINTPTR_MAX-0x50)return fail(e,"Staff display summary column1 absent/overflow");out.summary_value=fields+0x50;
    uintptr_t grid_fields=0;int32_t grid_count=0,columns=0,hired=0;
    if(!at(out.grid,0x314,columns,e)||!array(out.grid,0x3d0,grid_fields,grid_count,512,e)||!at(lounge,0x310,hired,e))return false;
    if(columns<3||columns>16||hired!=bound.owners.count)return fail(e,"Staff display grid columns or hired count changed");
    uintptr_t developer_data=0;int32_t developer_count=0;
    if(!at(lounge,0x70,developer_data,e)||!at(lounge,0x78,developer_count,e))return false;
    if(!bound.owners.snapshot.creation||developer_data!=bound.owners.snapshot.creation->developer_data||developer_count!=50)
        return fail(e,"Staff display owned Developer catalogue changed");
    std::set<uintptr_t> field_owners;
    for(const auto& d:bound.owners.developers)if(d.row<hired){Cell cell;cell.id=d.id;cell.row=d.row;cell.developer=d.address;
        int32_t actual_id=0;uint8_t wanted=0;uintptr_t actual_developer=0;
        if(!at(developer_data,size_t(d.row)*8,actual_developer,e)||actual_developer!=d.address)return fail(e,e.empty()?"Staff display Developer catalogue row changed":e);
        if(!type(d.address,0x17422e0,e)||!at(d.address,0x4c0,actual_id,e)||!at(d.address,0x4c4,cell.job,e)||!at(d.address,0x4d8,wanted,e)||!at(d.address,0x4d9,cell.employed,e)||!at(d.address,0x368,cell.on_map,e))return false;
        if(actual_id!=d.id||wanted!=1||cell.employed!=1||cell.on_map>1)return fail(e,"Staff display visible Developer identity/employment changed");
        const int32_t index=d.row*columns+1;if(index<0||index>=grid_count||!at(grid_fields,size_t(index)*8,cell.field,e))return fail(e,e.empty()?"Staff display activity cell outside Grid array":e);
        if(!type(cell.field,0x1789a90,e)||!field_owners.insert(cell.field).second)return fail(e,e.empty()?"Staff display activity TextField duplicated":e);
        out.cells.push_back(cell);
    }
    World after;if(!origin(bound.context,bound.binding,after,e)||!same_world(current,after))return fail(e,e.empty()?"Staff display Origin changed during UI pin":e);return true;
}
struct Calls {
    const Cache& bound;uintptr_t window;UI before;json expected_summary,expected_project,expected_developers=json::array();bool complete=false;
    Calls(const Cache& c,uintptr_t w):bound(c),window(w){}
    bool pin(const staff_display::State&,std::string& e){return ui_pin(bound,window,before,e);}
    bool prepare(const staff_display::State& target,std::string& e){
        for(const auto& cell:before.cells){auto it=std::find_if(target.developers.begin(),target.developers.end(),[&](const auto& d){return d.id==cell.id;});if(it==target.developers.end())return fail(e,"Staff display Host ID missing for visible local row");}
        return code_result(native_complete(bound.owners.snapshot.lounge,complete),"implementation complete getter",e);
    }
    bool summary(int32_t n,std::string& e){Value value;if(!value.number(n,e)||!loc(reinterpret_cast<uintptr_t>(value.value.bytes),expected_summary,e))return false;
        if(!code_result(native_summary(before.summary,value.value.bytes),"summary SetText",e))return false;return value.close(e);}
    bool project(int32_t active,std::string& e){Value value;if(!value.literal(staff_display::project_key(complete,active),e)||!loc(reinterpret_cast<uintptr_t>(value.value.bytes),expected_project,e))return false;
        if(!code_result(native_project(before.project,value.value.bytes),"project SetText",e))return false;return value.close(e);}
    bool developer(const staff_display::Developer& d,std::string& e){
        const auto it=std::find_if(before.cells.begin(),before.cells.end(),[&](const auto& cell){return cell.id==d.id;});if(it==before.cells.end())return true;
        Value value;json expected;const Borrowed empty{"",0,{}};
        if(!activity_value(d,bound.project,value,e)||!loc(reinterpret_cast<uintptr_t>(value.value.bytes),expected,e))return false;
        expected_developers.push_back({{"id",d.id},{"row",it->row},{"activity",expected}});
        if(!code_result(native_developer(before.grid,it->row,value.value.bytes,&empty),"activity SetText",e))return false;return value.close(e);
    }
    bool readback(const staff_display::State& target,std::string& e){
        UI after;if(!ui_pin(bound,window,after,e)||before!=after)return fail(e,e.empty()?"Staff display UI/Developer owners changed during native setters":e);
        json summary,project,developers=json::array();
        if(!loc(after.summary_value,summary,e)||!loc(after.project+0x2c8,project,e))return false;
        for(const auto& cell:after.cells){json activity;if(!loc(cell.field+0x10,activity,e))return false;developers.push_back({{"id",cell.id},{"row",cell.row},{"activity",activity}});}
        std::sort(expected_developers.begin(),expected_developers.end(),[](const auto& a,const auto& z){return a.at("row").template get<int32_t>()<z.at("row").template get<int32_t>();});
        if(summary!=expected_summary||project!=expected_project||developers!=expected_developers)return fail(e,"Staff display actual native cell readback differs");
        receipt.visible={{"active_developers",target.active_developers},{"active_value",summary},{"project_state_key",project.at("key")},{"project_state",project},{"developers",developers}};
        return true;
    }
};
}
