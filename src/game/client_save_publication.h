#pragma once
#include "client_save_export.h"

namespace mt2mp::game::client_save_export {
// Keep original bytes in the request's private staging directory. Publish the
// native file last, so a failed group cannot look like a confirmed new save.
class PublicationGroup {
    struct Entry {fs::path target,source,backup,prepared;bool existed=false,published=false;File old_file,new_file;};
    std::array<Entry,3> files_{};
    fs::path target_,staging_;std::string nonce_;Current current_;
    bool retained_=false,attempted_=false,complete_=false;
    private_profile::DirectoryPins directories_;
    static void copy(const fs::path& source,const fs::path& target,const File& expected){
        require(!selected_snapshot::regular(target,false),"publication staging destination already exists");
        require(CopyFileW(source.c_str(),target.c_str(),TRUE)!=0,"publication copy failed Win32="+std::to_string(GetLastError()));
        require(hash(source)==expected&&hash(target)==expected,"publication copy source/readback differs");
    }
public:
    void retain(const fs::path& target,const fs::path& staging,const std::string& nonce,Current owner){
        require(!retained_&&!attempted_,"publication destination already retained");
        require(target.is_absolute()&&staging.is_absolute()&&net::snapshot_world_path::valid_uuid(nonce),"publication local owner invalid");
        current_=std::move(owner);current(current_);target_=target;staging_=staging;nonce_=nonce;
        directories_.verify(target.parent_path());directories_.verify(staging);
        files_[0].target=metadata(target);files_[1].target=sidecar(target);files_[2].target=target;
        for(size_t i=0;i<files_.size();++i){auto& file=files_[i];
            file.existed=selected_snapshot::regular(file.target,false);if(file.existed)file.old_file=hash(file.target);
            file.backup=staging/net::snapshot_world_path::utf8_path("previous-"+nonce+"-"+std::to_string(i));
            file.prepared=target.parent_path()/net::snapshot_world_path::utf8_path(".mt2-export-"+nonce+"-"+std::to_string(i));
            require(!selected_snapshot::regular(file.backup,false)&&!selected_snapshot::regular(file.prepared,false),"publication nonce was already used");
        }
        retained_=true;
    }
    void publish(const Receiver& receiver){
        current(current_);require(retained_&&!attempted_&&!complete_&&receiver.verified()&&receiver.destination()==target_,"publication lacks its exact retained local target");
        const auto offer=parse(receiver.receipt(),Identity{receiver.receipt().at("session"),receiver.receipt().at("world"),
            receiver.receipt().at("requester"),receiver.receipt().at("mask"),nonce_});
        files_[0].source=metadata(receiver.verified_native());files_[0].new_file=offer.metadata;
        files_[1].source=sidecar(receiver.verified_native());files_[1].new_file=offer.companion;
        files_[2].source=receiver.verified_native();files_[2].new_file=offer.native;
        // Consume before the first filesystem mutation. A failure keeps its
        // previous files and exact publication record; it never repeats Save.
        attempted_=true;
        for(auto& file:files_){
            current(current_);require(selected_snapshot::regular(file.target,false)==file.existed,"local save presence changed after original request");
            if(file.existed){require(hash(file.target)==file.old_file,"local save changed after original request");copy(file.target,file.backup,file.old_file);}
            copy(file.source,file.prepared,file.new_file);
        }
        for(auto& file:files_){
            current(current_);require(selected_snapshot::regular(file.target,false)==file.existed,"local save presence changed before publication");
            if(file.existed)require(hash(file.target)==file.old_file,"local save changed before publication");
            require(MoveFileExW(file.prepared.c_str(),file.target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,
                "publication replacement failed Win32="+std::to_string(GetLastError()));
            file.published=true;require(hash(file.target)==file.new_file,"published native save group readback differs");
        }
        for(const auto& file:files_){require(hash(file.target)==file.new_file,"final published save group changed");
            if(file.existed)require(hash(file.backup)==file.old_file,"retained previous save backup changed");}
        companion(sidecar(target_),offer);complete_=true;
    }
    bool complete()const{return complete_;}
    Json receipt()const{
        Json out={{"nonce",nonce_},{"attempted",attempted_},{"complete",complete_},{"files",Json::array()}};
        for(const auto& file:files_)out["files"].push_back({{"target",net::snapshot_world_path::utf8(file.target)},
            {"previous_present",file.existed},{"previous_sha256",file.old_file.sha},{"previous_bytes",file.old_file.bytes},
            {"backup",net::snapshot_world_path::utf8(file.backup)},{"published",file.published},
            {"sha256",file.new_file.sha},{"bytes",file.new_file.bytes}});
        return out;
    }
};
}
