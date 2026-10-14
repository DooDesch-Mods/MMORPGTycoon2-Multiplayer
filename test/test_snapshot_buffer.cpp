#include "mt2mp/net/snapshot_buffer.h"
#include "mt2mp/net/relay_protocol.h"
#include <fstream>
#include <iostream>
#include <cstdlib>
static void check(bool result,const std::string& name) { if(!result) { std::cerr<<"error: "<<name<<'\n'; std::exit(1); } }
int main(int argc,char** argv) {
    using namespace mt2mp::net;
    const auto root=std::filesystem::absolute(argc>1?std::filesystem::path(argv[1]):std::filesystem::path("snapshot_buffer_evidence"))/(std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    std::filesystem::create_directories(root);
    std::string error; size_t checks=0;
    auto require=[&](bool result,const std::string& name) { check(result,name+" cause="+error); ++checks; };
    SnapshotSha256 known; const uint8_t abc[]{'a','b','c'}; known.add(abc,3);
    require(known.finish()=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","known SHA256");
    std::vector<uint8_t> first(snapshot_chunk_bytes,8); const uint8_t end[]{3,4,5};
    SnapshotSha256 hash; hash.add(first.data(),first.size()); hash.add(end,3); const auto digest=hash.finish();
    auto stage=[&](const std::string& name) { return root/(name+".incoming"); };
    auto target=[&](const std::string& name) { return root/(name+".vrz"); };
    auto begin=[&](SnapshotBuffer& buffer,const std::string& name) { return buffer.begin(snapshot_chunk_bytes+3,2,stage(name),target(name),digest,error); };
    SnapshotBuffer guard;
    require(!guard.begin(snapshot_max_bytes+1,1,stage("large"),target("large"),digest,error),"oversized manifest rejected");
    require(!guard.begin(1,2,stage("count"),target("count"),digest,error),"chunk manifest mismatch rejected");
    require(!guard.begin(0,0,stage("zero"),target("zero"),digest,error),"empty manifest rejected");
    SnapshotBuffer good; require(begin(good,"good"),"manifest accepted");
    require(good.add(0,first.data(),first.size(),error),"first sequential chunk");
    require(!good.complete()&&!std::filesystem::exists(target("good")),"partial snapshot not committed");
    require(good.add(1,end,3,error)&&good.complete(),"last sequential chunk");
    require(good.commit(error)&&good.committed(),"verified atomic commit");
    std::string actual; uint64_t size=0;
    require(hash_snapshot_file(target("good"),actual,size,error)&&actual==digest&&size==snapshot_chunk_bytes+3,"exact content and size preserved");
    require(!std::filesystem::exists(stage("good")),"staging renamed at commit");
    SnapshotBuffer order; require(begin(order,"order"),"order fixture begins");
    require(!order.add(1,end,3,error)&&error.find("order")!=std::string::npos,"out of order rejected");
    require(!order.commit(error)&&!std::filesystem::exists(target("order")),"out of order cannot commit");
    SnapshotBuffer partial; require(begin(partial,"partial"),"partial fixture begins");
    require(partial.add(0,first.data(),first.size(),error),"partial first chunk");
    require(!partial.commit(error)&&std::filesystem::exists(stage("partial"))&&!std::filesystem::exists(target("partial")),"incomplete transfer retained but not accepted");
    SnapshotBuffer truncated; require(begin(truncated,"truncated"),"truncated fixture begins");
    require(!truncated.add(0,first.data(),2,error),"truncated chunk rejected");
    SnapshotBuffer replay; require(begin(replay,"replay")&&replay.add(0,first.data(),first.size(),error),"replay fixture begins");
    require(!replay.add(0,first.data(),first.size(),error)&&!replay.complete(),"identical replay rejected");
    SnapshotBuffer corrupted; require(begin(corrupted,"corrupted"),"corruption fixture begins");
    auto changed=first; changed[8]^=1;
    require(corrupted.add(0,changed.data(),changed.size(),error)&&corrupted.add(1,end,3,error),"corrupted complete stream received");
    require(!corrupted.commit(error)&&error=="snapshot SHA256 mismatch"&&!std::filesystem::exists(target("corrupted")),"corrupted stream never accepted");
    SnapshotBuffer occupied; require(!begin(occupied,"good")&&error.find("refusing overwrite")!=std::string::npos,"existing final preserved");
    SnapshotBuffer missing; require(!missing.begin(1,1,root/"absent/file.incoming",root/"absent/file.vrz",digest,error)&&error.find("Win32=")!=std::string::npos,"disk open cause retained");
    SnapshotBuffer raced; require(begin(raced,"raced")&&raced.add(0,first.data(),first.size(),error)&&raced.add(1,end,3,error),"commit race fixture begins");
    { std::ofstream file(target("raced"),std::ios::binary); file<<"original"; file.close(); require(bool(file),"owned target race fixture written"); }
    require(!raced.commit(error)&&error.find("snapshot final commit Win32=")!=std::string::npos,"commit race does not overwrite");
    require(std::filesystem::file_size(target("raced"))==8&&std::filesystem::exists(stage("raced")),"existing target and failed stage retained");
    SnapshotFileSender sender; require(sender.begin(target("good"),digest,error),"file sender pinned");
    HANDLE writer=CreateFileW(target("good").c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(writer==INVALID_HANDLE_VALUE&&GetLastError()==ERROR_SHARING_VIOLATION,"concurrent source mutation denied");
    require(!sender.finish(error),"source partial EOF not complete");
    std::vector<uint8_t> sent; require(sender.next(sent,error)&&sent==first,"source reads bounded first chunk");
    require(sender.next(sent,error)&&sent.size()==3&&sent[2]==5,"source reads exact final tail");
    require(!sender.next(sent,error),"extra chunk beyond EOF rejected");
    require(sender.finish(error),"source complete EOF and close");
    SnapshotFileSender bad_sender; require(!bad_sender.begin(target("good"),std::string(64,'0'),error)&&error.find("SHA256 differs")!=std::string::npos,"outbound checksum mismatch rejected");
    using namespace relay; auto packet=frame(Data,3,first.data(),50); Header header{}; std::vector<uint8_t> payload;
    auto fragments=std::vector<uint8_t>(packet.begin(),packet.begin()+7);
    require(pop(fragments,header,payload)==Parse::Incomplete,"fragmented relay header waits");
    fragments.insert(fragments.end(),packet.begin()+7,packet.end());
    require(pop(fragments,header,payload)==Parse::Complete&&payload.size()==50,"fragmented relay frame completes");
    Header oversized{magic,Data,3,1,max_payload+1}; std::vector<uint8_t> malformed(sizeof(oversized)); memcpy(malformed.data(),&oversized,sizeof(oversized));
    require(pop(malformed,header,payload)==Parse::Invalid,"relay oversized length rejected before allocation");
    std::cout<<"snapshot file guards passed checks="<<checks<<" root="<<root.string()<<"\n";
}
