namespace {
void modular_decode_cases(const api::DecodeApi& funcs,Memory& memory){std::string error;
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
