#pragma once
#include "../game/client_save_export_protocol.h"
namespace mt2mp::net {
bool configure_save_export(game::client_save_export::Protocol::Ports,std::string&);
bool request_save_export(const std::string& nonce,const std::filesystem::path& local_target,
    const std::filesystem::path& local_staging,uint32_t kind,bool option,std::string&);
bool provide_save_export(const std::string& nonce,const std::filesystem::path& native,std::string&);
bool save_export_publication_confirmed();
bool complete_save_export(std::string&);
void fail_save_export(const std::string&);
nlohmann::json save_export_status();
bool retire_save_export(std::string&);
}
