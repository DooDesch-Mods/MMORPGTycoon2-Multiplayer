#pragma once
#include "lifetime.h"
namespace mt2mp::game::bug_authority {
template<class Birth>
bool random_result(uintptr_t collection,uintptr_t result,const std::vector<uintptr_t>& before,const std::vector<uintptr_t>& after,Lifetimes& lifetimes,Birth&& born,std::string& error){
    if(!result)return before==after||fail(error,"Original random Bug returned null but changed collection membership");
    const auto prior=std::count(before.begin(),before.end(),result),current=std::count(after.begin(),after.end(),result);
    if(current!=1)return fail(error,"Original random Bug result is not a unique current collection member");
    if(prior){
        if(prior!=1||before!=after)return fail(error,"Original random Bug strengthened result changed collection membership");
        const auto* row=lifetimes.pointer(result);
        return (row&&row->collection==collection&&!row->retired)||fail(error,"Original random Bug existing result lacks exact live collection occurrence");
    }
    auto expected=before;expected.push_back(result);
    if(after!=expected||lifetimes.pointer(result))return fail(error,"Original random Bug new result is not one fresh appended occurrence");
    return born(error);
}
}
