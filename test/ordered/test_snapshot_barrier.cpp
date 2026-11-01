#include "snapshot_barrier.h"
#include <set>
#include <iostream>
#include <cstring>
using namespace mt2mp::action_session;
namespace tc=mt2mp::action_session::tick_commands;
int checks=0;
void need(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
const std::string world="11111111-1111-4111-8111-111111111111",nonce_id="22222222-2222-4222-8222-222222222222";
Context context(uint64_t peer=1){return {17,peer,1,3,peer==1?Role::Host:Role::Client,world};}
std::array<uint8_t,32> hash(){std::array<uint8_t,32> h{};h.fill(7);return h;}
tc::SnapshotRequest request(){return {1,1,0,nonce_id};}
tc::SnapshotBarrier armed(uint64_t peer=1){tc::SnapshotBarrier b;std::string e;need(b.arm(context(peer),request(),5,8,e),"actual ARM failed");return b;}
void lifecycle(){
    std::string e;auto h=armed(),c=armed(2);
    need(c.seal(hash(),e),"Client SEAL");
    const auto prepared=c.packet(tc::wire::SnapshotPhase::Prepared,2);
    need(h.receive(2,prepared,e),"early actual Prepared must wait for Host SEAL");
    need(h.receive(2,prepared,e),"equal duplicate Prepared");
    need(h.seal(hash(),e)&&h.prepared==3,"all covered witnesses equal");
    need(h.begin_capture(e),"all ACKs admit capture");
    need(h.capture_returned(true,{},e),"capture callback returned");
    need(h.confirm_file(nonce_id,e)&&h.begin_release(e),"actual file and companion allow release");
    auto release=h.packet(tc::wire::SnapshotPhase::Release,3);
    need(c.receive(1,release,e)&&c.receive(1,release,e),"Release and duplicate");
    need(c.finish_release(e),"Client native release acknowledgment");
    auto released=c.packet(tc::wire::SnapshotPhase::Released,3);
    need(h.receive(2,released,e)&&h.receive(2,released,e),"Host actual Client Released");
    need(h.finish_release(e)&&!h.active(),"Host complete release");
    need(h.receive(2,prepared,e),"late equal duplicate no mutation");
}
void negatives(){
    std::string e;
    {auto b=armed();need(!b.begin_capture(e)&&e.find("all equal")!=std::string::npos,"no capture without ACKs");}
    {auto b=armed();b.seal(hash(),e);need(!b.begin_capture(e),"local SEAL is not remote ACK");}
    {auto b=armed();auto p=b.packet(tc::wire::SnapshotPhase::Prepared,2);p.witness=hash();p.witness[0]++;need(b.receive(2,p,e)&&!b.seal(hash(),e),"early mismatch retained");}
    for(int field=0;field<7;++field){
        auto b=armed();b.seal(hash(),e);auto p=b.packet(tc::wire::SnapshotPhase::Prepared,2);
        if(field==0)p.session++;if(field==1)p.snapshot_uuid=nonce_id;if(field==2)p.nonce=world;
        if(field==3)p.tick++;if(field==4)p.order++;if(field==5)p.ack_mask=3;if(field==6)p.witness[4]++;
        need(!b.receive(2,p,e)&&b.phase==tc::SnapshotBarrier::Phase::Failed,"mismatched control admitted");
    }
    {auto b=armed();b.seal(hash(),e);auto p=b.packet(tc::wire::SnapshotPhase::Prepared,2);need(!b.receive(3,p,e),"foreign peer");}
    {auto b=armed();b.seal(hash(),e);auto p=b.packet(tc::wire::SnapshotPhase::Prepared,2);b.receive(2,p,e);b.begin_capture(e);
     need(!b.capture_returned(false,"original native failure",e)&&e=="original native failure","original failure retained");
     need(!b.begin_capture(e)&&e=="original native failure","failed capture not replayed");}
    {auto b=armed();b.seal(hash(),e);auto p=b.packet(tc::wire::SnapshotPhase::Prepared,2);b.receive(2,p,e);b.begin_capture(e);b.capture_returned(true,{},e);
     need(!b.begin_release(e),"capture return alone lacks file/companion");}
    {auto b=armed(2);b.seal(hash(),e);need(!b.confirm_file(nonce_id,e),"Client cannot confirm Host file");}
}


void complete_membership(){
    std::string e;auto owner=context();owner.peer_mask=15;tc::SnapshotBarrier host;
    need(host.arm(owner,request(),5,8,e)&&host.seal(hash(),e),"four-peer Host seal");
    for(uint64_t peer:{2,3,4})need(host.receive(peer,host.packet(tc::wire::SnapshotPhase::Prepared,tc::SnapshotBarrier::bit(peer)),e),"physical prepare");
    need(host.prepared==15&&host.begin_capture(e)&&host.capture_returned(true,{},e),"complete preparation admits capture");
    need(host.confirm_file(nonce_id,e)&&host.begin_release(e),"confirmed file admits release");
    for(uint64_t peer:{2,3})need(host.receive(peer,host.packet(tc::wire::SnapshotPhase::Released,15),e),"physical release");
    need(host.released==7&&host.active(),"fourth peer remains held");
    need(host.receive(4,host.packet(tc::wire::SnapshotPhase::Released,15),e)&&host.finish_release(e),"complete release permits retirement");
    need(host.arm(owner,request(),6,9,e),"released barrier can start next snapshot");
}
void control_wire(){
    std::string e;auto b=armed();need(b.seal(hash(),e),"wire seal");
    for(auto phase:{tc::wire::SnapshotPhase::Prepared,tc::wire::SnapshotPhase::Release,tc::wire::SnapshotPhase::Released}){
        tc::wire::Packet packet{3,b.packet(phase,phase==tc::wire::SnapshotPhase::Prepared?2:3)},out;
        std::vector<uint8_t> bytes;need(tc::wire::encode(packet,bytes,e)&&tc::wire::decode(bytes.data(),bytes.size(),out,e)&&out==packet,"snapshot packet roundtrip");
        for(size_t size=0;size<bytes.size();++size)need(!tc::wire::decode(bytes.data(),size,out,e),"truncated snapshot control admitted");
        bytes.push_back(0);need(!tc::wire::decode(bytes.data(),bytes.size(),out,e),"trailing snapshot control admitted");
    }
}
int main()try{lifecycle();negatives();complete_membership();control_wire();std::cout<<checks<<" snapshot barrier checks passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
