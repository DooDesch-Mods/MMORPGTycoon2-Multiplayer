#pragma once
#include "../typed_action/tutorial_action.h"
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>
namespace mt2mp::game::tutorial_replication::ordered_message {
using Intent=action_session::TutorialIntent;
struct Current {
    uintptr_t step=0;
    bool sequence_complete=false,step_complete=false,message=false;
    Intent intent;
};
// The native Instance Update remains the caller and owns its history update.
// This port replaces only Sequence Update's Update-once/Advance loop.
class Port {
public:
    virtual ~Port()=default;
    virtual bool current(Current&,std::string&)=0;
    virtual bool update(uintptr_t,float,std::string&)=0;
    virtual bool advance(uintptr_t,bool clear_undo,std::string&)=0;
};
struct Entry {Intent intent;bool observed=false,submitted=false,granted=false,released=false;};
class Gate {
    std::map<std::pair<std::string,int32_t>,Entry> entries_;
public:
    void reset(){entries_.clear();}
    Entry* find(const Intent& key){
        auto i=entries_.find({key.asset,key.index});
        return i==entries_.end()?nullptr:&i->second;
    }
    const auto& entries()const{return entries_;}
    bool touch(const Intent& key,Entry*& entry,std::string& error){
        if(!action_session::tutorial_valid(key)){error="Tutorial current asset/index/type/ID invalid";return false;}
        auto i=entries_.find({key.asset,key.index});
        if(i!=entries_.end()){
            if(!action_session::tutorial_same_step(i->second.intent,key)){error="Tutorial current step identity changed";return false;}
            entry=&i->second;return true;
        }
        if(entries_.size()>=124){error="Tutorial message identity count exceeds both native assets";return false;}
        entry=&entries_.emplace(std::make_pair(key.asset,key.index),Entry{key}).first->second;return true;
    }
    bool observed(const Intent& key,bool clear,std::string& error){
        Entry* entry=nullptr;if(!touch(key,entry,error))return false;
        if(entry->released){error="Tutorial released message updated again";return false;}
        entry->observed=true;entry->intent.clear_undo|=clear;return true;
    }
    // The ordered command grants a release; native effects run in actual Go.
    bool grant(const Intent& key,std::string& error){
        Entry* entry=nullptr;if(!touch(key,entry,error))return false;
        if(!entry->granted){entry->intent=key;entry->granted=true;}
        return true;
    }
    bool frame(Port& port,float delta,std::string& error){
        Current current;if(!port.current(current,error))return false;
        if(current.sequence_complete||!current.step)return true;
        Entry* entry=nullptr;
        if(current.message&&!touch(current.intent,entry,error))return false;
        // Both peers skip the same native Update after the consumed grant.
        // Before that grant both continue Update, even if one local UI is done.
        if(!entry||!entry->granted){
            if(!port.update(current.step,delta,error)||!port.current(current,error))return false;
        }
        for(size_t advanced=0;advanced<=124;++advanced){
            if(current.sequence_complete||!current.step)return true;
            entry=nullptr;
            if(current.message){
                if(!touch(current.intent,entry,error))return false;
                if(!entry->granted){
                    if(current.step_complete&&!observed(current.intent,false,error))return false;
                    return true;
                }
                if(entry->released){error="Tutorial granted step was already released";return false;}
            }else if(!current.step_complete)return true;
            const auto previous=current.step;
            // A failed/throwing original Advance stays attempted; no retry.
            if(entry)entry->released=true;
            if(!port.advance(previous,entry&&entry->intent.clear_undo,error)||!port.current(current,error))return false;
            if(!current.sequence_complete&&current.step==previous){error="Tutorial original Advance did not change current step";return false;}
        }
        error="Tutorial native inline transition count exceeds both assets";return false;
    }
};
}
