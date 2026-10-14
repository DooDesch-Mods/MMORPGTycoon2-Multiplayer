#include "undo_witness.h"
#include <algorithm>
#include <limits>

namespace mt2mp::build_actions::private_native {
namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
template<class T> bool at(Memory& m,uintptr_t p,size_t o,T& v,std::string& e){
    if(p>std::numeric_limits<uintptr_t>::max()-o)return fail(e,"undo_address_overflow");
    if(m.copy(p+o,&v,sizeof(v),e))return true;
    if(e.empty())e="undo_memory_read_failed";
    return false;
}
bool list(Memory& m,uintptr_t image,uintptr_t p,uintptr_t expected,std::vector<uintptr_t>& out,std::string& e){
    uintptr_t ap=0,data=0;int32_t count=0,capacity=0;
    if(!at(m,p,0,ap,e)||!at(m,p,8,data,e)||!at(m,p,16,count,e)||!at(m,p,20,capacity,e))return false;
    if(ap!=image+expected)return fail(e,"undo_array_addresspoint");
    if(count<0||capacity<count)return fail(e,"undo_array_count_capacity");
    out.clear();if(!count)return true;
    const size_t bytes=static_cast<size_t>(count)*sizeof(uintptr_t);
    if(!data||data>std::numeric_limits<uintptr_t>::max()-bytes)return fail(e,"undo_array_data_range");
    out.resize(static_cast<size_t>(count));
    if(!m.copy(data,out.data(),bytes,e)){if(e.empty())e="undo_array_read_failed";return false;}
    if(std::find(out.begin(),out.end(),uintptr_t{0})!=out.end())return fail(e,"undo_array_null_member");
    return true;
}
bool count(Memory& m,uintptr_t image,uintptr_t graph,size_t offset,uintptr_t expected,int32_t& n,std::string& e){
    uintptr_t ap=0,data=0;int32_t cap=0;
    if(!at(m,graph,offset,ap,e)||!at(m,graph,offset+8,data,e)||!at(m,graph,offset+16,n,e)||!at(m,graph,offset+20,cap,e))return false;
    if(ap!=image+expected)return fail(e,"undo_graph_array_addresspoint");
    if(n<0||cap<n||(!data&&n))return fail(e,"undo_graph_count_capacity");
    return true;
}
}
bool read_undo(Memory& m,uintptr_t image,UndoWitness& out,std::string& e){
    out={};e.clear();
    try {
        if(!image||image>std::numeric_limits<uintptr_t>::max()-0x1a3aa80)return fail(e,"undo_image_range");
        auto once=[&](UndoWitness& w){
            uintptr_t ap=0;
            if(!at(m,image,0x13df380,w.receiver,e)||!at(m,image,0x1a3aa80,w.active_graph,e))return false;
            if(!w.receiver)return fail(e,"undo_manager_missing");
            if(!at(m,w.receiver,0,ap,e))return false;
            if(ap!=image+0x16fe070)return fail(e,"undo_manager_addresspoint");
            if(w.receiver>std::numeric_limits<uintptr_t>::max()-0x38)return fail(e,"undo_manager_range");
            return list(m,image,w.receiver+8,0x16e1320,w.undo,e)&&list(m,image,w.receiver+0x20,0x16e1320,w.redo,e);
        };
        UndoWitness a,b;if(!once(a)||!once(b))return false;
        if(a!=b)return fail(e,"undo_changed_during_read");
        out=std::move(b);return true;
    }catch(const std::exception& x){e=std::string("undo_read_exception: ")+x.what();return false;}
    catch(...){return fail(e,"undo_read_unknown_exception");}
}
bool read_graph(Memory& m,uintptr_t image,uintptr_t graph,GraphWitness& out,std::string& e){
    out={};e.clear();
    try {
        if(!image||!graph||image>std::numeric_limits<uintptr_t>::max()-0x1748c50)return fail(e,"undo_graph_address_range");
        auto once=[&](GraphWitness& w){
            w.graph=graph;uint8_t locked=0;
            if(!count(m,image,graph,0,0x16e32d0,w.nodes,e)||!count(m,image,graph,0x18,0x16e2dc0,w.operations,e)||
               !count(m,image,graph,0x48,0x1748c50,w.pending_delete,e)||!at(m,graph,0x10c,locked,e))return false;
            if(locked>1)return fail(e,"undo_graph_lock_not_bool");
            w.locked=locked!=0;return true;
        };
        GraphWitness a,b;if(!once(a)||!once(b))return false;
        if(a!=b)return fail(e,"undo_graph_changed_during_read");
        out=b;return true;
    }catch(const std::exception& x){e=std::string("undo_graph_exception: ")+x.what();return false;}
    catch(...){return fail(e,"undo_graph_unknown_exception");}
}
bool validate_undo_commit(const UndoWitness& a,const UndoWitness& b,const GraphWitness& g,std::string& e){
    if(!a.receiver||a.receiver!=b.receiver)return fail(e,"undo_commit_receiver_changed");
    if(a.active_graph||b.active_graph)return fail(e,"undo_commit_graph_still_active");
    if(!g.graph||!g.locked||g.pending_delete)return fail(e,"undo_commit_not_drained_locked");
    if(!b.redo.empty())return fail(e,"undo_commit_redo_not_cleared");
    if(std::find(a.undo.begin(),a.undo.end(),g.graph)!=a.undo.end())return fail(e,"undo_commit_graph_not_new");
    auto expected=a.undo;expected.push_back(g.graph);
    // AddUndo trims its own history to twenty entries after appending.
    if(expected.size()>20)expected.erase(expected.begin(),expected.end()-20);
    if(expected!=b.undo)return fail(e,"undo_commit_history_mismatch");
    return true;
}
}
