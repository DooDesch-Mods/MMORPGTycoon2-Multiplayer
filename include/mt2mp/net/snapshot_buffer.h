#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#pragma comment(lib,"bcrypt.lib")
namespace mt2mp::net {
constexpr uint32_t snapshot_chunk_bytes=48*1024;
constexpr uint64_t snapshot_max_bytes=uint64_t(1024)*1024*1024;
constexpr uint32_t snapshot_window_chunks=8;
inline bool valid_snapshot_digest(const std::string& digest) {
    return digest.size()==64&&digest.find_first_not_of("0123456789abcdef")==std::string::npos;
}
class SnapshotSha256 {
public:
    SnapshotSha256() {
        try {
            checked(BCryptOpenAlgorithmProvider(&algorithm_,BCRYPT_SHA256_ALGORITHM,nullptr,0),"open");
            DWORD size=0,received=0;
            checked(BCryptGetProperty(algorithm_,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&received,0),"object length");
            object_.resize(size);
            checked(BCryptCreateHash(algorithm_,&hash_,object_.data(),size,nullptr,0,0),"create");
        } catch(...) { release(); throw; }
    }
    ~SnapshotSha256() { release(); }
    SnapshotSha256(const SnapshotSha256&)=delete;
    SnapshotSha256& operator=(const SnapshotSha256&)=delete;
    void add(const uint8_t* data,size_t size) {
        if(finished_||!data||size>(std::numeric_limits<ULONG>::max)()) throw std::runtime_error("SHA256 update invalid");
        checked(BCryptHashData(hash_,const_cast<PUCHAR>(data),ULONG(size),0),"update");
    }
    std::string finish() {
        if(finished_) throw std::runtime_error("SHA256 already finalized");
        std::array<uint8_t,32> bytes{};
        checked(BCryptFinishHash(hash_,bytes.data(),ULONG(bytes.size()),0),"finish");
        finished_=true; std::string digest; constexpr char digits[]="0123456789abcdef";
        for(auto byte:bytes) { digest+=digits[byte>>4]; digest+=digits[byte&15]; }
        return digest;
    }
private:
    static void checked(NTSTATUS code,const char* operation) {
        if(code<0) throw std::runtime_error(std::string("SHA256 ")+operation+" NTSTATUS="+std::to_string(code));
    }
    void release() noexcept {
        if(hash_) { const auto code=BCryptDestroyHash(hash_); if(code<0) std::fprintf(stderr,"error: SHA256 destroy NTSTATUS=%ld\n",long(code)); hash_=nullptr; }
        if(algorithm_) { const auto code=BCryptCloseAlgorithmProvider(algorithm_,0); if(code<0) std::fprintf(stderr,"error: SHA256 close NTSTATUS=%ld\n",long(code)); algorithm_=nullptr; }
    }
    BCRYPT_ALG_HANDLE algorithm_=nullptr;
    BCRYPT_HASH_HANDLE hash_=nullptr;
    std::vector<uint8_t> object_;
    bool finished_=false;
};
class SnapshotFileHandle {
public:
    ~SnapshotFileHandle() { close(); }
    SnapshotFileHandle()=default;
    SnapshotFileHandle(const SnapshotFileHandle&)=delete;
    SnapshotFileHandle& operator=(const SnapshotFileHandle&)=delete;
    HANDLE get() const { return handle_; }
    bool open(const std::filesystem::path& path,DWORD access,DWORD creation,std::string& error) {
        if(handle_!=INVALID_HANDLE_VALUE) { error="snapshot file already open"; return false; }
        // Denying write and delete sharing pins the source or staging identity.
        handle_=CreateFileW(path.c_str(),access,FILE_SHARE_READ,nullptr,creation,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
        if(handle_==INVALID_HANDLE_VALUE) { error="snapshot file open path="+path.string()+" Win32="+std::to_string(GetLastError()); return false; }
        return true;
    }
    bool close(std::string* error=nullptr) {
        if(handle_==INVALID_HANDLE_VALUE) return true;
        const auto file=handle_; handle_=INVALID_HANDLE_VALUE;
        if(CloseHandle(file)) return true;
        const auto code=GetLastError();
        if(error) *error="snapshot file close Win32="+std::to_string(code);
        else std::fprintf(stderr,"error: snapshot file close Win32=%lu\n",code);
        return false;
    }
private:
    HANDLE handle_=INVALID_HANDLE_VALUE;
};
inline bool snapshot_file_length(HANDLE file,uint64_t& length,std::string& error) {
    LARGE_INTEGER size{};
    if(!GetFileSizeEx(file,&size)) { error="snapshot file size Win32="+std::to_string(GetLastError()); return false; }
    if(size.QuadPart<=0||uint64_t(size.QuadPart)>snapshot_max_bytes) {
        error="snapshot file size actual="+std::to_string(size.QuadPart)+" required=1.."+std::to_string(snapshot_max_bytes); return false;
    }
    length=uint64_t(size.QuadPart); return true;
}
inline bool hash_snapshot_file(const std::filesystem::path& path,std::string& digest,uint64_t& length,std::string& error) {
    try {
        SnapshotFileHandle file;
        if(!file.open(path,GENERIC_READ,OPEN_EXISTING,error)||!snapshot_file_length(file.get(),length,error)) return false;
        SnapshotSha256 hash; std::array<uint8_t,snapshot_chunk_bytes> chunk{}; uint64_t total=0;
        for(;;) {
            DWORD read=0;
            if(!ReadFile(file.get(),chunk.data(),DWORD(chunk.size()),&read,nullptr)) { error="snapshot hash read offset="+std::to_string(total)+" Win32="+std::to_string(GetLastError()); return false; }
            if(!read) break;
            if(total+read>length) { error="snapshot hash file grew during read"; return false; }
            hash.add(chunk.data(),read); total+=read;
        }
        if(total!=length) { error="snapshot hash incomplete actual="+std::to_string(total)+" required="+std::to_string(length); return false; }
        digest=hash.finish(); return file.close(&error);
    } catch(const std::exception& failure) { error=failure.what(); return false; }
}
class SnapshotFileSender {
public:
    bool begin(const std::filesystem::path& path,const std::string& digest,std::string& error) {
        if(active_) { error="snapshot sender already active"; return false; }
        if(!valid_snapshot_digest(digest)) { error="snapshot sender digest invalid"; return false; }
        if(!file_.open(path,GENERIC_READ,OPEN_EXISTING,error)) return false;
        std::string actual; uint64_t length=0;
        if(!hash_snapshot_file(path,actual,length,error)||actual!=digest) {
            if(actual!=digest&&!actual.empty()) error="snapshot sender SHA256 differs";
            file_.close(); return false;
        }
        total_=length; offset_=0; index_=0; active_=true;
        return true;
    }
    bool next(std::vector<uint8_t>& bytes,std::string& error) {
        if(!active_||offset_>=total_) { error="snapshot sender has no next chunk"; return false; }
        const auto expected=DWORD(std::min<uint64_t>(snapshot_chunk_bytes,total_-offset_)); bytes.resize(expected); DWORD read=0;
        if(!ReadFile(file_.get(),bytes.data(),expected,&read,nullptr)) { error="snapshot source read offset="+std::to_string(offset_)+" Win32="+std::to_string(GetLastError()); return false; }
        if(read!=expected) { error="snapshot source EOF offset="+std::to_string(offset_)+" actual="+std::to_string(read)+" required="+std::to_string(expected); return false; }
        offset_+=read; ++index_; return true;
    }
    bool finish(std::string& error) {
        if(!active_||offset_!=total_) { error="snapshot sender incomplete actual="+std::to_string(offset_)+" required="+std::to_string(total_); return false; }
        uint64_t actual=0;
        if(!snapshot_file_length(file_.get(),actual,error)||actual!=total_) { if(actual!=total_&&error.empty()) error="snapshot source size changed"; return false; }
        uint8_t extra=0; DWORD read=0;
        if(!ReadFile(file_.get(),&extra,1,&read,nullptr)) { error="snapshot source EOF check Win32="+std::to_string(GetLastError()); return false; }
        if(read) { error="snapshot source grew beyond manifest"; return false; }
        active_=false; return file_.close(&error);
    }
    void stop() { active_=false; file_.close(); }
    bool active() const { return active_; }
    uint64_t size() const { return total_; }
    uint32_t chunks() const { return uint32_t((total_+snapshot_chunk_bytes-1)/snapshot_chunk_bytes); }
    uint32_t next_index() const { return index_; }
private:
    SnapshotFileHandle file_;
    uint64_t total_=0,offset_=0;
    uint32_t index_=0;
    bool active_=false;
};
class SnapshotBuffer {
public:
    bool begin(uint64_t length,uint32_t chunks,const std::filesystem::path& staging,const std::filesystem::path& target,const std::string& digest,std::string& error) {
        if(active_) { error="snapshot receiver already active"; return false; }
        if(!length||length>snapshot_max_bytes) { error="snapshot byte limit actual="+std::to_string(length)+" required=1.."+std::to_string(snapshot_max_bytes); return false; }
        if(chunks!=(length+snapshot_chunk_bytes-1)/snapshot_chunk_bytes) { error="snapshot chunk count differs from byte length"; return false; }
        if(!valid_snapshot_digest(digest)||staging==target||staging.parent_path()!=target.parent_path()) { error="snapshot digest or staging path invalid"; return false; }
        try {
            if(std::filesystem::exists(target)) { error="snapshot target already exists; refusing overwrite"; return false; }
            hash_=std::make_unique<SnapshotSha256>();
        } catch(const std::exception& failure) { error=failure.what(); return false; }
        if(!file_.open(staging,GENERIC_WRITE,CREATE_NEW,error)) { hash_.reset(); return false; }
        total_=length; chunks_=chunks; count_=0; written_=0; staging_=staging; target_=target; digest_=digest; active_=true; committed_=false;
        return true;
    }
    bool add(uint32_t index,const uint8_t* data,size_t length,std::string& error) {
        if(!active_) { error="snapshot receiver inactive"; return false; }
        if(index!=count_||index>=chunks_) { error="snapshot chunk order actual="+std::to_string(index)+" required="+std::to_string(count_); return reject(); }
        const auto expected=size_t(std::min<uint64_t>(snapshot_chunk_bytes,total_-written_));
        if(length!=expected||!data) { error="snapshot chunk length actual="+std::to_string(length)+" required="+std::to_string(expected); return reject(); }
        DWORD wrote=0;
        if(!WriteFile(file_.get(),data,DWORD(length),&wrote,nullptr)) { error="snapshot staging write offset="+std::to_string(written_)+" Win32="+std::to_string(GetLastError()); return reject(); }
        if(wrote!=length) { error="snapshot staging short write actual="+std::to_string(wrote)+" required="+std::to_string(length); return reject(); }
        try { hash_->add(data,length); }
        catch(const std::exception& failure) { error=failure.what(); return reject(); }
        written_+=length; ++count_; return true;
    }
    bool commit(std::string& error) {
        if(!complete()) { error="snapshot incomplete bytes="+std::to_string(written_)+" required="+std::to_string(total_)+" chunks="+std::to_string(count_)+" required="+std::to_string(chunks_); return reject(); }
        try { if(hash_->finish()!=digest_) { error="snapshot SHA256 mismatch"; return reject(); } }
        catch(const std::exception& failure) { error=failure.what(); return reject(); }
        if(!FlushFileBuffers(file_.get())) { error="snapshot staging flush Win32="+std::to_string(GetLastError()); return reject(); }
        uint64_t actual=0;
        if(!snapshot_file_length(file_.get(),actual,error)||actual!=total_) { if(actual!=total_&&error.empty()) error="snapshot staging size differs"; return reject(); }
        if(!file_.close(&error)) return reject();
        if(!MoveFileExW(staging_.c_str(),target_.c_str(),MOVEFILE_WRITE_THROUGH)) { error="snapshot final commit Win32="+std::to_string(GetLastError()); return reject(); }
        hash_.reset(); active_=false; committed_=true; return true;
    }
    void stop() { active_=false; file_.close(); hash_.reset(); }
    bool complete() const { return active_&&count_==chunks_&&written_==total_; }
    bool committed() const { return committed_; }
    uint64_t size() const { return total_; }
    uint64_t written_bytes() const { return written_; }
    uint32_t received_count() const { return count_; }
    const std::filesystem::path& target() const { return target_; }
private:
    bool reject() { stop(); return false; }
    SnapshotFileHandle file_;
    std::unique_ptr<SnapshotSha256> hash_;
    std::filesystem::path staging_,target_;
    std::string digest_;
    uint64_t total_=0,written_=0;
    uint32_t chunks_=0,count_=0;
    bool active_=false,committed_=false;
};
}
