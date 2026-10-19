#define main group_reader_fixture_main
#include "test_group_reader.cpp"
#undef main
#include "duplicate_service_history.h"
#include "duplicate_quest_history.h"
#include "duplicate_building_history.h"
namespace sh=mt2mp::game::duplicate_service_history;
namespace qh=mt2mp::game::duplicate_quest_history;
namespace bh=mt2mp::game::duplicate_building_history;
int main()try{
    std::string error;
    mt2mp::action_session::ServiceMembershipState price_state;mt2mp::service_membership_reader::Local price_local;
    price_state.rows.resize(1);price_local.rows.resize(1);price_state.rows[0].price={0,15};
    price_state.configs={{"Inn",{0,30}}};price_state.rows[0].config_key="Inn";price_state.config_present=true;price_local.configs={0x33000};
    std::array<int32_t,2> effective{};
    f::need(sh::effective_price(price_state,price_local,0,effective,error)&&effective==std::array<int32_t,2>{0,15},"non-sync Service Price changed");
    price_state.rows[0].flags[0]=true;
    f::need(sh::effective_price(price_state,price_local,0,effective,error)&&effective==std::array<int32_t,2>{0,30},"unresolved sync Service Price did not use original named config");
    price_local.rows[0].config=0x33000;
    f::need(sh::effective_price(price_state,price_local,0,effective,error)&&effective==std::array<int32_t,2>{0,30},"cached sync Service Price did not use its current owner");
    price_local.rows[0].config=0x44000;f::need(!sh::effective_price(price_state,price_local,0,effective,error),"foreign cached Service config accepted");
    price_local.rows[0].config=0;price_state.config_present=false;
    f::need(sh::effective_price(price_state,price_local,0,effective,error)&&effective==std::array<int32_t,2>{0,15},"absent Config manager changed effective Service Price");
    price_state.config_present=true;price_state.rows[0].config_key="Missing";
    f::need(sh::effective_price(price_state,price_local,0,effective,error)&&effective==std::array<int32_t,2>{0,15},"missing Config name changed effective Service Price");
    price_state.rows[0].config_key.clear();f::need(!sh::effective_price(price_state,price_local,0,effective,error),"native empty config assertion entered preflight");
    constexpr uintptr_t service=0x91000,building=0x94000,destination=0x96000,node=0xa0000;
    sh::Daily daily;daily.entries={5,7};daily.history={3,6,9};daily.words={4,22,100,10,8,2};daily.warning=1;
    auto put_daily=[&](f::Memory& memory,uintptr_t pointer,const sh::Daily& value,int32_t extra_capacity){
        memory.put(pointer,f::image+0x16e4710);
        for(size_t i=0;i<2;++i){const auto offset=i?size_t(0x40):size_t(0x28);const auto& values=i?value.history:value.entries;
            const auto data=pointer+0x8000+i*0x1000;memory.put(pointer+offset,data);memory.put(pointer+offset+8,int32_t(values.size()));
            memory.put(pointer+offset+12,int32_t(values.size())+extra_capacity);
            for(size_t j=0;j<values.size();++j)memory.put(data+j*4,values[j]);}
        memory.put(pointer+0x50,value.words);memory.put(pointer+0x68,value.warning);
    };
    auto fixture=[&](){f::Memory memory;memory.put(service,f::image+0x1718480);memory.put(service+0x18,uint64_t(91));
        memory.put(service+0x148,building);memory.put(building+0x18,uint64_t(94));memory.put(destination+0x18,uint64_t(96));
        memory.put(service+0x20,int32_t(2));memory.put(service+0x48,std::array<uint8_t,3>{1,0,1});memory.put(service+0x130,destination);
        memory.put(service+0x28,f::image+0x174ab50);memory.put(service+0x40,std::array<int32_t,2>{0,25});
        put_daily(memory,service+0x50,daily,4);put_daily(memory,service+0xc0,daily,6);
        memory.put(node,f::image+0x1737830);memory.put(f::image+0x1737830+0x10,f::image+0x547370);
        memory.put(node+0xb8,uint64_t(91));memory.put(node+0xc0,int32_t(2));memory.put(node+0xc8,f::image+0x174ab50);
        memory.put(node+0xe0,std::array<int32_t,2>{0,25});memory.put(node+0x108,std::array<uint8_t,3>{1,0,1});
        memory.put(node+0x1f0,uint64_t(94));memory.put(node+0x1f8,uint64_t(96));
        put_daily(memory,node+0x110,daily,0);put_daily(memory,node+0x180,daily,0);return memory;};
    auto memory=fixture();bool alive=true;
    auto find=[&](uint64_t uid,uintptr_t& pointer,std::string&){pointer=uid==91?(alive?service:0):uid==94?building:uid==96?destination:0;return true;};
    sh::Saved saved;f::need(sh::capture(memory,f::image,service,building,{0,25},find,saved,error),error);
    f::need(saved.counts[0]==daily&&saved.counts[1]==daily,"complete native DailyCount arrays/scalars not captured");
    alive=false;f::need(sh::inverse(memory,f::image,node,saved,find,error),error);
    auto bad=[&](auto change){memory=fixture();change(memory);f::need(!sh::inverse(memory,f::image,node,saved,find,error),"invalid Service inverse admitted");};
    bad([&](auto& m){m.put(node+0xb8,uint64_t(92));});bad([&](auto& m){m.put(node+0x1f0,uint64_t(95));});
    bad([&](auto& m){m.put(node+0x1f8,uint64_t(97));});bad([&](auto& m){m.put(node+0xc0,int32_t(3));});
    bad([&](auto& m){m.put(node+0xe0+4,int32_t(26));});bad([&](auto& m){m.put(node+0x109,uint8_t(1));});
    bad([&](auto& m){m.put(node+0x110+0x8000,int32_t(99));});bad([&](auto& m){m.put(node+0x180+0x9000,int32_t(99));});
    bad([&](auto& m){m.put(node+0x180+0x60,int32_t(99));});bad([&](auto& m){m.put(node+0x110+0x68,uint8_t(0));});
    bad([&](auto& m){m.put(node+0x110+0x48,int32_t(100001));});
    memory=fixture();alive=true;f::need(!sh::inverse(memory,f::image,node,saved,find,error),"live Service UID allowed before restoration");
    f::need(sh::restored(memory,f::image,service,building,saved,find,error),error);
    memory.put(service+0x40+4,int32_t(26));f::need(!sh::restored(memory,f::image,service,building,saved,find,error),"changed restored Service price accepted");
    auto outbound=saved;outbound.uid=1;outbound.owner_uid=10;outbound.destination_uid=20;
    auto reciprocal=outbound;reciprocal.uid=2;reciprocal.owner_uid=20;reciprocal.destination_uid=10;
    auto survivor=reciprocal;survivor.uid=3;survivor.type=4;survivor.destination_uid=0;survivor.destination=0;
    sh::World services{{10,{outbound}},{20,{survivor,reciprocal}}};sh::Removal removed_services;
    f::need(sh::removal(services,{10},removed_services,error),error);
    f::need(removed_services.saved.size()==2&&removed_services.related_owners==std::set<uint64_t>{20}&&
        removed_services.remaining.at(20)==std::vector<uint64_t>{3},"reciprocal Service removal/order lost");
    f::need(sh::absent(removed_services,{{20,{survivor}}},error),error);
    f::need(!sh::absent(removed_services,{{20,{reciprocal,survivor}}},error),"removed reciprocal Service survived");
    auto changed_survivor=survivor;changed_survivor.counts[0].words[0]++;
    f::need(!sh::absent(removed_services,{{20,{changed_survivor}}},error),"unrelated Service counter changed");
    f::need(sh::removal(services,{10,20},removed_services,error)&&removed_services.saved.size()==3&&removed_services.remaining.empty(),
        "whole multi-Building Destroy saved reciprocal Service twice");

    using Witness=qh::q::Witness;Witness original;original.manager=0x100;original.completed=2;
    auto add=[&](uintptr_t owner,uint64_t uid,const std::string& key,uintptr_t quest,uintptr_t target,int32_t total){
        auto& value=original.owners[owner];value.pointer=owner;value.uid=uid;value.key=key;
        mt2mp::npc_quest_records::Record record;record.sequence_id=int32_t(value.state.records.size());record.name="Quest";
        record.type=1;record.action=2;record.monsters_to_kill=3;record.destination_key="target:"+std::to_string(target);record.completed.total=total;
        value.state.records.push_back(record);mt2mp::npc_quest_records::Local local;local.quest=quest;local.owner=owner;local.destination=target;
        value.state.local.push_back(local);value.destination_uids.push_back(target);original.quests.push_back(quest);
        original.rows.push_back({key,record});original.links.push_back({target,target+1,target+2,owner,owner+1,owner+2});};
    add(10,110,"owner:a",101,1000,2);add(10,110,"owner:a",102,2000,0);add(20,120,"owner:b",103,2000,5);
    Witness previous;
    f::need(qh::previous_owners(original,original,{},std::set<uintptr_t>{10},previous,error)&&
        !previous.owners.contains(10)&&previous.owners.contains(20),"retired child Quest owner would be reread through stale pointer");
    f::need(qh::previous_owners(previous,original,{{10,30,110,"owner:a"}},{},previous,error)&&
        previous.owners.at(30).uid==110&&previous.owners.at(30).key=="owner:a"&&previous.owners.at(30).state.records.empty(),
        "restored child previous-owner seed did not retain canonical identity without old Quest pointers");
    f::need(!qh::previous_owners(original,original,{{10,30,111,"owner:a"}},{},previous,error),"foreign restored child full UID accepted");
    f::need(!qh::previous_owners(original,original,{{10,20,110,"owner:a"}},{},previous,error),"restored child aliases unrelated Quest owner");
    f::need(!qh::previous_owners(original,original,{{10,30,110,"owner:a"}},{10},previous,error),"same previous owner retired and restored in one read");
    f::need(!qh::previous_owners(original,original,{},std::set<uintptr_t>{99},previous,error),"uncaptured retired child owner discarded");
    auto reduced=original;reduced.quests={103,102};reduced.rows={original.rows[2],original.rows[1]};reduced.links={original.links[2],original.links[1]};
    reduced.completed=1;reduced.owners.at(10).state.records.erase(reduced.owners.at(10).state.records.begin());
    reduced.owners.at(10).state.local.erase(reduced.owners.at(10).state.local.begin());reduced.owners.at(10).destination_uids.erase(reduced.owners.at(10).destination_uids.begin());
    reduced.owners.at(10).state.records[0].sequence_id=0;reduced.rows[1].record.sequence_id=0;
    f::need(qh::removed(original,reduced,{1000},error),error);
    f::need(qh::unchanged_owners(original,reduced,{"owner:a"},{},{},error),error);
    auto unrelated_change=reduced;unrelated_change.owners.at(20).state.records[0].description="unexpected";
    f::need(!qh::unchanged_owners(original,unrelated_change,{"owner:a"},{},{},error),"unrelated Quest runtime record changed under native graph authority");
    unrelated_change=reduced;unrelated_change.owners.at(10).uid++;
    f::need(!qh::unchanged_owners(original,unrelated_change,{"owner:a"},{},{},error),"changed Quest owner full UID drift escaped its original graph");
    auto retired=reduced;retired.owners.erase(10);retired.quests={103};retired.rows={original.rows[2]};retired.links={original.links[2]};retired.completed=1;
    f::need(qh::unchanged_owners(original,retired,{"owner:a"},{"owner:a"},{},error),error);
    f::need(!qh::unchanged_owners(original,reduced,{"owner:a"},{"owner:a"},{},error),"retired child Quest owner remained in actual complete witness");
    auto altered=reduced;altered.completed=2;f::need(!qh::removed(original,altered,{1000},error),"missing completed aggregate decrement accepted");
    altered=reduced;altered.owners.at(20).state.records[0].name="foreign";f::need(!qh::removed(original,altered,{1000},error),"unrelated Quest changed during Undo");
    auto restored=original;restored.completed=1;restored.quests={103,201,202};restored.rows={original.rows[2],original.rows[0],original.rows[1]};
    restored.links={original.links[2],original.links[0],original.links[1]};
    for(size_t i=0;i<2;++i){auto& row=restored.owners.at(10).state.records[i];row.completed.total=0;row.description="Original generated description";
        restored.owners.at(10).state.local[i].quest=201+i;restored.rows[1+i].record=row;}
    f::need(qh::restored(original,reduced,restored,{10},error),error);
    auto reborn=restored;auto reborn_owner=reborn.owners.at(10);reborn.owners.erase(10);reborn_owner.pointer=30;
    for(auto& local:reborn_owner.state.local)local.owner=30;reborn.owners.emplace(30,reborn_owner);
    for(size_t i=1;i<reborn.links.size();++i){reborn.links[i][3]=30;reborn.links[i][4]=31;reborn.links[i][5]=32;}
    f::need(qh::unchanged_owners(retired,reborn,{"owner:a"},{},{"owner:a"},error),error);
    f::need(!qh::unchanged_owners(retired,reborn,{"owner:a"},{},{},error),"new child Quest owner admitted without its restore receipt");
    unrelated_change=reborn;unrelated_change.owners.at(20).state.local[0].task=mt2mp::npc_quest_records::TaskStatus::Pending;
    f::need(!qh::unchanged_owners(retired,unrelated_change,{"owner:a"},{},{"owner:a"},error),"unrelated native Quest task phase changed");
    altered=restored;altered.owners.at(10).state.records[0].name="wrong";f::need(!qh::restored(original,reduced,altered,{10},error),"authored Quest definition drift accepted");
    altered=restored;std::swap(altered.quests[1],altered.quests[2]);f::need(!qh::restored(original,reduced,altered,{10},error),"original Quest global append order ignored");
    altered=restored;altered.links[0][1]++;f::need(!qh::restored(original,reduced,altered,{10},error),"unrelated Quest proxy replaced");
    f::need(!qh::restored(original,reduced,restored,{10,10},error),"duplicate NPC inverse owner admitted");
    for(auto form:{bh::Form::Art,bh::Form::Functional,bh::Form::Modular}){
        bh::Saved expected;expected.form=form;expected.uid=77;expected.root=f::root;expected.name="Owned Building";expected.state=2;expected.user_set=1;
        expected.fluff_sha="full-localized-fluff";expected.transform[7]=23;
        size_t name_offset=0xc0,fluff_offset=0xe0,state_offset=0x118,flag_offset=0x11c,root_offset=0x128,pose_offset=0x130;
        uintptr_t addresspoint=0x1723270,method=0x548b00;
        if(form==bh::Form::Art)expected.model=0x78900;
        else {expected.type="SmallInn";expected.variant="Wood";expected.bugs_sha="complete-bugs";
            if(form==bh::Form::Functional){addresspoint=0x173c5b0;method=0x548c70;name_offset=0xc8;fluff_offset=0xe8;
                state_offset=0x120;flag_offset=0x124;root_offset=0x168;pose_offset=0x170;expected.parent_uid=88;expected.dungeon=0x98700;}
            else{addresspoint=0x1737870;method=0x5491d0;root_offset=0x198;pose_offset=0x1a0;
                expected.theme="Theme";expected.layout_sha="complete-layout";expected.plan_sha="complete-plan";}}
        auto make=[&](){f::Memory m;m.put(node,f::image+addresspoint);m.put(f::image+addresspoint+0x10,f::image+method);
            m.put(node+0xb8,expected.uid);m.put(node+state_offset,expected.state);m.put(node+flag_offset,expected.user_set);
            m.put(node+root_offset,expected.root);m.put(node+pose_offset,expected.transform);
            if(form==bh::Form::Art)m.put(node+0x120,expected.model);
            else if(form==bh::Form::Functional){m.put(node+0xc0,expected.parent_uid);m.put(node+0x1e0,expected.dungeon);m.put(node+0x1e8,uintptr_t(0x99900));}
            else {m.put(node+0x180,uintptr_t(0x77700));m.put(node+0x188,uintptr_t(0x88800));m.put(node+0x190,uintptr_t(0x99900));}return m;};
        auto m=make();bool bad_text=false,bad_record=false;
        auto text=[&](uintptr_t pointer,std::string& value,std::string&){
            if(pointer==node+name_offset)value=expected.name;
            else if(pointer==node+(form==bh::Form::Functional?0x128:0x120))value=expected.type;
            else if(pointer==node+(form==bh::Form::Functional?0x148:0x140))value=expected.variant;
            else value=expected.theme;if(bad_text)value+=" changed";return true;};
        auto loc=[&](uintptr_t pointer,std::string& value,std::string&){value=pointer==node+fluff_offset?expected.fluff_sha:"wrong offset";return true;};
        auto record=[&](uintptr_t pointer,bool serialized,std::string& value,std::string&){
            value=pointer==0x77700&&!serialized?expected.layout_sha:pointer==0x88800&&!serialized?expected.plan_sha:
                pointer==0x99900&&serialized?expected.bugs_sha:"wrong record owner";if(bad_record)value+=" changed";return true;};
        auto absent=[](uint64_t,uintptr_t& value,std::string&){value=0;return true;};
        auto verify=[&](){return bh::inverse(m,f::image,node,expected,text,loc,record,absent,error);};
        f::need(verify(),error);m.put(node+root_offset,f::root+1);f::need(!verify(),"foreign Make Building root accepted");
        m=make();m.put(node+pose_offset,uint8_t(1));f::need(!verify(),"changed stored complete Building transform accepted");
        m=make();m.put(node+state_offset,int32_t(4));f::need(!verify(),"changed stored Building state accepted");
        m=make();bad_text=true;f::need(!verify(),"changed stored Building name/type/variant accepted");bad_text=false;
        if(form!=bh::Form::Art){m=make();bad_record=true;f::need(!verify(),"changed owned Building record accepted");}
        auto current=[&](){f::Memory live;live.put(building,f::image+0x16d93c0);live.put(building+0x18,expected.uid);
            live.put(building+0xcba,uint8_t(form==bh::Form::Art));live.put(building+0x7d0,uintptr_t(form==bh::Form::Modular?0x77700:0));
            live.put(building+0x170,expected.root);live.put(building+0x100,expected.transform);live.put(building+0x7a8,expected.state);
            live.put(building+0xbd0,expected.user_set);
            if(form!=bh::Form::Art){live.put(building+0xbd8,uintptr_t(0x99900));live.put(building+0x7d8,uintptr_t(0x88800));
                live.put(building+0xa30,uintptr_t(form==bh::Form::Functional?destination:0));live.put(building+0xa38,expected.dungeon);
                live.put(destination+0x18,expected.parent_uid);}
            return live;};
        auto live=current();bh::Saved result;bool fail_owner=false;
        auto current_text=[&](uintptr_t p,std::string& value,std::string&){
            value=p==building+0x38?expected.name:p==building+0x768?expected.type:p==building+0x788?expected.variant:"foreign";return true;};
        auto current_fluff=[&](uintptr_t p,std::string& value,std::string&){value=p==building?expected.fluff_sha:"foreign";return true;};
        auto current_record=[&](uintptr_t p,mt2mp::game::duplicate_building_values::OwnedKind kind,std::string& value,std::string&){
            using Kind=mt2mp::game::duplicate_building_values::OwnedKind;
            value=p==0x77700&&kind==Kind::Layout?expected.layout_sha:p==0x88800&&kind==Kind::Plan?expected.plan_sha:
                p==0x99900&&kind==Kind::Bugs?expected.bugs_sha:"foreign";return true;};
        auto current_theme=[&](uintptr_t,std::string& value,std::string&){value=expected.theme;return true;};
        auto current_model=[&](uintptr_t,uintptr_t& value,std::string&){value=expected.model;return true;};
        auto current_find=[&](uint64_t uid,uintptr_t& value,std::string&){value=uid==expected.uid?building:
            uid==expected.parent_uid&&!fail_owner?destination:0;return true;};
        auto read_current=[&](){return bh::live(live,f::image,building,current_text,current_fluff,current_record,current_theme,current_model,current_find,result,error);};
        f::need(read_current()&&result==expected,"complete live Building value differs from its original Make payload");
        live=current();live.put(building+0x18,expected.uid+1);f::need(!read_current(),"foreign current Building full UID accepted");
        live=current();live.put(building+0xcba,uint8_t(2));f::need(!read_current(),"invalid current Art flag accepted");
        live=current();live.put(building+0xbd0,uint8_t(2));f::need(!read_current(),"invalid current Building user-state flag accepted");
        if(form==bh::Form::Modular){live=current();live.put(building+0x7d8,uintptr_t(0));f::need(!read_current(),"missing live Modular Plan accepted");}
        if(form==bh::Form::Functional){live=current();fail_owner=true;f::need(!read_current(),"stale current Building parent full UID accepted");}
    }
    std::cout<<f::checks<<" actual Building Service inverse/complete Quest history cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
