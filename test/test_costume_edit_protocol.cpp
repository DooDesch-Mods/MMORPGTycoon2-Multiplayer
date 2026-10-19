#include "costume_edit_fixture.h"
#include "costume_native_view.h"
#include "costume_record_reader.h"
#include <iostream>
#include <limits>
int main()try{size_t checks=0;std::string error;std::vector<uint8_t> bytes;Message decoded;
    const auto check=[&](bool ok,const char* cause){++checks;if(!ok)throw std::runtime_error(std::string(cause)+" at comparison "+std::to_string(checks));};
    for(int32_t kind=0;kind<5;++kind)for(bool edited:{false,true}){character_kind=kind;edited_before=edited;
        for(auto message:{request(),result()}){check(encode(message,bytes,error),error.c_str());const bool read_ok=decode(bytes.data(),bytes.size(),decoded,error);if(!read_ok)throw std::runtime_error(error);check(decoded==message,"Costume complete authored record schema35 roundtrip differs");}
        const auto invalid=[&](auto mutate){auto message=result();mutate(message);check(!encode(message,bytes,error),"Costume accepted foreign record/effect/domain");};
        invalid([](auto& m){m.request.costume_edit.kind=5;});invalid([](auto& m){m.request.costume_edit.id=-1;});
        invalid([](auto& m){m.request.canonical_record_key="character:9:1/costume";});invalid([](auto& m){m.request.costume_edit.before_sha256="not a digest";});
        invalid([](auto& m){m.request.costume_edit.record.parts[0].pose.rotation[0]=std::numeric_limits<float>::infinity();});
        invalid([](auto& m){m.request.costume_edit.record.parts[0].attachments[0].pose.position[2]=std::numeric_limits<float>::quiet_NaN();});
        invalid([](auto& m){m.request.costume_edit.record.parts[0].colors[0][0]=std::numeric_limits<float>::infinity();});
        invalid([](auto& m){m.request.costume_edit.record.skeleton=std::string("a\0b",3);});invalid([](auto& m){m.request.costume_edit.record.parts.resize(129);});
        invalid([](auto& m){m.request.costume_edit.record.parts[0].attachments.resize(129);});invalid([](auto& m){m.request.costume_edit.record.parts[0].colors.resize(129);});
        invalid([](auto& m){m.evidence.costume_edit_after.record.parts[0].model="Lost model";});
        invalid([](auto& m){m.evidence.costume_edit_after.record.parts[0].attachments[0].pose.scale[0]+=1;});
        invalid([](auto& m){++m.evidence.costume_edit_after.count;});invalid([](auto& m){++m.evidence.costume_edit_after.owner.undo_count;});
        invalid([](auto& m){m.evidence.costume_edit_after.owner.name="Wrong owner";});invalid([](auto& m){m.evidence.costume_edit_after.owner.speed+=1;});
        invalid([](auto& m){++m.evidence.costume_edit_after.owner.boss_health;});invalid([](auto& m){++m.evidence.costume_edit_after.owner.boss_phases;});
        invalid([](auto& m){++m.evidence.costume_edit_after.owner.release_edits[0];});invalid([](auto& m){++m.evidence.after.cash;});invalid([](auto& m){m.request.ability_edit.id=1;});
        invalid([](auto& m){m.evidence.ability_edit_before.owner.name="foreign";});
        invalid([](auto& m){m.evidence.costume_edit_after.all_sha256=m.evidence.costume_edit_before.all_sha256;});
        auto equal=result();equal.request.costume_edit.record=equal.evidence.costume_edit_before.record;equal.evidence.costume_edit_after.record=equal.request.costume_edit.record;equal.evidence.costume_edit_after.all_sha256=equal.evidence.costume_edit_before.all_sha256;
        check(encode(equal,bytes,error),"Original equal Apply metadata transaction rejected");
        auto message=request();check(encode(message,bytes,error),error.c_str());auto encoded=nlohmann::json::parse(bytes);encoded["request"]["costume_edit"]["record"]["flag"]=1;
        auto malformed=encoded.dump();check(!decode(reinterpret_cast<const uint8_t*>(malformed.data()),malformed.size(),decoded,error),"Costume bool accepted integer");
        encoded=nlohmann::json::parse(bytes);encoded["request"]["costume_edit"]["record"]["parts"][0]["pose"]["rotation"].push_back(0);malformed=encoded.dump();check(!decode(reinterpret_cast<const uint8_t*>(malformed.data()),malformed.size(),decoded,error),"Costume transform accepted foreign dimension");
        const bool fits=costume_edit_wire_fits(request().request,before(),{},error);if(!fits)throw std::runtime_error("Normal Costume receipt preflight: "+error);check(fits,"Normal Costume receipt failed packet preflight");
        auto oversized=request().request;oversized.costume_edit.record.parts.resize(max_payload/1000+2,record().parts[0]);for(auto& p:oversized.costume_edit.record.parts)p.model=std::string(1000,'x');
        check(!costume_edit_wire_fits(oversized,before(),{},error)&&error.find("budget")!=std::string::npos,"Costume oversized receipt allowed before effect or rejected for another cause");
    }
    using namespace mt2mp::game;
    costume_native_view::Borrowed view;const auto original=record();constexpr uintptr_t image=0x140000000;
    auto transform=[](unsigned char* receiver,const CostumePose& pose){std::memcpy(receiver,pose.rotation.data(),16);std::memcpy(receiver+0x10,pose.position.data(),12);std::memcpy(receiver+0x1c,pose.scale.data(),12);receiver[0x28]=1;};view.build(image,original,transform);
    auto read=[](uintptr_t base,size_t offset,auto& out,std::string&){std::memcpy(&out,reinterpret_cast<const void*>(base+offset),sizeof out);return true;};
    auto text=[](uintptr_t base,std::string& out,std::string&){uintptr_t data=0;size_t count=0;std::memcpy(&data,reinterpret_cast<const void*>(base),8);std::memcpy(&count,reinterpret_cast<const void*>(base+8),8);out.assign(reinterpret_cast<const char*>(data),count);return true;};
    CostumeRecord actual;check(costume_record_reader::record(read,text,image,uintptr_t(view.costume.data()),actual,error)&&actual==original,"Costume exact authored ABI reader roundtrip differs");
    check(view.costume[0xb8]==0&&view.costume[0x98]==0&&view.costume[0xb0]==0,"Derived native bone caches leaked into borrowed input");
    view.costume[0xc0]=2;check(!costume_record_reader::record(read,text,image,uintptr_t(view.costume.data()),actual,error),"Native Costume invalid bool accepted");view.costume[0xc0]=1;
    costume_native_view::Borrowed::put(view.parts[0].data(),0x108,int32_t(129));check(!costume_record_reader::record(read,text,image,uintptr_t(view.costume.data()),actual,error),"Native color count outside bound accepted");
    std::cout<<checks<<" Costume authored record/protocol/owner/Undo/ABI checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
