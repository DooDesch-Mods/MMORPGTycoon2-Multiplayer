// This fixture controls command scope; native action bodies and identity comparisons remain real.
namespace ordered_test {
bool enabled=false;
mt2mp::action_session::Context context;
mt2mp::action_session::Binding binding;
mt2mp::action_session::Request request;
void bind(const mt2mp::action_session::Context& c,const mt2mp::action_session::Binding& b,const mt2mp::action_session::Request& r){enabled=true;context=c;binding=b;request=r;}
}
namespace mt2mp::action_session::tick_commands::authority {
bool active(){return ordered_test::enabled;}
bool matches(const Context& c,const Binding& b,const Request& r,std::string& e){
    if(!active()||c!=ordered_test::context||b!=ordered_test::binding||r!=ordered_test::request){e="fixture ordered command Context/Binding/request differs";return false;}
    return true;
}
}
