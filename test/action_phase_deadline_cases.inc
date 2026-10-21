struct PhaseFlow {
    uint64_t host_time=0,client_time=0;bool clock_failure=false;
    LocalSession hs{1},cs{2};std::deque<Packet> packets;
    std::unique_ptr<EngineFixture> he=std::make_unique<EngineFixture>(true),ce=std::make_unique<EngineFixture>(true);
    Wire hw{1,packets},cw{2,packets};std::unique_ptr<Controller> host,client;
    std::string error;
    explicit PhaseFlow(uint64_t phase_timeout=10000){
        he->simulated_time=&host_time;ce->simulated_time=&client_time;
        hw.simulated_time=&host_time;cw.simulated_time=&client_time;
        host=std::make_unique<Controller>(hs,*he,hw,Logger{},phase_timeout,[this]{if(clock_failure)throw std::runtime_error("original clock failure");return host_time;});
        client=std::make_unique<Controller>(cs,*ce,cw,Logger{},phase_timeout,[this]{return client_time;});
        check(host->start(0,error)&&client->start(0,error),error);
    }
    void deliver(){
        check(!packets.empty(),"phase test packet missing");auto packet=std::move(packets.front());packets.pop_front();
        // Exercise the real claimed callback, whose previous frame timestamp is stale.
        (packet.to==1?hw:cw).handler(packet.from,packet.bytes.data(),packet.bytes.size());
        check((packet.to==1?host:client)->status().last_packet_accepted,
              "phase callback rejected: "+(packet.to==1?host:client)->status().cause);
    }
};
void phase_deadlines(){
    {
        auto f=std::make_unique<PhaseFlow>(60000);
        f->he->execute_ms=30000;f->ce->apply_ms=30000;f->hw.send_ms=1000;f->cw.send_ms=500;
        check(f->client->submit_developer(true,id,0,f->error),f->error);
        f->host_time=1000;f->deliver();
        check(f->host_time==32000&&f->he->executions==1,"slow Host original/result work was not included");
        f->client_time=32000;
        check(f->client->tick(32000,f->error)&&!f->client->status().locked,
              "finite runtime budget expired before slow Host result arrived");
        f->deliver();
        check(f->client_time==62500&&f->ce->applications==1&&f->client->tick(62500,f->error),
              "matching result did not supersede original request timer during slow Client apply");
        f->host_time=62500;check(f->host->tick(62500,f->error),"Host ACK wait consumed its budget during original work");
        f->deliver();f->client_time=63500;f->deliver();
        check(f->host->status().confirmed==1&&f->client->status().confirmed==1&&
              !f->host->status().pending&&!f->client->status().pending&&
              f->he->executions==1&&f->ce->applications==1,"slow complete transaction lost confirmation or replayed native work");
    }
    {
        auto f=std::make_unique<PhaseFlow>(60000);
        check(f->client->submit_developer(true,id,0,f->error),f->error);
        f->client_time=59999;check(f->client->tick(59999,f->error),"runtime request wait expired before finite bound");
        f->client_time=60000;
        check(!f->client->tick(60000,f->error)&&f->error.find("deadline_ms=60000")!=std::string::npos&&
              f->ce->applications==0&&f->client->status().locked,"missing Host result bypassed finite runtime request bound");
    }
    {
        auto f=std::make_unique<PhaseFlow>(60000);f->he->execute_ms=30000;f->hw.send_ms=1000;
        check(f->client->submit_developer(true,id,0,f->error),f->error);f->host_time=1000;f->deliver();
        f->packets.clear();f->host_time=91999;
        check(f->host->tick(91999,f->error)&&!f->host->status().locked,"runtime ACK wait expired before post-send phase bound");
        f->host_time=92000;
        check(f->host->tick(92000,f->error)&&f->host->status().locked&&f->host->status().outcome==Outcome::Unknown&&f->host->status().ack_mask==1&&
              f->host->status().cause.find("deadline_ms=92000")!=std::string::npos&&f->he->executions==1,
              "missing native ACK did not fail at finite runtime bound or replayed original action");
    }
    for(bool drop_completion:{false,true}){
        auto f=std::make_unique<PhaseFlow>();
        f->he->execute_ms=6000;f->ce->apply_ms=9000;f->hw.send_ms=1000;f->cw.send_ms=500;
        check(f->client->submit_developer(true,id,0,f->error),f->error);
        f->host_time=1000;f->deliver();check(f->host_time==8000&&f->he->executions==1,"Host native work/send timing missing");
        f->client_time=8000;f->deliver();check(f->client_time==17500&&f->ce->applications==1,"Client apply/send timing missing");
        check(f->client->tick(17500,f->error)&&!f->client->status().locked,"old local request deadline survived matching HostResult");
        f->host_time=17999;check(f->host->tick(17999,f->error)&&!f->host->status().locked,"Host ACK budget started before native work/result send");
        f->deliver();check(f->host->status().confirmed==1&&f->host->status().ack_mask==3,"fresh claimed callback ACK did not confirm original-once action");
        if(drop_completion){
            f->packets.clear();f->client_time=27499;
            check(f->client->tick(27499,f->error),"Client Completion wait lost full post-ACK budget");
            f->client_time=27500;
            check(!f->client->tick(27500,f->error)&&f->error.find("deadline_ms=27500")!=std::string::npos&&f->client->status().locked,
                  "missing Completion did not expire at its exact unchanged ten-second phase bound");
        }else{
            f->client_time=18000;f->deliver();
            check(f->client->status().confirmed==1&&!f->client->status().pending,"separate request/result/apply/completion phases failed");
        }
        check(f->he->executions==1&&f->ce->applications==1,"phase clocks replayed original native work");
    }
    {
        auto f=std::make_unique<PhaseFlow>();f->ce->wait=true;
        check(f->client->submit_developer(true,id,0,f->error),f->error);f->deliver();
        auto result=f->packets.front();f->client_time=9000;f->deliver();
        check(f->ce->applications==0&&f->client->status().pending,"finite authority wait applied early");
        f->client_time=18000;check(f->client->tick(18000,f->error),"matching HostResult failed to retire old request timer");
        f->cw.handler(result.from,result.bytes.data(),result.bytes.size());
        check(f->client->status().last_packet_accepted&&!f->ce->applications,"authority result duplicate replayed native apply");
        f->client_time=19000;
        check(!f->client->tick(19000,f->error)&&f->error.find("deadline_ms=19000")!=std::string::npos,
              "authority polling or duplicate result extended its finite deadline");
    }
    {
        auto f=std::make_unique<PhaseFlow>();check(f->client->submit_developer(true,id,0,f->error),f->error);
        f->deliver();auto result=f->packets.front();f->client_time=1000;f->deliver();f->packets.clear();
        f->client_time=9000;f->cw.handler(result.from,result.bytes.data(),result.bytes.size());
        check(f->client->status().last_packet_accepted&&f->ce->applications==1,"confirmed result duplicate replayed apply");
        f->client_time=11000;check(!f->client->tick(11000,f->error)&&f->error.find("deadline_ms=11000")!=std::string::npos,
              "duplicate ApplyAck resend extended Completion wait");
    }
    {
        auto f=std::make_unique<PhaseFlow>();check(f->client->submit_developer(true,id,0,f->error),f->error);
        f->client_time=10000;check(!f->client->tick(10000,f->error)&&f->ce->applications==0,"missing HostResult bypassed original request timeout");
    }
    {
        auto f=std::make_unique<PhaseFlow>();check(f->client->submit_developer(true,id,0,f->error),f->error);
        const auto request=f->packets.front();f->deliver();f->host_time=9000;
        f->hw.handler(request.from,request.bytes.data(),request.bytes.size());
        check(f->host->status().last_packet_accepted&&f->he->executions==1,"duplicate request replayed Host native action");
        f->host_time=10000;f->host->tick(10000,f->error);
        check(f->host->status().locked&&f->host->status().cause.find("deadline_ms=10000")!=std::string::npos,"duplicate request extended Host ACK deadline");
    }
    {
        auto f=std::make_unique<PhaseFlow>();f->clock_failure=true;
        check(!f->host->tick(0,f->error)&&f->error.find("original clock failure")!=std::string::npos&&f->host->status().locked,"clock callback failure cause was lost");
    }
    {
        auto f=std::make_unique<PhaseFlow>();f->host_time=10;check(f->host->tick(10,f->error),f->error);
        f->host_time=9;check(!f->host->tick(9,f->error)&&f->error=="action monotonic clock regressed","backward clock was silently accepted");
    }
    {
        auto f=std::make_unique<PhaseFlow>();f->client_time=UINT64_MAX-9999;
        check(!f->client->submit_developer(true,id,0,f->error)&&f->error=="ACK deadline timestamp exhausted"&&f->packets.empty(),"deadline overflow sent a request");
    }
}
