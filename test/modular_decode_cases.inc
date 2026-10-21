namespace {
bool modular_records_equal(const std::vector<model::Record>& left,const std::vector<model::Record>& right){
    const auto same_token=[](const auto& a,const auto& b){return a.type==b.type&&a.bits==b.bits&&a.text==b.text;};
    if(left.size()!=right.size())return false;
    for(size_t i=0;i<left.size();++i){const auto& a=left[i];const auto& b=right[i];
        if(a.parent!=b.parent||a.child_index!=b.child_index||!same_token(a.label,b.label)||a.tokens.size()!=b.tokens.size())return false;
        for(size_t j=0;j<a.tokens.size();++j)if(!same_token(a.tokens[j],b.tokens[j]))return false;
    }return true;
}
std::vector<model::Record> modular_saved_records(const Record* root,const Record* grid=nullptr,const std::vector<uint32_t>& values={}){
    std::vector<model::Record> result;
    const auto token=[](const Token& value){return model::Token{value.type,value.bits,std::string(value.text.data,value.text.length)};};
    const auto visit=[&](auto&& self,const Record* row,size_t parent,size_t child)->void{
        const auto index=result.size();result.push_back({parent,child,token(row->label),{}});
        if(row==grid){
            // The original integer-array Save writer emits anonymous one-token
            // child Records, including a single empty child for an empty array.
            if(values.empty())result.push_back({index,0,{},{}});
            else for(size_t i=0;i<values.size();++i)result.push_back({index,i,{},{{4,values[i],{}}}});
        }else{
            for(int32_t i=0;i<row->tokens.count;++i)result[index].tokens.push_back(token(static_cast<const Token*>(row->tokens.data)[i]));
            for(int32_t i=0;i<row->children.count;++i)self(self,static_cast<Record* const*>(row->children.data)[i],index,size_t(i));
        }
    };
    visit(visit,root,SIZE_MAX,0);return result;
}
void modular_grid_decode_cases(const api::DecodeApi& funcs,Memory& memory){std::string error;
    const std::vector<model::Record> saved{
        {SIZE_MAX,0,{1,0,"mmoBuildingLayout"},{}},
        {0,0,{1,0,"dimX"},{{4,2,{}}}},
        {0,1,{1,0,"grid"},{}},
        {2,0,{},{{4,17,{}}}},{2,1,{},{{4,0xffffffff,{}}}},
        {0,2,{1,0,"dimY"},{{4,1,{}}}},{0,3,{1,0,"dimZ"},{{4,1,{}}}},
        {0,4,{1,0,"other"},{}},{7,0,{1,0,"grid"},{}},{8,0,{},{{4,23,{}}}}
    };
    auto source=saved;api::NativeTree tree;api::DecodeReceipt receipt;
    check(api::decode_modular_record(funcs,memory,false,source,tree,receipt,error),"Modular original child-form Layout grid decode");
    const auto* root=reinterpret_cast<Record*>(tree.root);const auto* grid=static_cast<Record* const*>(root->children.data)[1];
    // The exact original child branch loops over the prior array length, zero
    // for a factory Layout. Its token branch sets length from the Record first.
    std::vector<uint32_t> loaded;
    if(!grid->children.count)for(int32_t i=0;i<grid->tokens.count;++i)loaded.push_back(static_cast<const Token*>(grid->tokens.data)[i].bits);
    check(loaded==std::vector<uint32_t>{17,0xffffffff},"Modular fresh native array load would leave the grid empty or alter bits");
    check(modular_records_equal(modular_saved_records(root,grid,loaded),saved),"Modular original writer roundtrip changed grid, sibling order or another subtree");
    check(modular_records_equal(source,saved),"Modular detached normalization changed the immutable input");
    check(receipt.allocated==saved.size()-2&&receipt.attached==saved.size()-3&&tree.detached.size()==1,"Modular flattened grid native ownership differs");
    check(api::destroy_records(funcs,tree,receipt,error)&&allocated==released,"Modular grid cleanup is unbalanced");
    const auto reject=[&](const std::vector<model::Record>& input,const char* message){const auto before=allocated;
        check(!api::decode_modular_record(funcs,memory,false,input,tree,receipt,error)&&allocated==before&&!tree.root&&tree.detached.empty()&&!error.empty(),message);
    };
    auto bad=saved;bad[3].tokens[0].type=3;reject(bad,"Modular noninteger grid child reached allocation");
    bad=saved;bad[3].tokens.clear();reject(bad,"Modular partial empty grid child reached allocation");
    bad=saved;bad[3].tokens.push_back({4,1,{}});reject(bad,"Modular multitoken grid child reached allocation");
    bad=saved;bad[3].label={1,0,"unexpected"};reject(bad,"Modular labelled grid child reached allocation");
    bad=saved;bad.push_back({3,0,{},{{4,1,{}}}});reject(bad,"Modular nested grid cell reached allocation");
    bad=saved;bad.push_back({0,5,{1,0,"grid"},{}});reject(bad,"Modular repeated root grid reached allocation");
    bad=saved;bad[2].tokens.push_back({4,1,{}});reject(bad,"Modular mixed grid forms reached allocation");
    bad=saved;bad[4].child_index=2;reject(bad,"Modular partial grid child order reached allocation");
    source=saved;source[0].label.text="mmoBuildingPlan";
    check(api::decode_modular_record(funcs,memory,true,source,tree,receipt,error),"Modular Plan decode rejected unrelated child-form grid");
    check(modular_records_equal(modular_saved_records(reinterpret_cast<Record*>(tree.root)),source),"Modular Layout normalization altered Plan records");
    check(api::destroy_records(funcs,tree,receipt,error)&&allocated==released,"Modular Plan grid cleanup is unbalanced");
    source={{SIZE_MAX,0,{1,0,"mmoBuildingLayout"},{}},{0,0,{1,0,"grid"},{{4,17,{}},{4,0xffffffff,{}}}}};
    check(api::decode_modular_record(funcs,memory,false,source,tree,receipt,error),"Modular existing integer token grid rejected");
    check(modular_records_equal(modular_saved_records(reinterpret_cast<Record*>(tree.root)),source),"Modular existing token-form grid changed");
    check(api::destroy_records(funcs,tree,receipt,error)&&allocated==released,"Modular token grid cleanup is unbalanced");
    source={{SIZE_MAX,0,{1,0,"mmoBuildingLayout"},{}},{0,0,{1,0,"grid"},{}},{1,0,{},{}}};
    check(api::decode_modular_record(funcs,memory,false,source,tree,receipt,error),"Modular original empty grid marker rejected");
    root=reinterpret_cast<Record*>(tree.root);grid=static_cast<Record* const*>(root->children.data)[0];
    check(grid->tokens.count==0&&grid->children.count==0&&modular_records_equal(modular_saved_records(root,grid),source),"Modular empty grid writer roundtrip differs");
    check(api::destroy_records(funcs,tree,receipt,error)&&allocated==released,"Modular empty grid cleanup is unbalanced");
}
void modular_decode_cases(const api::DecodeApi& funcs,Memory& memory){std::string error;
    modular_grid_decode_cases(funcs,memory);
    for(bool plan:{false,true}){const auto* name=plan?"mmoBuildingPlan":"mmoBuildingLayout";std::vector<model::Record> source{{SIZE_MAX,0,{1,0,name},{}},{0,0,{1,0,"blocks"},{{4,7,{}},{3,0xbf800000,{}},{2,0,"stone"}}},{0,1,{1,0,"width"},{{4,3,{}}}}};api::NativeTree tree;api::DecodeReceipt receipt;
        check(api::decode_modular_record(funcs,memory,plan,source,tree,receipt,error),"Modular complete typed original Record decode");const auto* root=reinterpret_cast<Record*>(tree.root);check(std::string(root->label.text.data)==name&&root->children.count==2&&receipt.allocated==3&&receipt.attached==2&&tree.detached.size()==1,"Modular native tree root/order/ownership differs");
        const auto* child=static_cast<Record**>(root->children.data)[0];const auto* values=static_cast<Token*>(child->tokens.data);check(child->tokens.count==3&&values[0].type==4&&values[0].bits==7&&values[1].type==3&&values[1].bits==0xbf800000&&values[2].type==2&&std::string(values[2].text.data)=="stone","Modular exact token bits/text/order differ");
        check(!api::decode_modular_record(funcs,memory,plan,source,tree,receipt,error),"Modular already owned native tree replaced");check(api::destroy_records(funcs,tree,receipt,error)&&allocated==released,"Modular complete native tree cleanup is unbalanced");const auto before=allocated;
        check(!api::decode_modular_record(funcs,memory,!plan,source,tree,receipt,error)&&allocated==before,"Modular wrong typed root reached native allocation");auto malformed=source;malformed[1].parent=1;check(!api::decode_modular_record(funcs,memory,plan,malformed,tree,receipt,error)&&allocated==before,"Modular self-parent reached native allocation");
        auto missing=funcs;missing.add_child=nullptr;check(!api::decode_modular_record(missing,memory,plan,source,tree,receipt,error)&&allocated==before,"Modular missing native API reached allocation");
    }
    std::vector<model::Record> source{{SIZE_MAX,0,{1,0,"mmoBuildingLayout"},{}},{0,0,{1,0,"blocks"},{{4,7,{}}}}};api::NativeTree tree;api::DecodeReceipt receipt;crash_add=true;
    check(!api::decode_modular_record(funcs,memory,false,source,tree,receipt,error)&&receipt.unknown&&receipt.exception==0xc0000005,"Modular original attach fault cause lost");const auto before=released;check(!api::destroy_records(funcs,tree,receipt,error)&&released==before,"Modular uncertain native attachment destroyed or replayed");
    // The injected fake AddChild fault precedes ownership transfer.
    for(auto pointer:tree.detached){destroy(reinterpret_cast<void*>(pointer));release(reinterpret_cast<void*>(pointer));}crash_add=false;
}
}
