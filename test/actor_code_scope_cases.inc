namespace actor_code_scope_fixture {
namespace live_host_state=::mt2mp::game::live_host_state;
namespace monster_live_codes {auto values=std::to_array(monster_code_scope_fixture::monster_live_codes::values);}
uintptr_t fixture_image=0,last_rva=0,unreadable=0,changed=0;
size_t hashes=0;bool throws=false;
std::vector<std::pair<uintptr_t,size_t>> reads;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
namespace root_saved_building {
bool bytes(const void* object,size_t offset,void* out,size_t count,std::string& error){
    const auto address=uintptr_t(object)+offset;last_rva=address-fixture_image;reads.emplace_back(last_rva,count);
    if(last_rva==unreadable)return fail(error,"original Actor code read failed");
    if(throws)throw std::runtime_error("original Actor code read exception");
    MEMORY_BASIC_INFORMATION info{};
    if(t::virtual_query(reinterpret_cast<LPCVOID>(address),&info,sizeof info,t::Part::QuerySavedFields)!=sizeof info)
        return fail(error,"Actor fixture code metadata absent");
    std::memset(out,int(last_rva%251)+1,count);return true;
}
}
namespace mt2mp {
std::string sha256(const std::vector<unsigned char>& bytes){
    ++hashes;need(!bytes.empty()&&std::all_of(bytes.begin(),bytes.end(),[](auto value){return value==static_cast<unsigned char>(last_rva%251+1);}),"Actor hash receives all freshly read bytes");
    if(last_rva==changed)return "changed";
    for(const auto& pin:monster_live_codes::values)if(pin.rva==last_rva){need(bytes.size()==pin.size,"Actor original Monster pin length retained");return pin.sha;}
    if(last_rva==0x3c2a00){need(bytes.size()==64,"Actor first NPC pin extent");return "f5920de3004cfb920a54ca852cdffde6904e2122c7db69ba64893d6a35500bda";}
    need(last_rva==0x3c27b0&&bytes.size()==592,"Actor second NPC pin extent");return "062090540eac606dc417a74cf12782ff421d2a041eb62a060daa419d3dd98b6e";
}
}
#include "actor_code_scope_actual.inc"
void run(){
    struct Owned {void* memory=nullptr;~Owned(){if(memory&&!VirtualFree(memory,0,MEM_RELEASE))std::cerr<<"Actor code fixture release failed "<<GetLastError()<<'\n';}} owned;
    owned.memory=VirtualAlloc(nullptr,32*1024*1024,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);need(owned.memory!=nullptr,"Actor code fixture allocation");fixture_image=uintptr_t(owned.memory);
    const auto reset=[](){reads.clear();hashes=0;unreadable=changed=0;throws=false;};
    t::Scope take(t::Part::Take);std::string error;reset();const auto baseline_start=count(t::Part::QuerySavedFields);
    need(prior_code(fixture_image,error)&&reads.size()==24&&hashes==24,"Actor original pin fixture has 22 Monster and two NPC complete checks");
    const auto prior=reads;const auto baseline=count(t::Part::QuerySavedFields)-baseline_start;
    reset();const auto actual_start=count(t::Part::QuerySavedFields);
    need(code(fixture_image,error)&&reads==prior&&hashes==24,"Actor scoped pin check retains every byte extent and hash order");
    const auto actual=count(t::Part::QuerySavedFields)-actual_start;
    need(baseline==24&&actual<=2,"Actor code-only scope reuses permission metadata");
    need(!t::PageReadScope::reader_active(),"Actor code Reader ends before caller resumes native work");
    std::cout<<"Actor code reads="<<reads.size()<<" hashes="<<hashes<<" OS queries="<<baseline<<"->"<<actual<<'\n';
    changed=prior[0].first;need(!code(fixture_image,error)&&error.find("Actor Frame native original leaf body differs RVA=")==0,"Fresh second Actor pin pass rejects changed Monster bytes");
    reset();changed=0x3c27b0;need(!code(fixture_image,error)&&error.find("Actor Frame native NPC presentation/map leaf body differs RVA=")==0&&reads.size()==24,"Actor NPC body hash mismatch remains distinct");
    reset();unreadable=0x3c2a00;need(!code(fixture_image,error)&&error=="original Actor code read failed"&&hashes==22,"Actor NPC read failure preserves original cause and stops before hashing");
    auto pin=std::find_if(monster_live_codes::values.begin(),monster_live_codes::values.end(),[&](const auto& value){return value.rva==prior[0].first;});
    need(pin!=monster_live_codes::values.end(),"Actor selected leaf exists in original pin catalogue");
    const auto saved=*pin;reset();pin->tail="owned-tail";
    need(!code(fixture_image,error)&&error.find("Actor Frame native leaf has no original complete function pin RVA=")==0&&reads.empty(),"Actor rejects tail-only Monster pins before reading code");
    *pin=saved;reset();pin->rva=0;
    need(!code(fixture_image,error)&&error.find("Actor Frame native leaf has no original complete function pin RVA=")==0&&reads.empty(),"Actor rejects missing complete pins");
    *pin=saved;reset();throws=true;bool threw=false;try{code(fixture_image,error);}catch(const std::runtime_error& e){threw=std::string(e.what())=="original Actor code read exception";}
    need(threw&&!t::PageReadScope::reader_active(),"Actor exception propagates and closes Reader scope");
    reset();{t::PageReadScope parent;auto* owner=t::page_read_scope;
        need(code(fixture_image,error)&&t::page_read_scope==owner,"Actor code borrows an existing pure Reader without changing its lifetime");}
    need(!t::PageReadScope::reader_active(),"Actor caller Reader closes at original boundary");
}
}
