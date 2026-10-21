#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mt2mp::game::move_widget_refresh {
using Matrix=std::array<float,16>;
struct Widget {
    uintptr_t object=0,model=0;
    Matrix context{},model_matrix{};
    std::array<float,3> local{};
    std::array<float,4> rotation{},color{};
    uint8_t visible=0,dirty=0,model_visible=0;
};
struct Snapshot {
    uintptr_t target=0,data=0;
    Matrix building_matrix{};
    uint8_t matrix_dirty=0;
    std::vector<Widget> widgets;
};
struct Receipt {
    bool required=false,attempted=false,returned=false,readback=false;
    bool selection_preserved=false,world_pose_preserved=false;
    size_t widget_count=0;
    uint32_t exception=0;
    std::string error;
};
inline bool fail(std::string& error,const char* reason){error=reason;return false;}
template<size_t N> bool finite(const std::array<float,N>& values){
    for(float value:values)if(!std::isfinite(value))return false;
    return true;
}
template<class Read>
bool snapshot(uintptr_t image,uintptr_t target,Snapshot& out,Read&& read,std::string& error){
    out={};out.target=target;int32_t count=0;
    if(!target)return fail(error,"move widget current target is missing");
    if(!read(target+0x370,&out.data,sizeof out.data,error)||
       !read(target+0x378,&count,sizeof count,error)||
       !read(target+0x128,&out.matrix_dirty,sizeof out.matrix_dirty,error)||
       !read(target+0x12c,out.building_matrix.data(),sizeof out.building_matrix,error))return false;
    if(count<0||count>65536||(count&&!out.data)||out.matrix_dirty>1||!finite(out.building_matrix))
        return fail(error,"move widget current Building array or matrix is invalid");
    out.widgets.reserve(size_t(count));
    for(int32_t index=0;index<count;++index){
        Widget widget;uintptr_t addresspoint=0;
        if(!read(out.data+size_t(index)*sizeof(uintptr_t),&widget.object,sizeof widget.object,error))return false;
        if(!widget.object)return fail(error,"move widget array contains a missing owner");
        for(const auto& previous:out.widgets)if(previous.object==widget.object)
            return fail(error,"move widget array contains a duplicate owner");
        if(!read(widget.object,&addresspoint,sizeof addresspoint,error)||
           !read(widget.object+8,&widget.model,sizeof widget.model,error))return false;
        if(addresspoint!=image+0x1712b20||!widget.model)
            return fail(error,"move widget current class or model owner differs");
        if(!read(widget.object+0x10,widget.context.data(),sizeof widget.context,error)||
           !read(widget.object+0x50,widget.local.data(),sizeof widget.local,error)||
           !read(widget.object+0x5c,widget.rotation.data(),sizeof widget.rotation,error)||
           !read(widget.object+0x6c,widget.color.data(),sizeof widget.color,error)||
           !read(widget.object+0x80,&widget.visible,sizeof widget.visible,error)||
           !read(widget.object+0x81,&widget.dirty,sizeof widget.dirty,error)||
           !read(widget.model,widget.model_matrix.data(),sizeof widget.model_matrix,error)||
           !read(widget.model+0x70,&widget.model_visible,sizeof widget.model_visible,error))return false;
        if(!finite(widget.context)||!finite(widget.local)||!finite(widget.rotation)||
           !finite(widget.color)||!finite(widget.model_matrix)||widget.visible>1||widget.dirty>1||widget.model_visible>1)
            return fail(error,"move widget current presentation contains invalid values");
        out.widgets.push_back(widget);
    }
    return true;
}
inline float translation(const Matrix& context,const std::array<float,3>& local,size_t axis){
    // Match the original Matrix.ApplyTo(Matrix) translation-column operation
    // order. Widget-local rotation and screen size do not change this column.
    volatile float x=local[0]*context[axis],y=local[1]*context[4+axis];
    volatile float z=local[2]*context[8+axis],w=1.0f*context[12+axis];
    volatile float xy=y+x,zw=z+w;
    return xy+zw;
}
inline bool confirm(const Snapshot& before,const Snapshot& after,std::string& error){
    if(before.target!=after.target||before.data!=after.data||before.widgets.size()!=after.widgets.size())
        return fail(error,"move widget array ownership changed during refresh");
    if(after.matrix_dirty)return fail(error,"move widget current Building matrix remains dirty");
    for(size_t index=0;index<before.widgets.size();++index){
        const auto& old=before.widgets[index];const auto& current=after.widgets[index];
        if(old.object!=current.object||old.model!=current.model||old.local!=current.local||
           old.rotation!=current.rotation||old.color!=current.color||old.visible!=current.visible)
            return fail(error,"move widget owner or local entrance presentation changed during refresh");
        if(current.context!=after.building_matrix||current.dirty||current.model_visible!=current.visible)
            return fail(error,"move widget current context or native model visibility is stale");
        for(size_t axis=0;axis<3;++axis)
            if(current.model_matrix[12+axis]!=translation(current.context,current.local,axis))
                return fail(error,"move widget model translation differs from current Building entrance");
    }
    return true;
}
template<class Read,class Call>
bool refresh(uintptr_t image,uintptr_t target,Receipt& receipt,Read&& read,Call&& call,std::string& error){
    auto reject=[&](){receipt.error=error;return false;};
    if(receipt.attempted){fail(error,"move widget refresh already attempted; no replay");return reject();}
    Snapshot before;
    if(!snapshot(image,target,before,read,error))return reject();
    receipt.widget_count=before.widgets.size();receipt.required=!before.widgets.empty();
    if(!receipt.required)return true;
    if(!call(receipt,error))return reject();
    Snapshot after;
    if(!snapshot(image,target,after,read,error)||!confirm(before,after,error))return reject();
    receipt.readback=true;return true;
}
}
