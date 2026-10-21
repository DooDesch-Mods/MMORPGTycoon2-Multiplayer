#define main group_reader_fixture_main
#include "test_group_reader.cpp"
#undef main
#include "group_history_reader.h"
namespace gh=mt2mp::game::group_history;
int main()try{
    constexpr uintptr_t graph=0x90000,node=0xa0000;std::string e;bool present=false;std::string saved_name="Group";
    const std::string member_key="action:member",free_key="action:free",group_key="group:"+member_key;
    f::gr::Local current;current.handles={{member_key,{f::scenery,11}},{free_key,{f::free_prop,22}}};
    const mt2mp::action_session::GroupRow known_row{group_key,67,f::pose(),{{member_key,f::pose()},{free_key,f::pose()}}};
    std::map<uint64_t,gh::Known> known{{f::group_uid,{f::group_uid,known_row,"Backing property","Group"}}};
    auto find=[&](uint64_t uid,uintptr_t& out,std::string&){out=uid==11?f::scenery:uid==22?f::free_prop:(uid==f::group_uid&&present?f::group:0);return true;};
    auto region=[](int32_t id,uintptr_t& out,std::string&){out=id==67?f::region:0;return out!=0;};
    auto text=[&](uintptr_t p,std::string& out,std::string& why){if(p!=node+0xd0){why="foreign saved string receiver";return false;}out=saved_name;return true;};
    auto fixture=[&](bool add){auto m=f::setup();present=!add;saved_name="Group";current.state.groups.clear();current.handles.erase(group_key);
        if(present){current.state.groups.push_back(known_row);current.handles[group_key]={f::group,f::group_uid};}
        f::array(m,graph+0x18,0x16e2dc0,{node});m.put(node,f::image+(add?0x16ff380:0x1723370));m.put(node+0xb8,f::group_uid);
        m.put(f::image+0x16ff380+0x10,f::image+0x546700);m.put(f::image+0x1723370+0x10,f::image+0x545830);
        const size_t offset=add?0x160:0xc0;f::array(m,node+offset,0x1749370,{1,2});m.put(node+offset+0x2000,int32_t(11));m.put(node+offset+0x2004,int32_t(22));
        if(add){m.put(node+0xc0,f::root);m.put(node+0xc8,f::region);m.put(node+0xf0,f::pose());}return m;};
    std::vector<gh::LocalNode> out;
    for(bool add:{false,true}){auto m=fixture(add);
        auto read=[&](){return gh::read(m,f::image,f::root,graph,current,known,find,region,text,out,e);};
        f::need(read(),e);f::need(out.size()==1&&out[0].group_uid==f::group_uid&&out[0].value.members==std::vector<std::string>{member_key,free_key},"actual complete UID/order lost");
        auto bad=[&](auto change){m=fixture(add);change(m);f::need(!read(),"foreign/partial original history node admitted");};
        bad([&](auto& v){v.put(node+0xb8,uint64_t(7));});
        bad([&](auto& v){v.put(node,f::image+1);});
        bad([&](auto& v){v.put(f::image+(add?0x16ff380:0x1723370)+0x10,f::image+1);});
        const size_t offset=add?0x160:0xc0;
        bad([&](auto& v){v.put(node+offset+0x2000,int32_t(-1));});
        bad([&](auto& v){v.put(node+offset+0x2004,int32_t(11));});
        bad([&](auto& v){v.put(node+offset+16,int32_t(0));});
        bad([&](auto& v){f::array(v,graph+0x18,0x16e2dc0,{node,node});});
        m=fixture(add);current.handles[member_key].uid=0x10000000b;f::need(!read(),"original int32 member silently aliased complete UID");current.handles[member_key].uid=11;
        if(add){bad([&](auto& v){v.put(node+0xc0,f::root+1);});bad([&](auto& v){v.put(node+0xc8,f::region+1);});
            bad([&](auto& v){v.put(node+0xf0,uint32_t(0x7fc00000));});m=fixture(add);saved_name="Other";f::need(!read(),"saved original Group name drift accepted");}
        else{m=fixture(add);present=false;f::need(!read(),"original Remove missing Group admitted");}
    }
    {
        f::Memory names;const std::array<uint8_t,8> getter{0x48,0x8d,0x05,0x79,0xbf,0x56,0x01,0xc3};
        names.put(f::image+0x1749de0+0xa8,f::image+0x4cc4e0);names.put(f::image+0x4cc4e0,getter);
        unsigned reads=0;bool fail_text=false;std::string actual_name;
        auto native_text=[&](uintptr_t address,std::string& value,std::string& why){++reads;
            if(address!=f::image+0x1a38460||fail_text){why="native name read failure";return false;}value="Native display label";return true;};
        f::need(gh::restore_name(names,f::image,native_text,actual_name,e)&&actual_name=="Native display label"&&reads==1,e);
        names.put(f::image+0x1749de0+0xa8,f::image+0x4cc4e1);
        f::need(!gh::restore_name(names,f::image,native_text,actual_name,e)&&reads==1,"foreign GetName vslot read native name");
        names.put(f::image+0x1749de0+0xa8,f::image+0x4cc4e0);names.put(f::image+0x4cc4e0,uint8_t(0));
        f::need(!gh::restore_name(names,f::image,native_text,actual_name,e)&&reads==1,"changed GetName body read native name");
        names.put(f::image+0x4cc4e0,getter);fail_text=true;
        f::need(!gh::restore_name(names,f::image,native_text,actual_name,e)&&e=="native name read failure"&&reads==2,"native name failure hidden");
    }
    f::Memory codes;const std::array<uint8_t,16> first{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x58};
    for(auto [rva,stack]:std::vector<std::pair<uintptr_t,uint8_t>>{{0x546700,uint8_t(0x58)},{0x545830,uint8_t(0xb8)},{0x4ce730,uint8_t(0x48)},{0x4cd3f0,uint8_t(0x98)}}){auto prefix=first;prefix[13]=(rva==0x545830||rva==0x4cd3f0)?0x81:0x83;prefix[15]=stack;codes.put(f::image+rva,prefix);}
    f::need(gh::code(codes,f::image,e),e);codes.put(f::image+0x546700,uint8_t(0));f::need(!gh::code(codes,f::image,e),"changed original Do body admitted");
    std::cout<<f::checks<<" actual Group history graph/full UID/member/name/owner/vslot/body cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
