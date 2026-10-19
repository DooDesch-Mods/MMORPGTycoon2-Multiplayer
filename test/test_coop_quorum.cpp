#define main retained_staff_main
#include "test_tutorial_staff_controller.cpp"
#undef main

int main()try{
    unsigned scenarios=0;
    for(const uint8_t mask:{uint8_t(1),uint8_t(3),uint8_t(5),uint8_t(9),uint8_t(13),uint8_t(15)}){
        LocalSession session(1);session.state.peer_mask=mask;
        LocalEngine engine;std::deque<Packet> packets;Wire wire(1,packets);
        Controller controller(session,engine,wire,{});std::string error;
        need(controller.start(0,error),error);
        need(controller.submit_startingpoint_workers(id,1,0,error),error);
        need(engine.starts==1&&engine.receipt.retirement_confirmed,"Host execution/retirement was not observed exactly once");
        if(mask==1){
            const auto state=controller.status();
            need(!state.pending&&!state.locked&&state.confirmed==1&&state.required_mask==1&&state.ack_mask==1,
                "solo Host action waited for a nonexistent peer instead of local execution evidence");
            need(packets.empty(),"solo Host sent a fabricated peer packet");
            need(controller.tick(100000,error)&&controller.status().confirmed==1&&engine.starts==1,
                "solo Host replayed an effect or timed out after confirmed local completion");
        }else{
            need(controller.status().pending&&controller.status().confirmed==0,"Host ignored the current physical peer quorum");
            uint8_t sent=0;
            for(const auto& packet:packets){need(packet.target>=2&&packet.target<=4,"invalid physical peer target");sent|=uint8_t(1u<<(packet.target-1));}
            need(sent==uint8_t(mask&~1u),"Host targeted an absent slot or omitted a present slot");
        }
        ++scenarios;
    }
    for(const uint8_t mask:{uint8_t(0),uint8_t(2),uint8_t(4),uint8_t(16),uint8_t(31)}){
        LocalSession session(1);session.state.peer_mask=mask;LocalEngine engine;std::deque<Packet> packets;Wire wire(1,packets);
        Controller controller(session,engine,wire,{});std::string error;
        need(!controller.start(0,error)&&!error.empty()&&engine.starts==0,"invalid Host membership caused an effect");++scenarios;
    }
    std::cout<<scenarios<<" actual Controller quorum scenarios passed\n";return 0;
}catch(const std::exception& cause){std::cerr<<cause.what()<<'\n';return 1;}
