#pragma once
#include "vendor/nlohmann/json.hpp"
#include <filesystem>
#include <string>

namespace mt2mp::reporting {
using json=nlohmann::json;
std::string random_id();
bool valid_id(const std::string&);
std::string redact(std::string);
void initialize(const std::filesystem::path& data,const json& module_hashes,const json& mods=json::array());
void shutdown();
void observe(int level,const char* source,int line,const std::string& message);
void event(const std::string& code,const json& values=json::object(),const std::string& action_id="");
void bind_session(const std::string& diagnostic_session,unsigned slot);
void end_session();
void frame(double milliseconds);
void open_report();
void send_report(const std::string& comment);
void automatic_reports(bool enabled);
json status();
#ifdef MT2MP_REPORTING_TEST
namespace test {void set_upload(json(*)(const json&));}
#endif
}
