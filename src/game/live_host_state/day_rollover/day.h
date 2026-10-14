#pragma once
#include "live_host_state/state.h"
#include <map>
namespace mt2mp::game::live_host_state::day {
constexpr size_t history_limit=10;
struct Record {
    std::array<int32_t,5> values{};
    std::array<std::array<int32_t,2>,20> lines{};
    bool operator==(const Record&)const=default;
};
struct Row {uint64_t sequence=0;Record record;bool operator==(const Row&)const=default;};
struct Target {Frame frame;std::vector<Row> history;bool operator==(const Target&)const=default;};
struct NativeHistory {Pin pin;std::vector<uintptr_t> owners;std::vector<Record> records;};
struct Journal {
    uint64_t current_sequence=0;uintptr_t current_owner=0;
    std::map<uintptr_t,uint64_t> live;
    bool unknown=false;std::string cause;
    bool bootstrap(const NativeHistory&,std::string&);
    bool observe_make(const NativeHistory& before,const NativeHistory& after,std::string&);
    bool capture(const NativeHistory&,std::vector<Row>&,std::string&);
};
struct Receipt {
    bool attempted=false,confirmed=false,unknown=false;
    uint32_t closed_records=0,created_records=0,appended_records=0,pruned_records=0;
    std::string cause;
};
struct Port {
    virtual ~Port()=default;
    virtual bool rebind_snapshot(const Context&,const Binding&,std::string& e){e="actual Day rebind provider unavailable";return false;}
    virtual bool read(NativeHistory&,std::string&)=0;
    virtual bool write_closed(const NativeHistory&,uintptr_t owner,const Record& expected,const Record& target,std::string&)=0;
    virtual bool create(const Record&,uintptr_t& private_owner,std::string&)=0;
    virtual bool append(const NativeHistory&,uintptr_t private_owner,const Record&,std::string&)=0;
    virtual bool commit_current(const NativeHistory&,uintptr_t new_current,const Frame&,std::string&)=0;
    virtual bool prune(const NativeHistory&,size_t keep,std::string&)=0;
};
bool valid(const Target&,std::string&);
bool encode(const Target&,std::vector<uint8_t>&,std::string&);
bool decode(const uint8_t*,size_t,Target&,std::string&);
class Apply {
    bool locked_=false;
    std::vector<Row> known_;
    std::vector<uintptr_t> owners_;
    Receipt receipt_;
public:
    bool bind(const Target& authoritative_baseline,const NativeHistory& actual,std::string&);
    bool matches(const NativeHistory&,std::string&)const;
    bool adopt_current(const NativeHistory&,std::string&);
    bool synchronize(const Target&,const NativeHistory&,std::string&);
    const Receipt& run(Port&,const Target&);
    const Receipt& receipt()const{return receipt_;}
    bool locked()const{return locked_;}
    const std::vector<Row>& known()const{return known_;}
};
Record record(const Values&);
}
