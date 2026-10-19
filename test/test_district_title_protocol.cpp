#define main district_text_protocol_main
#include "test_district_protocol.cpp"
#undef main
int main()try{
    unsigned checks=0;auto m=sample();m.request.action=Action::MoveDistrictTitle;m.request.district_text={};m.request.district_title.position={0x3f800000,0x40000000,0x40400000};
    m.evidence.district_after=m.evidence.district_before;
    for(size_t i=0;i<3;++i)m.evidence.district_after->values[30+i]=m.request.district_title.position[i];
    m.evidence.district_after->flags[11]=1;
    std::string e;std::vector<uint8_t> bytes;Message out;
    need(encode(m,bytes,e)&&decode(bytes.data(),bytes.size(),out,e)&&out==m,e);++checks;
    need(district_title_wire_fits(m.request,*m.evidence.district_before,m.evidence.before,e),e);++checks;
    auto reject=[&](auto edit){auto bad=m;edit(bad);need(!encode(bad,bytes,e),"invalid title effect accepted");++checks;};
    reject([](auto& v){v.request.district_title.position[0]=0x7f800000;});
    reject([](auto& v){v.request.district_text.text="foreign";});
    reject([](auto& v){v.evidence.district_after->values[29]++;});
    reject([](auto& v){v.evidence.district_after->values[30]++;});
    reject([](auto& v){v.evidence.district_after->flags[11]=0;});
    reject([](auto& v){v.evidence.district_after->name="Other";});
    reject([](auto& v){v.evidence.district_after->manual_title_inside=false;});
    reject([](auto& v){v.evidence.district_after->shape.push_back({{1,2},{3,4}});});
    reject([](auto& v){v.evidence.after.line_counts[0]++;});
    reject([](auto& v){v.evidence.undo_confirmed=true;});
    need(encode(m,bytes,e),e);auto j=nlohmann::json::parse(bytes);
    auto malformed=[&](auto edit){auto v=j;edit(v);const auto s=v.dump();need(!decode(reinterpret_cast<const uint8_t*>(s.data()),s.size(),out,e),"malformed title wire accepted");++checks;};
    malformed([](auto& v){v["request"]["district_title"].push_back(4);});
    malformed([](auto& v){v["request"]["district_title"][0]=1.0;});
    malformed([](auto& v){v["request"]["district_title"][0]=-1;});
    malformed([](auto& v){v["schema"]=34;});
    auto large=*m.evidence.district_before;large.shape={TerrainPath(10000,{999999999999,999999999999})};
    need(!district_title_wire_fits(m.request,large,m.evidence.before,e),"large title result admitted");++checks;
    std::cout<<checks<<" District title strict codec/effect/budget cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
