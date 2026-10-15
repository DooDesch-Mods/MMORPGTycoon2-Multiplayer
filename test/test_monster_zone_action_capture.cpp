#include "typed_action/action_session.h"
#include <iostream>
#include <optional>
#include <stdexcept>
namespace a=mt2mp::action_session;using json=nlohmann::json;
struct Memory{};struct Pin{int region=1;};namespace d{struct Sample{int cash=0;};}
struct Capture{uintptr_t brush=100,graph=200;Pin pin;a::ZoneIntent intent;std::string cause,ui_token="token";bool cleanup_attempted=false;};
struct Runtime{bool capture_fault=false;unsigned thread=7,captures=0;std::optional<Capture> capture,pending;void(*finished)(void*,const void*)=nullptr;std::map<std::string,json> ui_jobs;std::string cause;}state;
bool connected=true;unsigned thread_id=7,original_calls=0;int current_cash=20;std::string fault;
namespace net::session_wire{bool connected(){return ::connected;}}
unsigned GetCurrentThreadId(){return thread_id;}
void reject(const std::string& e){state.cause=e;}
template<class T>bool field(Memory&,uintptr_t,size_t offset,T& out,std::string& e){if(fault=="read"){e="native graph unavailable";return false;}out=T(offset==0x740?200:0);if(fault=="graph"&&offset==0x740)out=T(201);if(fault=="operations"&&offset==0x28)out=T(1);return true;}
namespace d{bool sample(Memory&,int,Sample& s,std::string& e){if(fault=="sample"){e="native full state unavailable";return false;}s.cash=current_cash;return true;}}
uint32_t binary(void(*)(void*,const void*),void*,const void*){++original_calls;if(fault=="seh")return 5;if(fault=="effect")++current_cash;return 0;}
bool native_ok(uint32_t c,const char*,std::string& e){if(c)e="original Finish exception=5";return c==0;}
bool capture_unchanged(Memory&,const Capture&,const d::Sample& before,std::string& e){if(before.cash!=current_cash){e="original callback changed state";return false;}return true;}
#include "zone_finish_body.inc"
void need(bool v,const std::string& e){if(!v)throw std::runtime_error(e);}
void reset(){state={};connected=true;thread_id=7;original_calls=0;current_cash=20;fault.clear();Capture c;c.intent.region=67;c.intent.character_type=0;c.intent.level=1;
 c.intent.stamps.push_back({{1,0,2},{{{0,0},{1,0},{1,1}}}});state.capture=c;state.ui_jobs["token"]={{"captured",false}};}
int main(){try{unsigned count=0;reset();finished_entry(reinterpret_cast<void*>(100),nullptr);
 need(original_calls==1&&!state.capture&&state.pending&&state.captures==1&&state.ui_jobs["token"]["captured"]==true,"valid captured Finish failed");++count;
 finished_entry(reinterpret_cast<void*>(100),nullptr);need(original_calls==1,"Finish replayed");++count;
 for(const auto* f:{"read","graph","operations","sample","thread","prior_fault"}){reset();fault=f;if(fault=="thread")thread_id=8;if(fault=="prior_fault")state.capture_fault=true;
  finished_entry(reinterpret_cast<void*>(100),nullptr);need(!original_calls&&state.capture&&!state.pending&&!state.cause.empty(),"invalid owner reached original cleanup");++count;}
 for(const auto* f:{"seh","effect"}){reset();fault=f;finished_entry(reinterpret_cast<void*>(100),nullptr);need(original_calls==1&&state.capture_fault&&state.capture&&!state.pending&&!state.cause.empty(),"partial cleanup hidden");
  finished_entry(reinterpret_cast<void*>(100),nullptr);need(original_calls==1,"Unknown cleanup replayed");++count;}
 reset();state.capture->cause="invalid original stamp";finished_entry(reinterpret_cast<void*>(100),nullptr);
 need(original_calls==1&&!state.capture&&!state.pending&&state.cause=="invalid original stamp","invalid capture submitted or cleanup skipped");++count;
 reset();current_cash=30;finished_entry(reinterpret_cast<void*>(100),nullptr);need(original_calls==1&&state.pending&&current_cash==30,"legitimate between-frame simulation rejected");++count;
 std::cout<<count<<" actual Finish capture cases PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
