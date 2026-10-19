#include "mt2mp/net/snapshot_buffer.h"
#include <fstream>
#include <iostream>
using namespace mt2mp::net;
unsigned checks=0;
void need(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
struct Temporary {
    std::filesystem::path path;
    ~Temporary(){if(!path.empty()&&!DeleteFileW(path.c_str()))std::cerr<<"error: test snapshot cleanup Win32="<<GetLastError()<<'\n';}
};
int main()try{
    std::array<wchar_t,32768> folder{},file{};
    need(GetTempPathW(DWORD(folder.size()),folder.data())!=0,"temporary directory unavailable");
    need(GetTempFileNameW(folder.data(),L"mps",0,file.data())!=0,"owned temporary fixture creation failed");
    Temporary fixture{file.data()};
    {std::ofstream stream(fixture.path,std::ios::binary|std::ios::trunc);stream<<"shared snapshot fixture";stream.flush();need(bool(stream),"owned snapshot fixture write failed");}
    std::string hash,error;uint64_t bytes=0;
    need(hash_snapshot_file(fixture.path,hash,bytes,error),error);
    SnapshotFileSender sender;
    need(!sender.release_unsent(error),"inactive source was reported released");
    need(sender.begin(fixture.path,hash,error),error);
    const auto locked=CreateFileW(fixture.path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(locked!=INVALID_HANDLE_VALUE){
        const bool closed=CloseHandle(locked)!=0;
        throw std::runtime_error("sender did not pin source before validation release; unexpected handle cleanup="+std::string(closed?"closed":"failed"));
    }
    need(GetLastError()==ERROR_SHARING_VIOLATION,"source pin failed for a different cause");
    need(sender.release_unsent(error)&&!sender.active()&&sender.size()==bytes&&sender.next_index()==0,
        "Host-only source release fabricated transmitted bytes or lost validated size");
    const auto unlocked=CreateFileW(fixture.path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    need(unlocked!=INVALID_HANDLE_VALUE,"Host-only source handle remained locked");
    need(CloseHandle(unlocked)!=0,"readback file handle close failed");
    need(!sender.release_unsent(error),"double source release reported fresh completion");
    need(sender.begin(fixture.path,hash,error),"released sender could not bind a new actual source");
    std::vector<uint8_t> chunk;
    need(sender.next(chunk,error)&&chunk.size()==bytes,"normal source send path changed");
    need(!sender.release_unsent(error)&&sender.active(),"partly transmitted source was silently cancelled");
    need(sender.finish(error)&&!sender.active(),"normal complete send no longer finished");
    need(DeleteFileW(fixture.path.c_str())!=0,"owned snapshot fixture cleanup failed");fixture.path.clear();
    std::cout<<checks<<" actual validated snapshot release/normal-send checks passed\n";return 0;
}catch(const std::exception& cause){std::cerr<<cause.what()<<'\n';return 1;}
