namespace research_release {
struct Change {
    uintptr_t address=0,owner=0;
    std::string key;
    int32_t order=0;
    uint8_t feature=0;
    std::vector<uintptr_t> children;
    bool operator==(const Change&)const=default;
};
struct Graph {
    uintptr_t release=0;
    std::vector<uintptr_t> features,minor;
    std::vector<Change> changes;
    bool operator==(const Graph&)const=default;
};
bool array(uintptr_t owner,size_t offset,std::vector<uintptr_t>& out,std::string& e){
    uintptr_t data=0;int32_t count=0,capacity=0;
    if(!at(owner,offset+8,data,e)||!at(owner,offset+16,count,e)||!at(owner,offset+20,capacity,e))return false;
    if(count<0||capacity<count||capacity>4096||(!data&&count))return fail(e,"Research Release array bounds differ");
    out.resize(size_t(count));std::set<uintptr_t> unique;
    for(size_t i=0;i<out.size();++i){
        if(!at(data,i*sizeof(uintptr_t),out[i],e))return false;
        if(!out[i]||!unique.insert(out[i]).second)return fail(e,"Research Release array null/duplicate member");
    }
    return true;
}
bool read(uintptr_t release,Graph& out,std::string& e){
    Graph next;next.release=release;
    if(!type(release,0x16c12b0,e)||!array(release,0x148,next.features,e)||!array(release,0x178,next.minor,e))return false;
    auto roots=next.features;roots.insert(roots.end(),next.minor.begin(),next.minor.end());
    std::set<uintptr_t> seen;
    for(auto root:roots){
        if(!seen.insert(root).second)return fail(e,"Research Release root belongs to multiple arrays");
        Change node;node.address=root;
        if(!type(root,0x1786d10,e)||!at(root,0x70,node.owner,e)||!str(root,0x18,node.key,e)||
           !at(root,0xe8,node.order,e)||!at(root,0xec,node.feature,e)||!array(root,0xb8,node.children,e))return false;
        if(node.feature>1)return fail(e,"Research Release Change feature is not Boolean");
        next.changes.push_back(node);
        // AddFeature only visits the immediate children of feature/minor roots.
        // Keep each child's identity and contents, including its own child list.
        for(auto child:node.children){
            if(!seen.insert(child).second)return fail(e,"Research Release child has duplicate ownership");
            Change value;value.address=child;
            if(!type(child,0x1786d10,e)||!at(child,0x70,value.owner,e)||!str(child,0x18,value.key,e)||
               !at(child,0xe8,value.order,e)||!at(child,0xec,value.feature,e)||!array(child,0xb8,value.children,e))return false;
            if(value.feature>1)return fail(e,"Research Release child feature is not Boolean");
            next.changes.push_back(std::move(value));
        }
    }
    std::sort(next.changes.begin(),next.changes.end(),[](const auto& a,const auto& b){return a.address<b.address;});
    out=std::move(next);return true;
}
bool admissible(const Graph& graph,uintptr_t tech,std::string& e){
    for(const auto& row:graph.changes)if(row.owner==tech)
        return fail(e,"Research Commit selected Tech already has a Release reference");
    return true;
}
bool preserve(const Graph& before,const Graph& after,uintptr_t tech,nlohmann::json& receipt,std::string& e){
    if(before.release!=after.release||before.minor!=after.minor||after.features.size()!=before.features.size()+1||
       after.changes.size()!=before.changes.size()+1)return fail(e,"Research Commit Release graph count/owner differs");
    for(auto ptr:before.features)if(std::count(after.features.begin(),after.features.end(),ptr)!=1)
        return fail(e,"Research Commit removed/replaced existing Release feature");
    for(const auto& row:before.changes){
        const auto found=std::find_if(after.changes.begin(),after.changes.end(),[&](const auto& value){return value.address==row.address;});
        if(found==after.changes.end()||*found!=row)return fail(e,"Research Commit changed existing Release member/children");
    }
    for(const auto& row:after.changes)if(std::none_of(before.changes.begin(),before.changes.end(),[&](const auto& old){return old.address==row.address;})){
        if(row.owner!=tech||row.key!="tech_release"||row.feature!=1||!row.children.empty()||
           std::count(after.features.begin(),after.features.end(),row.address)!=1)
            return fail(e,"Research Commit inserted Release member differs from selected Tech");
    }
    receipt={{"preserved",true},{"features_before",before.features.size()},{"features_after",after.features.size()},
        {"minor_before",before.minor.size()},{"minor_after",after.minor.size()},
        {"members_before",before.changes.size()},{"members_after",after.changes.size()}};
    return true;
}
}
