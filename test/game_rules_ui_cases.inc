namespace rules_ui_fixture {
namespace rules=gr;
std::map<std::pair<uintptr_t,size_t>,uint64_t> fields;
std::map<uintptr_t,std::string> names;
std::map<uintptr_t,std::vector<uintptr_t>> children;
std::vector<uintptr_t> registry;
std::array<uint32_t,14> fixture_numeric_draft{};
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
struct Memory {bool copy(uintptr_t address,void* out,size_t bytes,std::string& e){if(address!=777||bytes!=registry.size()*sizeof(uintptr_t))return fail(e,"Registry array read differs");std::memcpy(out,registry.data(),bytes);return true;}};
struct Runtime {uintptr_t image=100000;Memory memory;};
template<class T>bool at(Runtime&,uintptr_t object,size_t offset,T& out,std::string& e){if constexpr(std::is_same_v<T,std::array<uint32_t,14>>){if(offset!=0x350)return fail(e,"Numeric draft offset differs");out=fixture_numeric_draft;return true;}else{auto found=fields.find({object,offset});if(found==fields.end())return fail(e,"Missing UI field");out=T(found->second);return true;}}
bool text(Runtime&,uintptr_t object,std::string& out,size_t,std::string& e){if(!names.contains(object))return fail(e,"Missing UI text");out=names.at(object);return true;}
bool ap(Runtime& r,uintptr_t object,uintptr_t expected,std::string& e){uintptr_t actual=0;return at(r,object,0,actual,e)&&(actual==r.image+expected||fail(e,"UI native class differs"));}
bool service_ui_children(Runtime&,uintptr_t pane,uintptr_t,uintptr_t,std::vector<uintptr_t>& pending,std::string&){for(auto child:children[pane])pending.push_back(child);return true;}
#include "rules_ui_find_actual.inc"
#include "rules_numeric_find_actual.inc"
void reset(Runtime& r){fields.clear();names.clear();children.clear();registry={10};
    fields[{r.image,0x1a3c478}]=777;fields[{r.image,0x1a3c480}]=1;fields[{r.image,0x1a3c484}]=1;
    fields[{10,0}]=r.image+0x16ff0a0;fields[{10,0x318}]=0;fields[{10,0x45f}]=1;names[10+0x1b0]="design_combat";
    fields[{20,0}]=r.image+0x1745a80;fields[{30,0}]=r.image+0x16d98c0;children[10]={20};children[20]={30};
    fields[{20,0x278}]=10;fields[{30,0x278}]=20;fields[{20,0x280}]=fields[{30,0x280}]=10;
    fields[{20,0x69}]=fields[{30,0x69}]=fields[{30,0x174}]=1;
    fields[{30,0x3c8}]=40;fields[{40,0}]=r.image+0x1749610;fields[{40,8}]=1000+0x1c0;fields[{40,0x38}]=1000+0x1dc;names[40+0x18]="delayForToons";
}
void run(){Runtime r;rules::Pin pin;pin.design=1000;RulesUi ui;std::string e;
    reset(r);need(rules_ui_find(r,pin,ui,e)&&ui.window==10&&ui.checkbox==30&&ui.visible&&ui.ready,"Actual UI finder rejected offered bound checkbox");
    reset(r);fields[{10,0x45f}]=0;need(rules_ui_find(r,pin,ui,e)&&ui.window==10&&!ui.visible&&!ui.ready&&!ui.checkbox,"Hidden Combat window exposed checkbox");
    reset(r);fields[{20,0x69}]=0;need(rules_ui_find(r,pin,ui,e)&&!ui.ready,"Hidden ancestor exposed checkbox");
    reset(r);fields[{30,0x174}]=0;need(rules_ui_find(r,pin,ui,e)&&!ui.ready,"Unclickable checkbox became ready");
    reset(r);fields[{40,0x38}]++;need(!rules_ui_find(r,pin,ui,e),"Foreign bound scalar accepted");
    reset(r);names[40+0x18]="enabled";need(!rules_ui_find(r,pin,ui,e),"Foreign bound property accepted");
    reset(r);fields[{40,8}]++;need(rules_ui_find(r,pin,ui,e)&&!ui.checkbox&&!ui.ready,"Foreign model owner exposed checkbox");
    reset(r);fields[{30,0x278}]=30;need(!rules_ui_find(r,pin,ui,e),"Visible ancestry cycle accepted");
    reset(r);registry={10,10};fields[{r.image,0x1a3c480}]=fields[{r.image,0x1a3c484}]=2;need(!rules_ui_find(r,pin,ui,e),"Duplicate actual registry window accepted");
    reset(r);fields[{r.image,0x1a3c480}]=2;need(!rules_ui_find(r,pin,ui,e),"Invalid registry extent accepted");
}
void numeric_setup(Runtime& r,bool xp){reset(r);fixture_numeric_draft={};fixture_numeric_draft[1]=10;
    fields[{10,0}]=r.image+(xp?0x17883d0:0x16ff0a0);
    names[10+0x1b0]=xp?"design_levels":"design_combat";fields[{30,0}]=r.image+0x1701d20;fields[{30,0x450}]=40;fields[{30,0x458}]=50;fields[{40,0}]=r.image+0x17496b0;
    fields[{40,8}]=xp?20+0x338:1000+0x1c0;fields[{40,0x38}]=xp?20+0x354:1000+0x1d8;names[40+0x18]=xp?"xpPerMob":"seconds";
    fields[{50,0}]=r.image+0x1703290;fields[{50,0x278}]=30;fields[{50,0x280}]=10;fields[{50,0x69}]=fields[{50,0x174}]=1;fields[{50,0x4a0}]=0;names[50+0x3e0]="MessageTo Parent ValueEdited";names[50+0x400]="";children[30]={50};
    if(xp){fields[{20,0}]=r.image+0x1703ce0;fields[{20+0x338,0}]=r.image+0x16d9f40;fields[{20,0x318}]=60;fields[{60,0}]=r.image+0x16e3ec0;fields[{60,0x278}]=20;fields[{60,0x280}]=10;fields[{60,0x69}]=fields[{60,0x174}]=1;fields[{60,0x46d}]=0;names[60+0x1b0]="xp_apply";names[60+0x388]="MessageTo Parent Apply";children[20].push_back(60);}
}
void numeric_run(){Runtime r;rules::Pin pin;pin.design=1000;RulesNumericUi ui;std::string e;
    for(bool xp:{false,true}){
        numeric_setup(r,xp);fields[{10,0x45f}]=0;
        need(rules_numeric_find(r,pin,xp,ui,e)&&ui.window==10&&!ui.visible&&!ui.ready,"Correct hidden stock window was not registered");
        fields[{10,0}]=r.image+(xp?0x16ff0a0:0x17883d0);
        need(rules_numeric_find(r,pin,xp,ui,e)&&!ui.window,"Opposite stock window class passed name-only matching");
        numeric_setup(r,xp);names[10+0x1b0]=xp?"design_combat":"design_levels";
        need(rules_numeric_find(r,pin,xp,ui,e)&&!ui.window,"Wrong named stock window accepted");
        numeric_setup(r,xp);fields[{10,0x318}]=123;
        need(rules_numeric_find(r,pin,xp,ui,e)&&!ui.window,"Window with foreign logic accepted");
        numeric_setup(r,xp);fields[{10,0x45f}]=2;
        need(rules_numeric_find(r,pin,xp,ui,e)&&!ui.window,"Non-boolean stock visibility accepted");
        numeric_setup(r,xp);registry={10,10};fields[{r.image,0x1a3c480}]=fields[{r.image,0x1a3c484}]=2;
        need(!rules_numeric_find(r,pin,xp,ui,e),"Duplicate stock named window accepted");
    }
    need(rules_numeric_integer(json(-1))&&rules_numeric_integer(json(INT32_MAX)),"Native numeric endpoint rejected");for(const auto& bad:{json(true),json(-2),json(uint64_t(UINT64_MAX)),json(uint64_t(INT32_MAX)+1)})need(!rules_numeric_integer(bad),"Native numeric overflow/coercion accepted");
    for(bool xp:{false,true}){numeric_setup(r,xp);need(rules_numeric_find(r,pin,xp,ui,e)&&ui.ready&&ui.edit==50&&ui.pane==30&&(!xp||(ui.apply_ready&&ui.draft==fixture_numeric_draft)),"Offered numeric PropertyPane finder failed");
        fields[{50,0x4a0}]=123;need(!rules_numeric_find(r,pin,xp,ui,e),"Direct TextEdit model binding allowed early gameplay store");fields[{50,0x4a0}]=0;
        names[50+0x400]="MessageTo Parent Preview";need(!rules_numeric_find(r,pin,xp,ui,e),"Unproved preview callback allowed before capture");names[50+0x400]="";
        fields[{30,0x69}]=0;need(rules_numeric_find(r,pin,xp,ui,e)&&!ui.ready,"Hidden numeric control became ready");
    }
    numeric_setup(r,true);children[10]={70};children[70]={};fields[{70,0}]=r.image+0x17015b0;fields[{70,0x3b8}]=80;fields[{80,0}]=r.image+0x16dfe90;fields[{80,0x280}]=10;fields[{80,0x410}]=fields[{80,0x418}]=20;fields[{80,0x3e0}]=0x656c6f7370;fields[{80,0x408}]=0;names[20+0x1b0]=names[80+0x3e8]="{design_tab_xp}";
    fields[{20,0x278}]=80;fields[{80,0x278}]=70;fields[{70,0x278}]=10;fields[{70,0x280}]=10;fields[{80,0x69}]=fields[{70,0x69}]=1;
    need(rules_numeric_find(r,pin,true,ui,e)&&ui.ready&&ui.apply_ready,"Actual settled XP tab route failed");
    names[80+0x3e8]="";names[70+0x360]="";
    need(rules_numeric_find(r,pin,true,ui,e)&&ui.ready&&ui.apply_ready&&ui.draft==fixture_numeric_draft,"Native empty default lost the actual first settled XP page");
    names[80+0x3e8]="foreign";need(rules_numeric_find(r,pin,true,ui,e)&&!ui.ready&&!ui.view,"Nonempty wrong requested page was accepted");names[80+0x3e8]="";
    names[70+0x360]="foreign";need(rules_numeric_find(r,pin,true,ui,e)&&!ui.ready&&!ui.view,"Configured nondefault page was ignored");names[70+0x360]="";
    fields[{80,0x418}]=21;need(rules_numeric_find(r,pin,true,ui,e)&&!ui.ready&&!ui.view,"Default next-page mismatch exposed controls");fields[{80,0x418}]=20;
    fields[{80,0x410}]=fields[{80,0x418}]=0;need(rules_numeric_find(r,pin,true,ui,e)&&!ui.ready&&!ui.view,"Absent default current page exposed controls");fields[{80,0x410}]=fields[{80,0x418}]=20;
    for(const auto key:{std::pair<uintptr_t,size_t>{80,0x278},{70,0x280},{20,0x278},{20,0x280}}){const auto before=fields[key];fields[key]=999;need(!rules_numeric_find(r,pin,true,ui,e),"Foreign default page ancestry was accepted");fields[key]=before;}
for(uintptr_t transition:{uintptr_t(1),uintptr_t(2)}){fields[{80,0x408}]=transition;need(rules_numeric_find(r,pin,true,ui,e)&&!ui.ready&&!ui.apply_ready,"Unsettled XP tab exposed editable controls");}fields[{80,0x408}]=3;need(!rules_numeric_find(r,pin,true,ui,e),"Invalid native tab transition enum accepted");
}
}
