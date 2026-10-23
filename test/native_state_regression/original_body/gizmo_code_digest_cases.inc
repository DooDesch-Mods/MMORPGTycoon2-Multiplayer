#include "npc_quest_list_ui_codes.h"
namespace gizmo_code_digest_fixture {
namespace npc_quest_list_ui_codes=mt2mp::game::npc_quest_list_ui_codes;
void entry(){}
void* original=nullptr;uint64_t generation=0;
npc_quest_list_ui_codes::Code ui_code{};
namespace dungeon_gizmo_codes {struct Code {uintptr_t rva;size_t size;const char* sha;};std::array<Code,8> helpers;}
namespace dungeon_gizmo_live_codes {std::array<Pin,29> values;}
#include "gizmo_code_actual.inc"
#include "gizmo_code_baseline.inc"
void run(){
    using namespace fixture;
    namespace timing=live_host_state::apply_timing;
    query_page=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    require(query_page!=nullptr,"allocate Gizmo code permission fixture");
    struct PageCleanup {~PageCleanup(){if(query_page&&!VirtualFree(query_page,0,MEM_RELEASE))std::cerr<<"error: Gizmo code page cleanup failed "<<GetLastError()<<'\n';query_page=nullptr;}} cleanup;
    timing::Scope take(timing::Part::Take);
    constexpr uintptr_t ui=0x6f4930,helper=0x987654,live=0x3c4c10;
    route(ui,96,"mp.dungeon-gizmo-ui.6f4930");
    // The catalog name is taken from the actual compiled Backend catalog.
    auto found=std::find_if(std::begin(hooks::framework::catalog),std::end(hooks::framework::catalog),[](const auto& item){return item.rva==ui;});
    require(found!=std::end(hooks::framework::catalog),"Gizmo UI fixture actual catalog entry");
    auto ui_service=services.extract("mp.dungeon-gizmo-ui.6f4930");ui_service.key()=found->name;services.insert(std::move(ui_service));
    const auto ui_sha=mt2mp::sha256(memory.at(image+ui)),helper_sha=mt2mp::sha256(memory.at(image+helper)),live_sha=mt2mp::sha256(service(live).pristine);
    ui_code={ui,96,ui_sha.c_str(),{}};std::copy_n(memory.at(image+ui).begin(),32,ui_code.prefix.begin());
    for(auto& pin:dungeon_gizmo_codes::helpers)pin={helper,64,helper_sha.c_str()};
    for(auto& pin:dungeon_gizmo_live_codes::values)pin={live,699,live_sha.c_str()};
    struct Result {bool okay;std::string error;unsigned routes,verifications,hashes;std::vector<std::pair<uintptr_t,size_t>> copies;uint64_t queries;};
    const auto invoke=[&](bool baseline){Memory bytes;std::string error="earlier error";reads=verifies=mt2mp::body_hash_calls=0;byte_reads.clear();
        const auto queries=timing::totals[size_t(timing::Part::MemoryVirtualQuery)].count;const auto* previous=timing::page_read_scope;
        const bool okay=baseline?code_baseline(bytes,error):code(bytes,error);
        require(timing::page_read_scope==previous,"Gizmo code permission scope closes on every result");
        return Result{okay,error,reads,verifies,mt2mp::body_hash_calls,byte_reads,timing::totals[size_t(timing::Part::MemoryVirtualQuery)].count-queries};};
    const auto compare=[&](const char* why){const auto before=invoke(true),after=invoke(false);
        require(before.okay==after.okay&&before.error==after.error,why);
        require(before.routes==after.routes&&before.verifications==after.verifications&&before.copies==after.copies,"Gizmo digest preserves every fresh byte read and actual physical-route validation");
        if(before.okay)require(before.queries==before.copies.size()&&after.queries==1,"Gizmo code shares permission metadata while retaining every byte and route check");return after;};
    auto current=compare("Gizmo cold code proof equals original");require(current.okay&&current.hashes==67,"Gizmo cold checks all38 original bodies including29 patched normalizations");
    current=compare("Gizmo warm code proof equals original");require(current.okay&&current.hashes==0&&current.routes==58&&current.verifications==29,"Gizmo warm skips hashes while retaining all hooked checks");
    auto& target=service(live);const auto saved=target.receipt;
    for(int scenario=0;scenario<11;++scenario){
        if(scenario==0)available=false;if(scenario==1)physical=false;if(scenario==2)verification=false;
        if(scenario==3)strcpy_s(target.receipt.owner,"foreign");if(scenario==4)++target.receipt.generation;
        if(scenario==5)++target.receipt.detour;if(scenario==6)++target.receipt.trampoline;
        if(scenario==7)target.receipt.current[12]^=1;if(scenario==8)memory[image+live].back()^=1;
        if(scenario==9)memory[image+helper].back()^=1;if(scenario==10)denied_read=image+helper;
        require(!compare("Gizmo warm memo keeps original byte/owner/failure cause").okay,"Gizmo changed evidence fails");
        available=physical=verification=true;target.receipt=saved;denied_read=0;
        if(scenario==8)memory[image+live].back()^=1;if(scenario==9)memory[image+helper].back()^=1;
        require(compare("Gizmo restored bytes recover without stale rejection").okay,"Gizmo restored proof succeeds");
    }
    auto& backend=hooks::framework::process_backend();
    require(backend.create(reinterpret_cast<void*>(image+ui),reinterpret_cast<void*>(&entry),&original)==MH_OK&&backend.enable(reinterpret_cast<void*>(image+ui))==MH_OK,"Gizmo installs actual owned UI fixture route");
    auto& ui_route=service(ui);generation=ui_route.receipt.generation;
    auto& trampoline=memory[uintptr_t(original)];trampoline.resize(19);std::copy_n(ui_code.prefix.begin(),5,trampoline.begin());
    constexpr std::array<unsigned char,6> continuation{0xff,0x25,0,0,0,0};std::copy(continuation.begin(),continuation.end(),trampoline.begin()+5);
    const uintptr_t resume=image+ui+5;std::memcpy(trampoline.data()+11,&resume,8);
    require(compare("Gizmo later UI hook keeps original owned physical proof").okay,"Gizmo hooked UI succeeds");
    current=compare("Gizmo warmed UI retains receipts and trampoline reads");require(current.okay&&current.hashes==0&&current.routes==60&&current.verifications==30,"Gizmo UI warm bypasses no route checks");
    trampoline[18]^=1;require(!compare("Gizmo warm proof rejects changed trampoline").okay,"Gizmo trampoline failure retained");trampoline[18]^=1;
    const auto ui_saved=ui_route.receipt;ui_route.receipt.detour++;
    require(!compare("Gizmo warm proof rejects changed UI owner").okay,"Gizmo UI owner failure retained");ui_route.receipt=ui_saved;
    require(compare("Gizmo complete hook proof recovers").okay,"Gizmo final actual code proof");
    {timing::PageReadScope reader;const auto first=invoke(false),second=invoke(false);require(first.okay&&second.okay&&first.queries==1&&second.queries==0,"Gizmo code borrows only an active pure reader");}
    {timing::PageReadScope original_call(timing::PageBoundary::OriginalCall);const auto first=invoke(false),second=invoke(false);require(first.okay&&second.okay&&first.queries==1&&second.queries==1,"Gizmo code isolates each verification inside an original-call body");}
    DWORD previous=0;require(VirtualProtect(query_page,4096,PAGE_NOACCESS,&previous)!=0,"protect Gizmo code permission fixture");
    const auto denied=invoke(false);require(!denied.okay&&denied.error=="fixture unreadable memory"&&denied.queries==1,"next Gizmo code proof observes revoked page access");
    require(VirtualProtect(query_page,4096,previous,&previous)!=0,"restore Gizmo code permission fixture");require(compare("Gizmo proof recovers after page restoration").okay,"Gizmo restored page access revalidated");
    std::cout<<"Gizmo38-pin hooked fixture fresh copies=39 route reads=60 verifications=30 permission queries=39->1\n";
    hooks::framework::CodeBodyDigest memo;std::string digest,error;unsigned calls=0;bool succeed=false;
    const auto hash=[&](std::string& value,std::string& cause){++calls;if(!succeed){cause="original digest failure";return false;}value="computed digest";return true;};
    std::array<unsigned char,2> bytes{1,2};
    require(!memo.read(1,bytes.data(),bytes.size(),"expected",digest,error,hash)&&error=="original digest failure","digest failure is not cached");
    succeed=true;require(memo.read(1,bytes.data(),bytes.size(),"expected",digest,error,hash)&&calls==2,"digest retries actual computation after prior failure");
    require(memo.read(1,bytes.data(),bytes.size(),"expected",digest,error,hash)&&calls==2,"digest exact input hits");
    require(memo.read(2,bytes.data(),bytes.size(),"expected",digest,error,hash)&&calls==3,"digest entry change recomputes");
    require(memo.read(2,bytes.data(),bytes.size(),"other",digest,error,hash)&&calls==4,"digest expected hash change recomputes");
    require(memo.read(2,bytes.data(),1,"other",digest,error,hash)&&calls==5,"digest span change recomputes");
}
}
