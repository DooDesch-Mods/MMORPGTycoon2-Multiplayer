#include "costume_edit_fixture.h"
#include "costume_weapon_read.h"
#include <cstring>
#include <iostream>
#include <memory>
#include <map>
namespace weapon_fixture {
namespace wr=mt2mp::game::costume_weapon_read;
struct Memory {
    std::map<uintptr_t,std::vector<unsigned char>> fields;std::map<uintptr_t,std::string> strings;
    uintptr_t failure=0;bool drift=false;unsigned reads=0;
    template<class T>void put(uintptr_t at,const T& value){const auto* p=reinterpret_cast<const unsigned char*>(&value);fields[at]={p,p+sizeof value};}
    template<class T>bool read(uintptr_t at,size_t offset,T& out,std::string& error){
        if(at+offset==failure){error="exact fixture category read failed";return false;}
        const auto found=fields.find(at+offset);if(found==fields.end()||found->second.size()!=sizeof out){error="fixture category field missing";return false;}
        std::memcpy(&out,found->second.data(),sizeof out);if(at==100&&offset==0xe0&&drift&&++reads>1)++out;return true;
    }
};
}
int main()try{using namespace weapon_fixture;unsigned checks=0;std::string error;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(std::string(why)+" cause="+error);};
    std::vector<std::string> result_categories;
    check(costume_weapon_after({"Sword","Staff","Bow"},{"Staff",false},result_categories)&&result_categories==std::vector<std::string>{"Sword","Bow"},"Native removal swap-last order differs");
    check(costume_weapon_after({"Sword","Staff","Bow"},{"Sword",false},result_categories)&&result_categories==std::vector<std::string>{"Bow","Staff"},"Native first-item removal did not use last member");
    check(costume_weapon_after({"Sword"},{"Staff",true},result_categories)&&result_categories==std::vector<std::string>{"Sword","Staff"},"Native insertion order differs");
    check(!costume_weapon_after({"Sword"},{"Sword",true},result_categories)&&!costume_weapon_after({"Sword"},{"Staff",false},result_categories),"Unchanged native checkbox admitted");
    std::vector<uint8_t> bytes;auto decoded=std::make_unique<Message>();
    for(bool add:{false,true}){auto message=std::make_unique<Message>(request());auto& m=*message;
        m.request.costume_edit.record=record();m.request.costume_edit.weapon_category=CostumeWeaponIntent{add?"Axe":"Sword",add};
        m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
        auto& e=m.evidence;e.costume_edit_before=before();e.costume_edit_before.weapon_categories={"Sword","Staff"};e.costume_edit_after=e.costume_edit_before;
        check(costume_weapon_after(e.costume_edit_before.weapon_categories,*m.request.costume_edit.weapon_category,e.costume_edit_after.weapon_categories),"Fixture checkbox transition failed");
        e.costume_edit_after.all_sha256=std::string(64,'f');e.retirement_confirmed=true;
        check(encode(m,bytes,error)&&decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==m,"Full weapon schema36 request/result failed");
        const auto document=nlohmann::json::parse(bytes);check(document.at("schema")==36,"Weapon did not use schema36");
        for(unsigned change=0;change<10;++change){auto bad=document;
            if(change==0)bad["schema"]=35;if(change==1)bad["schema"]=34;
            if(change==2)bad["request"]["action"]=85;
            if(change==3)bad["request"]["costume_edit"]["weapon_category"]["enabled"]=1;
            if(change==4)bad["request"]["costume_edit"]["weapon_category"]["category"]="";
            if(change==5)bad["evidence"]["costume_edit_after"]["weapon_categories"]=nlohmann::json::array({"Sword","Sword"});
            if(change==6)bad["evidence"]["costume_edit_after"]["record"]["name"]="Unrelated Costume mutation";
            if(change==7)bad["request"]["costume_edit"]["weapon_category"]["foreign"]=true;
            if(change==8)bad["request"]["costume_edit"].erase("weapon_category");
            if(change==9)bad["evidence"]["costume_edit_after"]["owner"]["name"]="Other Character";
            const auto text=bad.dump();check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),*decoded,error),"Malformed or foreign weapon packet admitted");}
        auto changed=std::make_unique<Message>(m);changed->evidence.costume_edit_after.owner.undo_count++;
        check(!encode(*changed,bytes,error),"Weapon invented native Undo history");
        changed->evidence=m.evidence;changed->evidence.after.cash++;
        check(!encode(*changed,bytes,error),"Weapon changed Ledger");
        changed->evidence=m.evidence;changed->evidence.costume_edit_after.owner.release_edits[7]++;
        check(!encode(*changed,bytes,error),"Checkbox prematurely marked original Apply release edit");
        check(costume_edit_wire_fits(m.request,m.evidence.costume_edit_before,{},error),"Weapon complete receipt failed unchanged packet cap");
    }
    {auto legacy=std::make_unique<Message>(result());check(encode(*legacy,bytes,error),"Apply source no longer encodes");auto old=nlohmann::json::parse(bytes);
        old["schema"]=35;old["request"]["costume_edit"].erase("weapon_category");
        old["evidence"]["costume_edit_before"].erase("weapon_categories");old["evidence"]["costume_edit_after"].erase("weapon_categories");
        const auto text=old.dump();check(decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),*decoded,error)&&*decoded==*legacy,"Authentic old Apply schema35 compatibility lost");}
    Memory memory;memory.put(100+0xd8,uintptr_t(1000));memory.put(100+0xe0,int32_t(3));memory.put(100+0xe4,int32_t(4));
    memory.strings={{1000,"Sword"},{1032,"Staff"},{1064,"Bow"}};
    auto read=[&](uintptr_t p,size_t o,auto& out,std::string& e){return memory.read(p,o,out,e);};
    auto text=[&](uintptr_t p,std::string& out,std::string& e){const auto found=memory.strings.find(p);if(found==memory.strings.end()){e="exact fixture native string missing";return false;}out=found->second;return true;};
    check(wr::categories(read,text,100,result_categories,error)&&result_categories==std::vector<std::string>{"Sword","Staff","Bow"},"Actual native category reader lost ordered members");
    memory.failure=100+0xe0;check(!wr::categories(read,text,100,result_categories,error)&&error=="exact fixture category read failed","Native read cause overwritten");memory.failure=0;
    memory.drift=true;check(!wr::categories(read,text,100,result_categories,error)&&error.find("changed during")!=std::string::npos,"Native list drift admitted");memory.drift=false;
    memory.strings[1032]="Sword";check(!wr::categories(read,text,100,result_categories,error),"Duplicate native set member admitted");memory.strings[1032]="Staff";
    memory.put(200+0x38,uintptr_t(2000));memory.put(200+0x40,int32_t(2));memory.put(200+0x44,int32_t(2));
    memory.put(2000,int32_t(2));memory.put(2000+0x30,int32_t(4));memory.put(2000+0x58,int32_t(1));memory.strings[2000+8]="Staff";
    std::string category;bool enabled=false;
    check(wr::command(read,text,200,category,enabled,error)&&category=="Staff"&&enabled,"Original checkbox command token read failed");
    for(int32_t value:{-1,2}){memory.put(2000+0x58,value);check(!wr::command(read,text,200,category,enabled,error),"Non-checkbox integer admitted");}
    memory.put(2000+0x58,int32_t(0));check(wr::command(read,text,200,category,enabled,error)&&!enabled,"Original unchecked checkbox lost");
    memory.put(2000+0x30,int32_t(2));check(!wr::command(read,text,200,category,enabled,error),"String checkbox state admitted");
    std::cout<<checks<<" actual Costume weapon codec/transition/native-reader checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
