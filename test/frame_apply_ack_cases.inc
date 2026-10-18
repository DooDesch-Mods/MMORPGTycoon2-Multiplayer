void frame_ack_cases(std::chrono::steady_clock::time_point now){
    using namespace std::chrono;std::string e;bool waiting=false;
    FrameApplyAcks a;check(a.ready(now,waiting,e)&&!waiting,"initial ACK tracker does not delay first frame");
    a.sent(1,14,now);check(a.pending()==14,"all three connected peers require full Apply");
    check(a.ready(now+seconds(1),waiting,e)&&waiting&&e.find("peers=14")!=std::string::npos,"missing ACK is a visible wait");
    check(!a.validate(1,1,"[]",e)&&!a.validate(5,1,"[]",e)&&!a.validate(2,0,"[]",e)&&!a.validate(2,2,"[]",e),"foreign zero and unsent ACK rejected");
    check(a.validate(2,1,"[]",e),"sent peer ACK admitted");a.acknowledge(2,1,"[]",now+seconds(1));
    a.acknowledge(3,1,"[]",now+seconds(2));check(a.pending()==8,"fast peers cannot release slow peer");
    a.acknowledge(4,1,"[]",now+seconds(3));check(a.pending()==0&&a.recoveries()==1,"all applied peers release backpressure");
    check(a.ready(now+seconds(3)+milliseconds(249),waiting,e)&&waiting&&e.find("250ms")!=std::string::npos,"render cooldown starts after last peer ACK");
    check(a.ready(now+seconds(3)+milliseconds(250),waiting,e)&&!waiting,"cooldown ends at exact250ms");
    check(a.validate(4,1,"[]",e),"identical duplicate ACK admitted");a.acknowledge(4,1,"[]",now+seconds(4));
    check(!a.cooling(now+seconds(4)),"duplicate ACK cannot restart cooldown");
    check(!a.validate(4,1,"[{\"id\":1}]",e)&&e.find("conflict")!=std::string::npos,"conflicting duplicate ACK cannot release host");
    a.sent(2,14,now+seconds(4));a.sent(3,14,now+seconds(5));
    check(a.timely(now+seconds(33),e)&&!a.timely(now+seconds(34),e)&&e.find("peer=2")!=std::string::npos,"authority bypass does not reset missing-ACK timeout");
    a.acknowledge(2,2,"[]",now+seconds(20));check(a.pending()==14,"old progress cannot confirm unsent newer state");
    check(a.validate(2,1,"[]",e),"validated older ACK stays harmless");a.acknowledge(2,1,"[]",now+seconds(21));check(a.peers()[1].revision==2,"older ACK cannot regress progress");
    a.clear();check(a.required()==0&&!a.pending()&&a.ready(now,waiting,e)&&!waiting&&!a.validate(2,1,"[]",e),"snapshot clear removes old ACK ownership and cooldown");

    f={};Bridge b;b.economy_=true;b.ack_mask=2;b.fake_send_time=now;
    check(b.tick(0,false,e,now)&&b.sends==1&&b.apply_acks_.required()==1,"actual tick initial send retains ACK requirement");
    for(int i=1;i<=100;++i)check(b.tick(0,false,e,now+milliseconds(i*5))&&b.sends==1&&b.captures==1,"slow Client cannot accumulate ordinary frames");
    check(b.apply_acks_.validate(2,1,"[]",e),"current ordinary ACK");b.apply_acks_.acknowledge(2,1,"[]",now+milliseconds(500));
    check(b.tick(0,false,e,now+milliseconds(749))&&b.captures==1,"actual tick waits250ms after ACK, not previous send");
    b.fake_send_time=now+milliseconds(750);check(b.tick(0,false,e,b.fake_send_time)&&b.captures==2,"actual tick captures at ACK cooldown boundary");
    // Dedupe does not send a second equal frame; change an authority revision to
    // exercise the next queued frame without changing the native fake state.
    b.fake_send_time=now+milliseconds(751);check(b.tick(1,false,e,b.fake_send_time)&&b.sends==2,"action revision bypass remains immediate");
    b.time_.revision=2;b.fake_send_time=now+milliseconds(752);check(b.tick(1,false,e,b.fake_send_time)&&b.sends==3,"Time revision bypass can supersede stale pending frame");
    check(b.tick(1,true,e,now+seconds(1))&&b.sends==3,"pending action still prevents ordinary capture");
    check(!b.tick(1,true,e,now+milliseconds(30751))&&b.locked_&&e.find("ACK timeout")!=std::string::npos,"missing ACK timeout remains fatal even while action pending");
    check(b.sends==3,"timeout never retries native or queued frame");
}
