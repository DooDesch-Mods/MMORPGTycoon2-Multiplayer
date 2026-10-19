namespace world_path_runtime {
bool wall_private_ready(region::Runtime& runtime,std::string& error){
    for(const auto& pin:wall_private_native::codes){
        std::string bytes(pin.size,'\0'),actual;
        if(!runtime.memory.copy(runtime.image+pin.rva,bytes.data(),bytes.size(),error)||
           !region::sha256(bytes,actual,error))return false;
        if(actual!=pin.sha256){error="Wall private original body differs RVA="+std::to_string(pin.rva);return false;}
    }
    return true;
}
namespace wall_private {
using Receipt=wall_private_native::Receipt;
using Point=std::array<float,3>;
struct StringView {const char* data;size_t size;unsigned char storage[16]{};};
static_assert(sizeof(StringView)==32);
struct Plan {
    uintptr_t region=0,start_node=0,end_node=0,start_split=0,end_split=0;
    bool start_preview=false,end_preview=false;
    std::vector<Point> preview;
};
bool prepare_plan(const action::WorldPathIntent& intent,const mixed_path_native::Census& census,
                  const std::map<uint64_t,std::string>& keys,Plan& out){
    if(intent.kind!=action::WorldPathKind::Wall||!action::world_path_valid(intent))return false;
    for(const auto& spline:intent.segments)for(auto bits:spline)
        if(!std::isfinite(std::bit_cast<float>(bits)))return false;
    Plan plan;
    for(const auto& row:census.regions)if(row.id==intent.start_region){
        if(plan.region||!row.object)return false;plan.region=row.object;}
    if(!plan.region)return false;
    auto resolve=[&](const std::string& key,bool path,uintptr_t& result){
        if(key.empty())return true;
        auto take=[&](const auto& rows){for(const auto& row:rows){const auto found=keys.find(row.member.uid);
            if(found!=keys.end()&&found->second==key){if(result||!row.member.object||!row.member.uid)return false;result=row.member.object;}}
            return result!=0;};
        return path?take(census.paths):take(census.nodes);
    };
    plan.start_preview=intent.start_split=="preview";plan.end_preview=intent.end_split=="preview";
    if(!resolve(intent.start_node,false,plan.start_node)||!resolve(intent.end_node,false,plan.end_node)||
       (!plan.start_preview&&!resolve(intent.start_split,true,plan.start_split))||
       (!plan.end_preview&&!resolve(intent.end_split,true,plan.end_split)))return false;
    for(const auto& bits:intent.preview)plan.preview.push_back({std::bit_cast<float>(bits[0]),
        std::bit_cast<float>(bits[1]),std::bit_cast<float>(bits[2])});
    out=std::move(plan);return true;
}
struct Native {
    uintptr_t image;
    const action::WorldPathIntent& intent;
    const Plan& plan;
    unsigned char* receiver=nullptr;
    unsigned char* ghost=nullptr;
    bool ghost_constructed=false,receiver_constructed=false;
    Point endpoint{};
    template<class T>static T& at(void* object,size_t offset){
        return *reinterpret_cast<T*>(static_cast<unsigned char*>(object)+offset);
    }
    uint32_t prepare(Receipt& receipt){
        __try {
            if(at<uintptr_t>(reinterpret_cast<void*>(plan.region),0)!=image+0x1787930)return ERROR_INVALID_DATA;
            const auto shard=at<uintptr_t>(reinterpret_cast<void*>(plan.region),0x70);
            if(!shard)return ERROR_INVALID_DATA;
            const auto scene=reinterpret_cast<uintptr_t(*)(uintptr_t)>(image+0x507ba0)(shard);
            if(!scene)return ERROR_INVALID_DATA;
            receipt.shard=shard;receipt.scene=scene;
            receiver=reinterpret_cast<unsigned char*>(reinterpret_cast<void*(*)(size_t)>(image+0xa02670)(0x2d8));
            receipt.receiver=uintptr_t(receiver);
            if(!receiver)return ERROR_OUTOFMEMORY;
            reinterpret_cast<void(*)(void*)>(image+0x5ee3d0)(receiver);
            receiver_constructed=true;
            if(at<uintptr_t>(receiver,0)!=image+0x172f8d0||!at<uintptr_t>(receiver,0x178)||
               at<uintptr_t>(receiver,0x180)||at<uintptr_t>(receiver,0x188)||
               at<uintptr_t>(receiver,0x1b0)||at<uintptr_t>(receiver,0x1b8))return ERROR_INVALID_DATA;
            const StringView name{intent.name.data(),intent.name.size(),{}};
            reinterpret_cast<void(*)(void*,const void*)>(image+0x11bfce0)(receiver+0x190,&name);
            at<int32_t>(receiver,0x148)=intent.type;
            at<int32_t>(receiver,0x14c)=intent.node_type;
            at<uint32_t>(receiver,0x158)=intent.quoted_length;
            const auto manager=reinterpret_cast<uintptr_t(*)()>(image+0xb2dc70)();
            if(!manager)return ERROR_INVALID_DATA;
            auto make_model=reinterpret_cast<uintptr_t(*)(uintptr_t,const void*,bool,uintptr_t)>(image+0x316920);
            receipt.first_model=make_model(manager,receiver+0x190,false,scene);
            at<uintptr_t>(receiver,0x1b0)=receipt.first_model;
            if(!receipt.first_model)return ERROR_INVALID_DATA;
            receipt.last_model=make_model(manager,receiver+0x190,false,scene);
            at<uintptr_t>(receiver,0x1b8)=receipt.last_model;
            if(!receipt.last_model||receipt.last_model==receipt.first_model)return ERROR_INVALID_DATA;
            ghost=reinterpret_cast<unsigned char*>(reinterpret_cast<void*(*)(size_t)>(image+0xa02670)(0x480));
            receipt.ghost=uintptr_t(ghost);
            if(!ghost)return ERROR_OUTOFMEMORY;
            reinterpret_cast<void(*)(void*)>(image+0x43f840)(ghost);
            ghost_constructed=true;
            if(at<uintptr_t>(ghost,0)!=image+0x1743f10)return ERROR_INVALID_DATA;
            at<uintptr_t>(receiver,0x238)=uintptr_t(ghost);
            ghost[0x46e]=1;
            reinterpret_cast<void(*)(void*,int32_t,const void*,uintptr_t)>(image+0x438c70)(ghost,intent.type,receiver+0x190,plan.region);
            const auto ap=at<uintptr_t>(ghost,0);
            if(*reinterpret_cast<uintptr_t*>(ap+0x190)!=image+wall_private_native::path_set_shard||
               *reinterpret_cast<uintptr_t*>(ap+8)!=image+wall_private_native::path_d0)return ERROR_INVALID_DATA;
            reinterpret_cast<void(*)(void*,uintptr_t)>(*reinterpret_cast<uintptr_t*>(ap+0x190))(ghost,shard);
            ghost[0x81]=1;
            reinterpret_cast<void(*)(void*,int32_t)>(image+0x97a9d0)(ghost+0x30,0);
            reinterpret_cast<void(*)(void*,const Point*,int32_t,bool)>(image+0x444140)(ghost,plan.preview.data(),int32_t(plan.preview.size()),intent.preview_valid);
            if(at<uintptr_t>(ghost,0x178)!=plan.region||at<uintptr_t>(ghost,0x170)!=shard||
               at<int32_t>(ghost,0x2a0)!=int32_t(plan.preview.size())||
               std::memcmp(reinterpret_cast<const void*>(at<uintptr_t>(ghost,0x298)),plan.preview.data(),plan.preview.size()*sizeof(Point)))
                return ERROR_INVALID_DATA;
            const auto count=int32_t(intent.segments.size());
            auto capacity=at<int32_t>(receiver,0x2bc);
            auto splines=at<uintptr_t>(receiver,0x2b0);
            if(capacity!=4||at<int32_t>(receiver,0x2b8)!=0||!splines)return ERROR_INVALID_DATA;
            if(count>capacity){
                auto* replacement=reinterpret_cast<unsigned char*>(reinterpret_cast<void*(*)(size_t)>(image+0xa026c0)(size_t(count)*0x30));
                receipt.scratch_splines=uintptr_t(replacement);
                if(!replacement)return ERROR_OUTOFMEMORY;
                for(int32_t i=0;i<count;++i)reinterpret_cast<void(*)(void*)>(image+0x9fd9d0)(replacement+size_t(i)*0x30);
                reinterpret_cast<void(*)(void*)>(image+0xa01280)(reinterpret_cast<void*>(splines));
                splines=uintptr_t(replacement);capacity=count;
                at<uintptr_t>(receiver,0x2b0)=splines;at<int32_t>(receiver,0x2bc)=capacity;
                receipt.scratch_splines=0;
            }
            for(int32_t i=0;i<count;++i)std::memcpy(reinterpret_cast<void*>(splines+size_t(i)*0x30),intent.segments[size_t(i)].data(),0x30);
            at<int32_t>(receiver,0x2b8)=count;
            at<uintptr_t>(receiver,0x240)=plan.start_preview?uintptr_t(ghost):plan.start_split;
            at<uintptr_t>(receiver,0x248)=plan.start_node;
            at<uintptr_t>(receiver,0x278)=plan.end_node;
            at<uintptr_t>(receiver,0x280)=plan.end_preview?uintptr_t(ghost):plan.end_split;
            at<uintptr_t>(receiver,0x288)=plan.region;
            std::memcpy(receiver+0x298,intent.start.data(),sizeof(Point));
            std::memcpy(endpoint.data(),intent.end.data(),sizeof(Point));
            receiver[0x2c0]=1;receiver[0x2c2]=uint8_t(intent.preview_valid);
            return 0;
        }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    }
    uint32_t purchase(bool& allowed){
        __try{allowed=reinterpret_cast<bool(*)(int32_t,float,const Point*)>(image+0x43a1b0)(
            intent.type,std::bit_cast<float>(intent.quoted_length),&endpoint);return 0;}
        __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    }
    uint32_t finish(){
        __try{reinterpret_cast<void(*)(void*,const Point*)>(image+0x5ebd20)(receiver,&endpoint);return 0;}
        __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    }
    uint32_t cleanup(Receipt& receipt){
        __try{
            if(!receiver_constructed||!ghost_constructed)return ERROR_INVALID_DATA;
            const auto retained=at<uintptr_t>(receiver,0x238);
            if(receipt.finish_returned){if(retained)return ERROR_INVALID_DATA;}
            else {
                if(retained!=uintptr_t(ghost)||ghost[0x46e]!=1)return ERROR_INVALID_DATA;
                const auto ap=at<uintptr_t>(ghost,0);
                reinterpret_cast<void(*)(void*)>(*reinterpret_cast<uintptr_t*>(ap+8))(ghost);
                at<uintptr_t>(receiver,0x238)=0;
            }
            // These are private originals returned by MakeTurretInstance. Do
            // not call Deactivate, which also changes a borrowed split Path.
            for(size_t offset:{size_t(0x1b0),size_t(0x1b8)}){
                auto model=at<uintptr_t>(receiver,offset);
                if(!model)return ERROR_INVALID_DATA;
                reinterpret_cast<void(*)(uintptr_t)>(image+0x9a3660)(model);
                reinterpret_cast<void(*)(uintptr_t,size_t)>(image+0x1264b60)(model,0x78);
                at<uintptr_t>(receiver,offset)=0;
            }
            reinterpret_cast<void(*)(void*)>(image+0x5e9280)(receiver);
            reinterpret_cast<void(*)(void*,size_t)>(image+0x1264b60)(receiver,0x2d8);
            return 0;
        }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    }
};
}
uint32_t replay_wall(uintptr_t image,const action::WorldPathIntent& intent,
                     const mixed_path_native::Census& census,const std::map<uint64_t,std::string>& keys,
                     wall_private_native::Receipt& receipt){
    receipt={};wall_private::Plan plan;
    if(!image||!wall_private::prepare_plan(intent,census,keys,plan))return receipt.cause=ERROR_INVALID_PARAMETER;
    wall_private::Native native{image,intent,plan};
    return wall_private_native::execute(native,receipt);
}
}
