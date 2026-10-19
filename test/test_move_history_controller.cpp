#define main retained_building_history_main
#include "test_building_remove_restore_controller.cpp"
#undef main
namespace {
struct MovingNative:Native {
    bool malformed=false;
    MovingNative(){state=building();state.x=10;}
    Outcome execute_action(const Binding&,const Request& r,std::string&)override{
        ++effects;receipt={};receipt.before=receipt.after=money;receipt.object_before=state;
        state.x=r.action==Action::UndoBuilding?8.0f:10.0f;receipt.object_after=state;receipt.undo_confirmed=true;
        if(malformed)receipt.object_after.type="DifferentType";return Outcome::Confirmed;
    }
};
void move_flow(bool baseline,bool malformed){
    std::deque<Packet> packets;LocalSession hs(1),cs(2);MovingNative he,ce;Wire hw(1,packets),cw(2,packets);
    Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string error;
    const auto key=baseline?std::string(64,'a')+":world:building:44":"action:"+placed;
    he.malformed=malformed;need(host.start(0,error)&&client.start(0,error),error);
    const auto original_money=he.money;
    need(client.submit_building_history(Action::UndoBuilding,undo,removed,key,0,error),error);
    need(he.effects==0&&ce.effects==0,"Move inverse ran before Host ordering");
    const Packet request=packets.front();
    for(unsigned i=0;!packets.empty()&&i<16;++i){auto p=packets.front();packets.pop_front();(p.target==1?host:client).receive(p.peer,p.bytes.data(),p.bytes.size(),1,error);}
    if(malformed){need(host.status().confirmed==0&&host.status().locked&&he.effects==1&&ce.effects==0,"malformed inverse result confirmed");return;}
    need(host.status().confirmed==1&&client.status().confirmed==1&&host.status().ack_mask==3&&
         he.effects==1&&ce.effects==1&&he.state==ce.state&&he.state.x==8&&he.money==original_money&&ce.money==original_money,
         "Move Undo lacks original effect, parity or both real ACKs: "+host.status().cause+"/"+client.status().cause);
    host.receive(request.peer,request.bytes.data(),request.bytes.size(),2,error);
    for(unsigned i=0;!packets.empty()&&i<16;++i){auto p=packets.front();packets.pop_front();(p.target==1?host:client).receive(p.peer,p.bytes.data(),p.bytes.size(),2,error);}
    need(he.effects==1&&ce.effects==1,"duplicate Move Undo replayed");
    need(host.submit_building_history(Action::RedoBuilding,redo,removed,key,3,error),error);
    for(unsigned i=0;!packets.empty()&&i<16;++i){auto p=packets.front();packets.pop_front();(p.target==1?host:client).receive(p.peer,p.bytes.data(),p.bytes.size(),4,error);}
    need(host.status().confirmed==2&&client.status().confirmed==2&&he.effects==2&&ce.effects==2&&he.state==ce.state&&
         he.state.x==10&&he.money==original_money&&ce.money==original_money,"Move Redo lacks matching full state");
}
}
int main(){if(retained_building_history_main()!=0)return 1;try{
    for(bool baseline:{false,true})for(bool malformed:{false,true})move_flow(baseline,malformed);
    std::cout<<"shared Move Undo/Redo Controller flows passed; native effects use doubles\n";return 0;
}catch(const std::exception& cause){std::cerr<<cause.what()<<'\n';return 1;}}
