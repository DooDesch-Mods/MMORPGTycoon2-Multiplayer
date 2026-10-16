#include "developer_staff/bug_policy.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::game::developer_staff;
namespace policy=mt2mp::game::developer_staff::bug_policy;
int cases=0;
void need(bool value,const std::string& why){++cases;if(!value)throw std::runtime_error(why);}
struct TestPort final:policy::Port {
    policy::Snapshot state;
    policy::Control binding{1,2,3,4,5,6,7};
    std::vector<std::string> calls;
    bool ready=true,pressed=false,fail_release=false,changed_binding=false,changed_ledger=false,wrong_value=false,other_value=false,throw_press=false,show_visible=true,page_requested=true;
    bool read(policy::Snapshot& out,std::string&)override{out=state;return true;}
    bool window(const policy::Snapshot&,policy::Control& out,std::string& e,bool shown=false)override{out=binding;if(shown&&!show_visible){e="original Show stayed hidden";return false;}return true;}
    bool control(const policy::Snapshot&,policy::Scope,policy::Control& out,bool down,std::string& e)override{
        if(!ready){e="actual Policies checkbox is ambiguous";return false;}
        if(down!=pressed){e="actual pressed flag differs";return false;}
        out=binding;if(down&&changed_binding)++out.checkbox;return true;
    }
    bool show(const policy::Control&,std::string&)override{calls.push_back("show");return true;}
    bool page(const policy::Control&,std::string&)override{calls.push_back("page");return true;}
    bool requested(const policy::Control&,std::string& e)override{if(!page_requested){e="original SetPage request differs";return false;}return true;}
    bool input(const policy::Control&,policy::Input input,std::string& e)override{
        if(input==policy::Input::Hover){calls.push_back("hover");return true;}
        if(input==policy::Input::Press){calls.push_back("press");pressed=true;if(throw_press)throw std::runtime_error("original Press exception");return true;}
        calls.push_back("release");if(fail_release){e="original Release SEH=5";return false;}pressed=false;
        if(!wrong_value)state.policies[policy::index(scope)]^=1;
        if(other_value)state.policies[0]^=1;
        if(changed_ledger)++state.capture.pin.ledger.line_counts[6];
        return true;
    }
    policy::Scope scope=policy::Scope::Online;
};
policy::Request request(){return {"11111111-1111-4111-8111-111111111111","22222222-2222-4222-8222-222222222222",7,9,policy::Scope::Online,policy::Command::Toggle,false};}
TestPort port(){TestPort p;p.state.capture.pin.origin.uuid=request().origin_uuid;p.state.capture.pin.origin.world_epoch=7;p.state.capture.pin.origin.generation=9;return p;}
nlohmann::json args(){return {{"command","Toggle"},{"scope","online"},{"token",request().token},{"native_origin_uuid",request().origin_uuid},{"epoch",7},{"generation",9},{"expected",false}};}
int main(){try{
    std::string e;policy::Receipt out;auto r=request();auto p=port();policy::Fixture fixture;
    const nlohmann::json running={{"initialized",true},{"started",true},{"locked",false},
        {"pending",false},{"lobby_retiring",false},{"cause",""}};
    need(policy::ready_controller(running,true,e),"healthy running live session rejected: "+e);
    need(!policy::ready_controller(running,false,e)&&e=="Bug-C policy UI requires active live authority","inactive live session accepted");
    for(const auto* field:{"initialized","started","locked","pending","lobby_retiring"}){
        auto unavailable=running;unavailable[field]=!unavailable[field].get<bool>();
        need(!policy::ready_controller(unavailable,true,e)&&!e.empty(),"unavailable Controller accepted");
    }
    auto failed=running;failed["cause"]="original ordered action failed";
    need(!policy::ready_controller(failed,true,e)&&e.find("original ordered action failed")!=std::string::npos,"Controller original cause lost");
    need(!policy::ready_controller(nlohmann::json{{"error","wrong native thread"}},true,e)&&e.find("wrong native thread")!=std::string::npos,"Controller original status error lost");
    need(fixture.run(r,p,out,e)&&out.confirmed&&!out.unknown&&out.attempted_mask==7&&out.returned_mask==7,"ordinary online toggle did not confirm: "+e);
    need(p.calls==std::vector<std::string>{"hover","press","release"}&&p.state.policies[2]==1,"original input ordering/count wrong");
    need(!fixture.run(r,p,out,e)&&out.replay_rejected&&p.calls.size()==3,"successful token replayed input");
    p=port();r.command=policy::Command::Show;policy::Fixture show;
    need(show.run(r,p,out,e)&&p.calls==std::vector<std::string>{"show","page"}&&out.confirmed,"Show did not request original page once");
    p=port();p.show_visible=false;policy::Fixture hidden;
    need(!hidden.run(r,p,out,e)&&out.unknown&&out.returned_mask==1&&e=="original Show stayed hidden"&&p.calls==std::vector<std::string>{"show"},"returned Show without visible effect falsely confirmed");
    p=port();p.page_requested=false;policy::Fixture missing_page;
    need(!missing_page.run(r,p,out,e)&&out.unknown&&out.returned_mask==3&&e=="original SetPage request differs","returned SetPage without request falsely confirmed");
    p=port();p.scope=policy::Scope::Offline;r=request();r.scope=p.scope;policy::Fixture offline;
    need(offline.run(r,p,out,e)&&p.state.policies[5]==1&&p.state.policies[2]==0,"offline toggle touched online policy");
    p=port();p.state.policies[2]=1;r=request();r.expected=true;policy::Fixture disable;
    need(disable.run(r,p,out,e)&&p.state.policies[2]==0,"true-to-false original UI toggle failed");
    p=port();p.ready=false;r=request();policy::Fixture missing;
    need(!missing.run(r,p,out,e)&&e=="actual Policies checkbox is ambiguous"&&p.calls.empty()&&!missing.locked(),"ambiguous binding called native input");
    p=port();r=request();r.expected=true;policy::Fixture stale;
    need(!stale.run(r,p,out,e)&&p.calls.empty()&&e.find("expected")!=std::string::npos,"stale expected policy called input");
    for(int kind=0;kind<3;++kind){p=port();r=request();if(kind==0)r.origin_uuid="33333333-3333-4333-8333-333333333333";if(kind==1)++r.epoch;if(kind==2)++r.generation;policy::Fixture f;need(!f.run(r,p,out,e)&&p.calls.empty(),"foreign native Origin called input");}
    p=port();p.fail_release=true;r=request();policy::Fixture partial;
    need(!partial.run(r,p,out,e)&&out.unknown&&!out.confirmed&&partial.locked()&&e=="original Release SEH=5"&&out.attempted_mask==7&&out.returned_mask==3,"partial native failure became success/lost cause");
    const auto calls=p.calls;
    need(!partial.run(r,p,out,e)&&out.replay_rejected&&out.cause=="original Release SEH=5"&&p.calls==calls,"partial token replayed/lost cause");
    r.token="44444444-4444-4444-8444-444444444444";
    need(!partial.run(r,p,out,e)&&p.calls==calls&&e=="original Release SEH=5","new token bypassed retained Unknown");
    for(int kind=0;kind<4;++kind){p=port();r=request();p.changed_binding=kind==0;p.changed_ledger=kind==1;p.wrong_value=kind==2;p.other_value=kind==3;policy::Fixture f;
        need(!f.run(r,p,out,e)&&out.unknown&&!out.confirmed&&f.locked()&&!e.empty(),"changed binding/ledger/policy falsely confirmed");
        if(kind==0)need(p.calls==std::vector<std::string>{"hover","press"},"changed pressed binding invoked Release on foreign owner");}
    p=port();p.throw_press=true;r=request();policy::Fixture throwing;
    need(!throwing.run(r,p,out,e)&&out.unknown&&throwing.locked()&&e.find("original Press exception")!=std::string::npos,"native exception lost cause/Unknown");
    policy::Request decoded;
    need(policy::parse(args(),decoded,e)&&decoded.expected==false&&decoded.epoch==7,"valid exact request rejected");
    for(const auto* field:{"command","scope","token","native_origin_uuid","epoch","generation","expected"}){auto a=args();a.erase(field);need(!policy::parse(a,decoded,e),"missing argument accepted");}
    for(int kind=0;kind<9;++kind){auto a=args();if(kind==0)a["extra"]=0;if(kind==1)a["expected"]=0;if(kind==2)a["epoch"]=true;if(kind==3)a["generation"]=-1;if(kind==4)a["epoch"]=0;if(kind==5)a["command"]="SetKnown";if(kind==6)a["scope"]="all";if(kind==7)a["token"]="uppercase-or-nonuuid";if(kind==8)a["generation"]=1.5;need(!policy::parse(a,decoded,e),"wrong typed/scalar request accepted");}
    std::cout<<"PASS "<<cases<<" actual policy parser/orchestration assertions; native calls are controlled ports\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
