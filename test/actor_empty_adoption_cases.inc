namespace actor_empty_fixture {
struct Pin {int selected=0,owner=1;};
struct Port {
    Pin current;int reads=0,guards=0;bool fail_read=false,fail_guard=false;
    bool read(Pin& out,std::string& error){++reads;if(fail_read){error="fixture pin unreadable";return false;}out=current;return true;}
    bool current_owners(const Pin& pin,std::string& error){++guards;if(fail_guard||pin.owner!=current.owner){error="fixture owner changed";return false;}return true;}
};
void run(){
    const std::vector<int> empty,one{1};std::string error;
    Port port;a::EmptyAdoption<Port,Pin> guard(port);
    need(guard.begin(empty,empty,empty,empty,error)&&guard.active(),"Empty Actor read requires no adoption");
    need(port.reads==1&&port.guards==1,"Empty Actor entry preserves Clock and owner proof");
    need(guard.finish(error)&&port.guards==2,"Empty Actor final owner proof retained");
    for(int condition=0;condition<4;++condition){Port full;a::EmptyAdoption<Port,Pin> fallback(full);
        need(fallback.begin(condition==0?one:empty,condition==1?one:empty,condition==2?one:empty,condition==3?one:empty,error)&&!fallback.active(),"Nonempty actor, native owner or lifecycle uses original adoption");
        need(fallback.finish(error)&&full.reads==0&&full.guards==0,"Full adoption retains its own guards");
    }
    Port unreadable;unreadable.fail_read=true;a::EmptyAdoption<Port,Pin> read_failure(unreadable);
    need(!read_failure.begin(empty,empty,empty,empty,error)&&error=="fixture pin unreadable","Empty Actor preserves failed native read cause");
    Port running;running.current.selected=1;a::EmptyAdoption<Port,Pin> clock_failure(running);
    need(!clock_failure.begin(empty,empty,empty,empty,error)&&error=="Actor authoritative adoption requires actual Client Clock0","Empty Actor rejects running Client Clock");
    Port invalid;invalid.fail_guard=true;a::EmptyAdoption<Port,Pin> owner_failure(invalid);
    need(!owner_failure.begin(empty,empty,empty,empty,error)&&error=="fixture owner changed","Empty Actor rejects entry owner drift");
    Port changed;a::EmptyAdoption<Port,Pin> final_failure(changed);
    need(final_failure.begin(empty,empty,empty,empty,error),"Empty Actor prepares fresh owner");changed.current.owner=2;
    need(!final_failure.finish(error)&&error=="fixture owner changed","Empty Actor rejects owner drift after final census");
}
}
