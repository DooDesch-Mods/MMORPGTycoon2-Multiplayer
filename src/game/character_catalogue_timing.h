#pragma once
#include <array>
#include <chrono>
#include <cstdint>

namespace mt2mp::game::character_catalogue_timing {
enum class Part {Code,Prepare,Census,Quests,References,ExternalOwners,ExternalProps,PropRead,PropCode,PropLookup,PropText,Lifetime,NativeCapture,GraphRead,CanonicalRecords,FinalHash,Count};
using Clock=std::chrono::steady_clock;
inline uint64_t elapsed(Clock::time_point started){return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-started).count());}
struct Totals {
    std::array<uint64_t,size_t(Part::Count)> ns{};
    uint64_t preparations=0,records=0,graph_nodes=0,private_links=0;
    double ms(Part part)const{return double(ns[size_t(part)])/1000000.0;}
};
inline thread_local Totals* active=nullptr;
struct Binding {
    Totals* previous=active;
    explicit Binding(Totals& value){active=&value;}
    ~Binding(){active=previous;}
    Binding(const Binding&)=delete;
    Binding& operator=(const Binding&)=delete;
};
struct Scope {
    Totals* totals=active;Part part;Clock::time_point started;
    explicit Scope(Part value):part(value),started(totals?Clock::now():Clock::time_point{}){}
    ~Scope(){if(totals)totals->ns[size_t(part)]+=elapsed(started);}
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
};
}
