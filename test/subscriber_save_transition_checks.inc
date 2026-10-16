namespace subscriber_save_transition {
using Target=live::day::Target;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
Owners before_logout(){
 auto value=fixture(baseline()).value;value.history.pin.context.role=mt2mp::action_session::Role::Host;
 value.history.pin.pause=0;value.next_id=105;
 auto& row=value.state.subscribers[0];row.serialized_id=79;row.play_days=86;
 row.pay={2,0,149,0,40};row.activity={5,3,1146195381};
 row.needs_bits={1120403456,1120050856,1114620505,3267887104,1101519774,3238337113,1120403456};
 Subscriber added;added.serialized_id=104;value.state.subscribers.push_back(added);
 value.subscribers.push_back(2);value.subscriber_slots.push_back(1);value.needs_owners.resize(2);return value;
}
Owners logged_out(const Owners& before){
 auto value=before;auto& row=value.state.subscribers[0];row.motion.reset();row.play_days=22;
 row.activity.logged_in_time_bits=1146280430;row.needs_bits[5]=3228854628;return value;
}
struct CapturePort:Port {
 Owners value,next;unsigned captures=0,reads=0,restores=0;int32_t captured_id=-1;
 bool bind_actual_receipt(const live::day::Target& target,std::string& e)override{if(target!=value.state.day)return fail(e,"fixture Day receipt differs");e.clear();return true;}
 bool read(Owners& out,std::string& e)override{++reads;out=value;e.clear();return true;}
 bool store_subscriber(const Owners&,size_t,size_t,int32_t,int32_t,std::string& e)override{return fail(e,"unexpected Subscriber store");}
 bool store_loan(const Owners&,size_t,size_t,int32_t,int32_t,std::string& e)override{return fail(e,"unexpected Loan store");}
 bool capture_subscriber(int32_t id,nlohmann::json& graph,std::string& e)override{
  ++captures;captured_id=id;const auto before=value;value=next;graph={{"captured_id",id}};
  return restore_capture_pause(before,value,[&](const live::Pin& pin,std::string&){++restores;value.history.pin.pause=pin.pause;return true;},
   [&](Owners& out,std::string& error){return read(out,error);},e);
 }
};
// The native graph cache is a leaf double. Capture orchestration is extracted
// verbatim; final State validation and wire encode/decode use the real codecs.
struct Cache {
 bool failed=false;
 bool locked()const{return failed;}
 template<class Capture>bool host(const std::vector<int32_t>& ids,int32_t,uint64_t,Capture capture,
  mt2mp::subscriber_lifecycle::additions::State& out,std::string& e,
  const std::vector<mt2mp::subscriber_lifecycle::additions::Retirement>&){
  nlohmann::json graph;out={};failed=!capture(ids.back(),graph,e);return !failed;
 }
};
struct Coupling {
 Port& port_;bool locked_=false;std::string capture_cause_;Cache additions_;
 explicit Coupling(Port& p):port_(p){}
 bool locked()const{return locked_||additions_.locked();}
 bool capture(const live::day::Target&,State&,std::string&);
};
#include "subscriber_save_capture.inc"
void run(){
 const auto before=before_logout();const auto after=logged_out(before);const auto fresh=after;
 std::string error;
 need(!capture_readback(before,after,error),"strict comparison admitted logout schedule change");
 const bool accepted=capture_readback(before,after,error,true);
 need(accepted,"original logout86->22 rejected: "+error);
 need(after.state==fresh.state&&after.state.subscribers[0].play_days==22,"fresh logout state overwritten");
 for(unsigned day=1;day<=7;++day){
  auto current=after;current.state.subscribers[0].play_days=before.state.subscribers[0].play_days^(uint32_t{1}<<day);
  const bool okay=capture_readback(before,current,error,true);need(okay,"native day1..7 single-bit logout rejected: "+error);
 }
 auto reject=[&](Owners old,Owners current,const std::string& field){
  const bool okay=capture_readback(old,current,error,true);
  need(!okay&&error.find(field)!=std::string::npos,"logout refresh lost guard "+field+": "+error);
 };
 reject(after,before,"play_days");
 auto current=after;current.state.subscribers[0].motion=before.state.subscribers[0].motion;reject(before,current,"play_days");
 auto old=before;old.state.subscribers[0].motion.reset();reject(old,after,"play_days");
 current=after;current.state.subscribers[0].play_days=before.state.subscribers[0].play_days^6u;reject(before,current,"play_days");
 for(const uint32_t bit:{1u,256u,0x80000000u}){current=after;current.state.subscribers[0].play_days=before.state.subscribers[0].play_days^bit;reject(before,current,"play_days");}
 current=after;current.state.subscribers[0].pay[0]++;reject(before,current,".pay");
 current=after;current.history.pin.world++;reject(before,current,"native_owners_changed");
 current=after;current.state.subscribers[0].serialized_id++;reject(before,current,"id_after");
 current=after;current.next_id++;reject(before,current,"next_id_after");
 current=after;current.state.day.frame.values.cash++;reject(before,current,"day_changed");
 current=after;current.state.subscribers[0].needs_bits[5]=0x7f800000;reject(before,current,"Needs invalid");
 current=after;current.state.subscribers[0].activity.logged_in_time_bits=0x7f800000;reject(before,current,"activity invalid");
 CapturePort port;port.value=before;port.next=after;Coupling coupling(port);State out;
 const bool captured=coupling.capture(before.state.day,out,error);
 need(captured&&port.captures==1&&port.captured_id==104&&port.reads==2&&port.restores==0,"actual Coupling capture failed: "+error);
 need(out==after.state&&out.subscribers[0].play_days==22&&!out.subscribers[0].motion,"actual Coupling published stale logout state");
 std::vector<uint8_t> wire;State decoded;const bool encoded=encode(out,wire,error);
 need(encoded,"actual outgoing economy encode failed: "+error);
 const bool received=decode(wire.data(),wire.size(),decoded,error);
 need(received&&decoded==after.state,"actual economy wire lost refreshed logout state: "+error);
 CapturePort paused;paused.value=before;paused.value.history.pin.pause=1;paused.next=after;Coupling paused_capture(paused);
 const bool restored=paused_capture.capture(before.state.day,out,error);
 need(restored&&paused.captures==1&&paused.restores==1&&out==after.state,"actual pause restoration lost fresh logout state: "+error);
 CapturePort bad;bad.value=before;bad.next=after;bad.next.state.subscribers[0].pay[0]++;Coupling rejected(bad);
 need(!rejected.capture(before.state.day,out,error)&&error.find(".pay")!=std::string::npos,"actual capture ignored unrelated scalar change");
 const auto cause=error;const auto reads=bad.reads;
 need(!rejected.capture(before.state.day,out,error)&&error==cause&&bad.captures==1&&bad.reads==reads,"failed capture cause changed or native work replayed");
}
}
