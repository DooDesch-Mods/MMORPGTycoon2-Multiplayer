#include "live_host_state/frame_fragments.h"
#include "live_host_state/day_rollover/economy/codec.h"
#include "mt2mp/core/hash.h"
#include <iostream>
#include "typed_action/action_session.h"
namespace live=mt2mp::game::live_host_state;
namespace economy=live::day::economy;
namespace staff=mt2mp::game::developer_staff;
namespace bug=mt2mp::game::bug_authority;
namespace quest=mt2mp::subscriber_quest_state;
namespace fragments=live::fragments;
using Json=nlohmann::json;
namespace {
#include "frame_fragment_fixture.inc"
size_t checks=0;
void need(bool ok,const std::string& cause){++checks;if(!ok)throw std::runtime_error(cause);}
live::Context context(const live::Frame& f){return {f.session,2,1,3,mt2mp::action_session::Role::Client,f.snapshot};}
std::vector<Json> json_packets(const fragments::Packets& packets){std::vector<Json> out;for(const auto& p:packets)out.push_back(Json::parse(p));return out;}
bool take(fragments::Assembly& a,const live::Frame& f,const Json& packet,std::vector<uint8_t>& out,std::string& e){return a.take(1,context(f),f.epoch,f.source_generation,packet,mt2mp::sha256,out,e);}
}
#include "frame_fragment_route_checks.inc"
int main(){try{
    std::string e;fragments::Packets packets;std::vector<uint8_t> bytes,complete;
    auto state=fixture();const auto f=state.day.frame;
    channel9_fixture::checks(f);
    need(economy::encode(state,bytes,e)&&fragments::encode(f,bytes,mt2mp::sha256,packets,e)&&packets.size()==1&&packets[0]==bytes,"small frame bytes changed: "+e);
    for(int32_t id=64;id<264;++id){auto subscriber=state.subscribers[0];subscriber.serialized_id=id;state.subscribers.push_back(subscriber);}
    need(economy::encode(state,bytes,e)&&bytes.size()>90895&&bytes.size()<live::max_logical_frame_payload,"grown full Economy fixture did not cross actual failure size: "+e);
    need(fragments::encode(f,bytes,mt2mp::sha256,packets,e)&&packets.size()>1,"grown frame not split: "+e);
    size_t wire_bytes=0;for(const auto& p:packets){wire_bytes+=p.size();need(p.size()<=live::max_frame_payload,"packet exceeded physical limit");}
    need(wire_bytes*3<7*1024*1024,"three-peer queue budget exceeded");
    const auto rows=json_packets(packets);fragments::Assembly receiver;
    for(size_t i=0;i<rows.size();++i){need(take(receiver,f,rows[i],complete,e),"ordered receive: "+e);if(i+1<rows.size())need(complete.empty()&&receiver.pending(),"incomplete bytes exposed");}
    need(complete==bytes&&mt2mp::sha256(complete)==mt2mp::sha256(bytes),"reassembly changed payload/SHA");
    need(receiver.completed().at("bytes")==bytes.size()&&receiver.completed().at("chunks")==rows.size()&&receiver.completed().at("revision")==f.revision&&receiver.completed().at("sha256")==mt2mp::sha256(bytes)&&receiver.completed().at("sha256_verified")==true&&receiver.completed().at("complete")==true,"completed frame diagnostic lacks exact verified evidence");
    economy::State decoded;need(economy::decode(complete.data(),complete.size(),decoded,e)&&decoded==state,"actual full Economy-Day-HostFrame validator changed: "+e);
    for(const auto& row:rows)need(take(receiver,f,row,complete,e)&&complete.empty(),"completed fragment replay emitted another frame");
    receiver.clear();need(take(receiver,f,rows[0],complete,e)&&take(receiver,f,rows[0],complete,e)&&complete.empty(),"identical inflight duplicate not idempotent");
    auto conflict=rows[0];auto changed=conflict["data"].get<std::string>();changed[0]=changed[0]=='0'?'1':'0';conflict["data"]=changed;
    need(!take(receiver,f,conflict,complete,e)&&e.find("duplicate")!=std::string::npos,"conflicting duplicate accepted");
    need(!take(receiver,f,rows[1],complete,e)&&complete.empty(),"failed assembly retried");
    receiver.clear();need(!take(receiver,f,rows[1],complete,e)&&e.find("first index")!=std::string::npos,"out-of-order initial chunk accepted");
    receiver.clear();need(take(receiver,f,rows[0],complete,e)&&!take(receiver,f,rows[2],complete,e)&&e.find("order")!=std::string::npos,"missing middle chunk accepted");
    receiver.clear();const auto now=std::chrono::steady_clock::now();need(receiver.take(1,context(f),f.epoch,f.source_generation,rows[0],mt2mp::sha256,complete,e,now),"timeout setup");
    need(receiver.timely(now+std::chrono::seconds(29),e)&&!receiver.timely(now+std::chrono::seconds(30),e)&&e.find("incomplete")!=std::string::npos,"incomplete stream never expired");
    receiver.clear();need(!receiver.pending()&&take(receiver,f,rows[0],complete,e),"explicit lifetime clear did not reset buffer");
    for(const char* field:{"session","epoch","source_generation","revision","snapshot_uuid","bytes","chunks","index","sha256","data"}){
        receiver.clear();auto bad=rows[0];
        if(std::string(field)=="snapshot_uuid")bad[field]="22222222-2222-4222-8222-222222222222";
        else if(std::string(field)=="sha256")bad[field]="wrong";
        else if(std::string(field)=="data")bad[field]="xx";
        else if(std::string(field)=="revision")bad[field]=uint64_t(0);
        else if(std::string(field)=="bytes")bad[field]=uint64_t(live::max_logical_frame_payload+1);
        else bad[field]=bad[field].get<uint64_t>()+1;
        need(!take(receiver,f,bad,complete,e)&&complete.empty(),std::string("bad fragment field accepted=")+field);
    }
    for(const char* field:{"index","sha256","session"}){
        auto duplicate=rows[0].dump();duplicate.pop_back();duplicate+=std::string(",\"")+field+"\":0}";bool rejected=false;
        try{fragments::packet_json(reinterpret_cast<const uint8_t*>(duplicate.data()),duplicate.size());}
        catch(const std::exception& x){rejected=std::string(x.what()).find("duplicate")!=std::string::npos;}
        need(rejected,std::string("duplicate outer field collapsed=")+field);
    }
    receiver.clear();need(!receiver.take(3,context(f),f.epoch,f.source_generation,rows[0],mt2mp::sha256,complete,e),"foreign physical sender accepted");
    receiver.clear();auto host=context(f);host.role=mt2mp::action_session::Role::Host;need(!receiver.take(1,host,f.epoch,f.source_generation,rows[0],mt2mp::sha256,complete,e),"Host received Client state stream");
    receiver.clear();auto corrupt=rows;corrupt.back()["data"]=std::string(corrupt.back()["data"].get<std::string>().size(),'0');
    for(size_t i=0;i<corrupt.size();++i){const bool ok=take(receiver,f,corrupt[i],complete,e);need(ok==(i+1<corrupt.size()),"SHA corruption acceptance differs");}
    need(e.find("SHA256")!=std::string::npos&&complete.empty(),"corruption reached Apply");
    auto invalid=Json::parse(bytes);invalid["schema"]=11;const auto invalid_text=invalid.dump();std::vector<uint8_t> invalid_bytes(invalid_text.begin(),invalid_text.end());
    need(fragments::encode(f,invalid_bytes,mt2mp::sha256,packets,e),"invalid-codec transport setup");receiver.clear();for(const auto& packet:json_packets(packets))need(take(receiver,f,packet,complete,e),"transport rejected structural codec test");
    need(!economy::decode(complete.data(),complete.size(),decoded,e)&&e.find("schema12")!=std::string::npos,"fragment path bypassed existing Economy validation");
    auto other=f;other.revision++;need(fragments::encode(other,bytes,mt2mp::sha256,packets,e),"identity mismatch setup");receiver.clear();const auto wrong=json_packets(packets);
    for(size_t i=0;i<wrong.size();++i)need(take(receiver,f,wrong[i],complete,e)==(i+1<wrong.size()),"envelope/payload identity mismatch accepted");
    need(e.find("payload identity")!=std::string::npos,"wrong identity cause lost");
    // Exact measured failure length, with real SHA256 and byte-for-byte output.
    auto exact=Json::parse(bytes);exact["subscribers"]=Json::array();exact["padding"]="";const auto prefix=exact.dump().size();need(prefix<90895,"exact fixture prefix too large");exact["padding"]=std::string(90895-prefix,'x');const auto exact_text=exact.dump();std::vector<uint8_t> exact_bytes(exact_text.begin(),exact_text.end());
    need(exact_bytes.size()==90895&&fragments::encode(f,exact_bytes,mt2mp::sha256,packets,e),"90895 byte setup");receiver.clear();for(const auto& packet:json_packets(packets))need(take(receiver,f,packet,complete,e),"90895 receive");need(complete==exact_bytes,"90895 byte payload differs");
    auto maximum=exact;maximum["padding"]="";maximum["padding"]=std::string(live::max_logical_frame_payload-maximum.dump().size(),'x');const auto maximum_text=maximum.dump();std::vector<uint8_t> max_bytes(maximum_text.begin(),maximum_text.end());
    need(fragments::encode(f,max_bytes,mt2mp::sha256,packets,e),"maximum supported logical length rejected");wire_bytes=0;for(const auto& p:packets)wire_bytes+=p.size();need(3*wire_bytes<7*1024*1024,"maximum aggregate queue budget invalid");
    max_bytes.push_back(' ');need(!fragments::encode(f,max_bytes,mt2mp::sha256,packets,e)&&packets.empty()&&e.find("1048576")!=std::string::npos,"oversize produced partial packets");
    std::cout<<"PASS "<<checks<<" actual frame-fragment/codec checks\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
