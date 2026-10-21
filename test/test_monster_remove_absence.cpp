#include "monster_remove_fixture.h"
#include "monster_remove_read.h"
#include "direct_monster_identity.h"
#include <cstring>
#include <iostream>
namespace remove_absence_fixture {
namespace a=mt2mp::action_session;
namespace npc_quest_records=mt2mp::npc_quest_records;
namespace npc_census=mt2mp::npc_census;
namespace rr=mt2mp::game::monster_remove_read;
namespace qr=mt2mp::game::monster_quest_retirement;
namespace identity=mt2mp::game::direct_monster_identity;
namespace mz=mt2mp::game::monster_zone_read;
bool fail(std::string& e,const std::string& reason){e=reason;return false;}
struct Memory:mt2mp::npc_census::Memory {
 int process=0;std::map<uintptr_t,unsigned char> data;unsigned target_reads=0;
 template<class T>void set(uintptr_t p,const T& v){auto b=reinterpret_cast<const unsigned char*>(&v);for(size_t i=0;i<sizeof v;++i)data[p+i]=b[i];}
 void text(uintptr_t p,const std::string& s){for(size_t i=0;i<s.size();++i)data[p+i]=static_cast<unsigned char>(s[i]);}
 bool copy(uintptr_t p,void* out,size_t n,std::string& e)override{
  if(p>=0x70000&&p<0x71000){++target_reads;e="freed target dereferenced";return false;}
  auto b=static_cast<unsigned char*>(out);for(size_t i=0;i<n;++i){auto f=data.find(p+i);if(f==data.end()){e="fixture unreadable";return false;}b[i]=f->second;}return true;}
};
template<class T>bool field(Memory& m,uintptr_t base,size_t off,T& v,std::string& e){return m.copy(base+off,&v,sizeof v,e);}
struct Pin{struct{uintptr_t root=10,selection=40;}world;struct{uintptr_t region=0x80000,root=10;}region;struct{uintptr_t find=1;}api;};
namespace d{struct Sample{mz::State state;struct Undo{std::vector<uintptr_t> undo;bool active_graph=false;}undo;};}
struct RemoveOperation{a::Binding binding;identity::Entry birth;d::Sample before;qr::Witness quests;mt2mp::action_session::Request request;a::Evidence evidence;uint64_t character_id=7;std::map<uint64_t,std::string> names_before;};
struct {uintptr_t image=0x100000000;}state;
uintptr_t lookup_result=0;uint32_t lookup_error=0;bool monster_spatial=false,character_spatial=false,selection=true;unsigned spatial_calls=0;
uint32_t lookup(uintptr_t,uint64_t,uintptr_t& out){out=lookup_result;return lookup_error;}
uint32_t spatial_has(uintptr_t spatial,uintptr_t object,bool& present){need(object==0x70000,"spatial query token differs");++spatial_calls;present=spatial==0x90000?monster_spatial:character_spatial;return 0;}
namespace action_runtime{bool selection_absent(Memory&,uintptr_t,uintptr_t,uintptr_t,std::string& e){return selection||fail(e,"selection retains target");}}
namespace undo_probe {
struct GraphWitness{uintptr_t graph=0;int nodes=0,operations=1,pending_delete=0;bool locked=true;};
bool graph_ok=true,commit_ok=true;GraphWitness value;
bool read_graph(int,uintptr_t,uintptr_t g,GraphWitness& out,std::string& e){if(!graph_ok)return fail(e,"graph unreadable");out=value;out.graph=g;return true;}
bool validate_undo_commit(const d::Sample::Undo&,const d::Sample::Undo&,const GraphWitness&,std::string& e){return commit_ok||fail(e,"Undo commit differs");}
}
bool read_remove_quests(Memory& m,const a::Binding&,const std::string&,uintptr_t object,const std::string& target,qr::Witness& out,std::string& e,const qr::Witness* before=nullptr){
 const auto owner=[](uintptr_t,uint64_t&,std::string&,std::string&){return false;};
 const auto destination=[](uintptr_t,std::string&,std::string&){return false;};
 return qr::read(m,state.image,object,target,owner,destination,out,e,before);
}
bool quest_digest(const qr::Witness&,std::string& out,std::string&){out=std::string(64,'d');return true;}
#include "remove_absent_body.inc"
#include "remove_inverse_body.inc"
void setup(Memory& m,Pin& p,RemoveOperation& op,d::Sample& after){
 lookup_result=0;lookup_error=0;monster_spatial=character_spatial=false;selection=true;spatial_calls=0;
 undo_probe::graph_ok=undo_probe::commit_ok=true;undo_probe::value={};
 op.birth.pointer=0x70000;op.birth.uid=0x100000001ull;op.evidence.monster_before=born(empty());
 op.names_before[op.birth.uid]="Deathblood";
 mz::Monster row;row.pointer=op.birth.pointer;row.uid=op.birth.uid;
 op.before.state.manager=after.state.manager=1;op.before.state.monster_manager=after.state.monster_manager=0x10000;
 op.before.state.monsters={row};after.state.created_monsters=1;after.undo.undo={0x20000};
 m.set(0x10070,uintptr_t{0x11000});m.set(0x10078,int32_t{0});m.set(0x1007c,int32_t{1});m.set(0x10088,uintptr_t{0x90000});
 constexpr uintptr_t qm=0x12000;m.set(state.image+0x13df410,qm);m.set(qm,state.image+0x17020b0);
 m.set(qm+0x48,int32_t{0});m.set(qm+0x30,uintptr_t{0});m.set(qm+0x38,int32_t{0});m.set(qm+0x3c,int32_t{0});
 op.quests.manager=qm;
 const auto& actor=op.evidence.monster_before.actor;constexpr uintptr_t graph=0x20000,data=0x21000,inverse=0x22000,name=0x23000;
 m.set(graph+0x20,data);m.set(data,inverse);m.set(inverse,state.image+0x1706550);m.set(inverse+0xb8,op.birth.uid);
 m.set(inverse+0xc0,name);m.set(inverse+0xc8,actor.name.size());m.text(name,actor.name);
 m.set(inverse+0xe0,p.world.root);m.set(inverse+0xe8,p.region.region);m.set(inverse+0xf0,actor.rank);m.set(inverse+0xf8,uint64_t{0});
 m.set(inverse+0x100,actor.rotation);m.set(inverse+0x110,actor.position);m.set(inverse+0x11c,actor.scale);m.set(inverse+0x16c,actor.level);m.set(inverse+0x170,op.character_id);
}
}
int main(){try{using namespace remove_absence_fixture;std::string e;unsigned n=0;
 for(unsigned mode=0;mode<11;++mode){Memory m;Pin p;RemoveOperation op;d::Sample after;setup(m,p,op,after);
  if(mode==1)lookup_result=op.birth.pointer;if(mode==2)lookup_error=5;if(mode==3)monster_spatial=true;if(mode==4)character_spatial=true;if(mode==5)selection=false;
  if(mode==6){m.set(0x10078,int32_t{1});m.set(0x11000,op.birth.pointer);}if(mode==7)m.set(0x10078,int32_t{-1});
  if(mode==8)after.state.created_monsters=0;if(mode==9)++after.state.next_monster_tag;if(mode==10)op.quests.manager++;
  const auto okay=remove_absent(m,p,after,op,e);need(okay==(mode==0),"actual absence guard missed mode="+std::to_string(mode));need(!m.target_reads,"absence read accessed freed Monster");++n;
 }
 for(unsigned mode=0;mode<14;++mode){Memory m;Pin p;RemoveOperation op;d::Sample after;setup(m,p,op,after);
  if(mode==1)undo_probe::value.pending_delete=1;if(mode==2)undo_probe::value.operations=2;if(mode==3)undo_probe::commit_ok=false;
  if(mode==4)m.set(0x220b8,uint64_t{1});if(mode==5)m.set(0x220f8,uint64_t{5});if(mode==6)m.set(0x220e8,uintptr_t{9});
  if(mode==7)m.set(0x2216c,int32_t{2});if(mode==8)m.set(0x22170,uint64_t{8});if(mode==9)m.set(0x220f0,int32_t{2});
  if(mode==10)m.set(0x22110,std::array<float,3>{0,0,0});if(mode==11)m.text(0x23000,"Wrongblood");
  if(mode==12)after.undo.undo.clear();if(mode==13)undo_probe::graph_ok=false;
  need(remove_inverse(m,p,op,after,e)==(mode==0),"actual inverse guard missed mode="+std::to_string(mode));need(!m.target_reads,"inverse read accessed freed target");++n;
 }
 for(unsigned mode=0;mode<4;++mode){Memory m;Pin p;RemoveOperation op;d::Sample after;setup(m,p,op,after);
  constexpr uintptr_t first=0x30000,second=0x31000,qa=0x40000,qb=0x41000;
  op.quests.quests={1,2,3};op.quests.removed={1,2,3};op.quests.links={{{0,0,0,qa,0,0}},{{0,0,0,qa,0,0}},{{0,0,0,qb,0,0}}};
  for(auto owner:{qa,qb}){qr::Owner value;value.pointer=owner;value.uid=uint64_t(owner)+0x100000000ull;
   mt2mp::npc_quest_records::Record invalid;invalid.action=8;value.state.records={invalid};op.quests.owners.emplace(owner,value);}
  m.set(0x21000,first);m.set(0x21008,second);m.set(0x21010,uintptr_t(0x22000));undo_probe::value.operations=3;
  for(auto inverse:{first,second}){m.set(inverse,state.image+0x1713330);m.set(inverse+0xb8,op.quests.owners.at(inverse==first?qa:qb).uid);
   m.set(inverse+0xc0,uint8_t(0));m.set(inverse+0xd0,uintptr_t(0));m.set(inverse+0xd8,int32_t(0));m.set(inverse+0xdc,int32_t(0));}
  if(mode==1){m.set(0x21000,second);m.set(0x21008,first);}if(mode==2)undo_probe::value.operations=4;
  if(mode==3)m.set(first+0xb8,uint64_t(qa));
  need(remove_inverse(m,p,op,after,e)==(mode==0),"actual deduplicated Quest/Monster graph order or full UID guard missed");++n;
 }
 std::cout<<n<<" actual absence, pending-delete and inverse cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
