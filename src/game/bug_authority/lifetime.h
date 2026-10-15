#pragma once
#include "state.h"
#include <algorithm>
#include <map>
#include <set>
namespace mt2mp::game::bug_authority {
inline bool fail(std::string& error,const std::string& cause){error=cause;return false;}
inline bool same_identity(const Row& a,const Row& b){return a.parent_kind==b.parent_kind&&a.parent_key==b.parent_key&&a.birth==b.birth;}
struct Local {
    Row row;
    uintptr_t bug=0,parent=0,collection=0;
    bool retired=false;
};
// Pointer values are used only after an original registration or constructor
// witness. Original Tracker removal ends that occurrence before allocator reuse.
class Lifetimes {
    std::vector<Local> rows_;
public:
    const std::vector<Local>& rows()const{return rows_;}
    void clear(){rows_.clear();}
    bool born(Local value,std::string& error){
        if(!value.bug||!value.collection)return fail(error,"Bug birth receiver/collection absent");
        for(const auto& row:rows_){
            if(same_identity(row.row,value.row))return fail(error,"Bug birth identity already consumed");
            if(!row.retired&&row.bug==value.bug)return fail(error,"Bug receiver still owns a prior live occurrence");
        }
        rows_.push_back(std::move(value));return true;
    }
    bool remove(uintptr_t bug,std::string& error){
        Local* found=nullptr;
        for(auto& row:rows_)if(!row.retired&&row.bug==bug){if(found)return fail(error,"Bug removal has duplicate live occurrence");found=&row;}
        if(found)found->retired=true;
        return true;
    }
    Local* pointer(uintptr_t value){for(auto& row:rows_)if(!row.retired&&row.bug==value)return &row;return nullptr;}
    Local* identity(const Row& value){for(auto& row:rows_)if(!row.retired&&same_identity(row.row,value))return &row;return nullptr;}
    bool consumed(const Row& value)const{for(const auto& row:rows_)if(same_identity(row.row,value))return true;return false;}
    bool bind_parent(uintptr_t bug,ParentKind kind,const std::string& key,uintptr_t parent,std::string& error){
        auto* row=pointer(bug);if(!row)return fail(error,"Bug parent binding lacks original birth");
        if(row->parent&&(row->parent!=parent||row->row.parent_kind!=kind||row->row.parent_key!=key))
            return fail(error,"Bug original occurrence changed parent identity");
        row->parent=parent;row->row.parent_kind=kind;row->row.parent_key=key;return true;
    }
};
// This list is populated by original RTTI SaveStream calls or original
// ObjectContext registrations inside one exact collection-property scope.
struct Children {
    uintptr_t collection=0,stream=0,context=0;
    int32_t declared=-1;
    std::vector<uintptr_t> witnessed;
    bool child(uintptr_t actual_stream,uintptr_t actual_context,uintptr_t object,std::string& error){
        if(actual_stream!=stream||actual_context!=context||!object||declared<0||witnessed.size()>=static_cast<size_t>(declared))
            return fail(error,"Bug original child stream/context/count differs");
        if(std::find(witnessed.begin(),witnessed.end(),object)!=witnessed.end())return fail(error,"Bug original child registration repeated");
        witnessed.push_back(object);return true;
    }
    bool finish(const std::vector<uintptr_t>& current,std::string& error)const{
        if(declared<0||witnessed.size()!=static_cast<size_t>(declared)||witnessed!=current)
            return fail(error,"Bug original child witnesses differ from completed collection");
        return true;
    }
};
}
