#include "mt2mp/core/reporting.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/private_profile_paths.h"
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
#include <regex>
#include <thread>

#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"bcrypt.lib")
namespace mt2mp::reporting {
namespace fs=std::filesystem;
namespace {
using Clock=std::chrono::steady_clock;
constexpr size_t event_limit=256, bundle_limit=256*1024, report_limit=20;
struct State {
    std::mutex mutex;
    std::condition_variable cv;
    std::thread worker;
    bool ready=false,stop=false,open=false,prepare=false,submit=false,automatic=false,automatic_inflight=false,preferences_dirty=false,incident=false;
    unsigned auto_sent=0,slot=0;
    uint64_t dropped=0;
    std::string session,client,error,phase="idle",receipt;
    fs::path root;
    json build,latest,events=json::array();
    std::deque<json> background;
    Clock::time_point started,last_incident{};
    double frame_total=0,frame_max=0;
    uint64_t frames=0;
};
State state;
#ifdef MT2MP_REPORTING_TEST
json(*test_upload)(const json&)=nullptr;
#endif
uint64_t milliseconds(){return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-state.started).count());}
std::string bounded_text(std::string text,size_t maximum){
    if(text.size()>maximum)text.resize(maximum);
    while(!text.empty()&&!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),nullptr,0))text.pop_back();
    return text;
}
std::string digest(const std::string& text){
    std::array<unsigned char,32> bytes{};
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA256 provider failed");
    const auto code=BCryptHash(algorithm,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<char*>(text.data())),ULONG(text.size()),bytes.data(),ULONG(bytes.size()));
    BCryptCloseAlgorithmProvider(algorithm,0);
    if(code<0)throw std::runtime_error("SHA256 digest failed");
    const char* hex="0123456789abcdef";std::string out;for(auto byte:bytes){out+=hex[byte>>4];out+=hex[byte&15];}return out;
}
json read_json(const fs::path& path){
    if(fs::is_symlink(fs::symlink_status(path))||!fs::is_regular_file(path)||fs::file_size(path)>bundle_limit)
        throw std::runtime_error("diagnostic file type or size invalid");
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("diagnostic file open failed");
    return json::parse(input);
}
void atomic_write(const fs::path& path,const json& value){
    const auto text=value.dump();if(text.size()>bundle_limit)throw std::runtime_error("diagnostic bundle exceeds 256 KiB");
    const auto temporary=fs::path(path.wstring()+L".tmp");
    const auto handle=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("diagnostic write open failed Win32="+std::to_string(GetLastError()));
    DWORD written=0;const bool stored=WriteFile(handle,text.data(),DWORD(text.size()),&written,nullptr)&&written==text.size()&&FlushFileBuffers(handle);
    const auto cause=GetLastError();CloseHandle(handle);
    if(!stored)throw std::runtime_error("diagnostic write/flush failed Win32="+std::to_string(cause));
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("diagnostic commit failed Win32="+std::to_string(GetLastError()));
}
void retain(){
    std::vector<fs::directory_entry> reports;
    for(const auto& file:fs::directory_iterator(state.root))
        if(file.is_regular_file()&&!file.is_symlink()&&file.path().extension()==L".json"&&valid_id(file.path().stem().string()))reports.push_back(file);
    std::sort(reports.begin(),reports.end(),[](const auto& a,const auto& b){return a.last_write_time()>b.last_write_time();});
    for(size_t i=report_limit;i<reports.size();++i){
        const auto id=reports[i].path().stem().wstring();fs::remove(reports[i].path());
        const auto receipt=state.root/(id+L".receipt");if(fs::exists(receipt))fs::remove(receipt);
    }
}
json bundle(const char* kind){
    return {{"schema",1},{"product","mmorpgtycoon2-multiplayer"},{"report_id",random_id()},
        {"diagnostic_session",state.session},{"client_id",state.client},{"kind",kind},{"consent",false},{"comment",""},
        {"build",state.build},{"role",state.slot==1?"host":state.slot?"client":"none"},{"peer_slot",state.slot},
        {"elapsed_ms",milliseconds()},{"dropped_events",state.dropped},{"events",state.events}};
}
struct HttpHandle {HINTERNET value=nullptr;~HttpHandle(){if(value)WinHttpCloseHandle(value);}operator HINTERNET()const{return value;}};
json upload(const json& value){
#ifdef MT2MP_REPORTING_TEST
    if(!test_upload)throw std::runtime_error("test upload transport is not installed; real network refused");
    return test_upload(value);
#endif
    const auto body=value.dump();
    HttpHandle session{WinHttpOpen(L"MT2MultiplayerDiagnostics/1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,nullptr,nullptr,0)};
    if(!session)throw std::runtime_error("Upload could not start. Your report is saved locally.");
    if(!WinHttpSetTimeouts(session,5000,5000,10000,10000))throw std::runtime_error("Upload timeout configuration failed.");
    HttpHandle connection{WinHttpConnect(session,L"support.doodesch.de",INTERNET_DEFAULT_HTTPS_PORT,0)};
    HttpHandle request{connection?WinHttpOpenRequest(connection,L"POST",L"/api/diagnostics",nullptr,nullptr,nullptr,WINHTTP_FLAG_SECURE):nullptr};
    if(!request)throw std::runtime_error("Could not connect. Your report is saved locally.");
    DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if(!WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy)))throw std::runtime_error("Upload redirect protection failed.");
    if(!WinHttpSendRequest(request,L"Content-Type: application/json\r\n",DWORD(-1),const_cast<char*>(body.data()),DWORD(body.size()),DWORD(body.size()),0)||!WinHttpReceiveResponse(request,nullptr))
        throw std::runtime_error("Delivery is unconfirmed. Retry sends the same report safely.");
    DWORD code=0,size=sizeof(code);
    if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&code,&size,nullptr))throw std::runtime_error("Upload response status is unavailable.");
    if(code!=201)throw std::runtime_error(code==429?"Too many reports. Please try again later.":"Upload failed (HTTP "+std::to_string(code)+"). Your report is saved locally.");
    std::string response;std::array<char,2048> buffer{};DWORD read=0;
    do{if(!WinHttpReadData(request,buffer.data(),DWORD(buffer.size()),&read))throw std::runtime_error("Delivery is unconfirmed. Retry sends the same report safely.");
        response.append(buffer.data(),read);if(response.size()>8192)throw std::runtime_error("Upload receipt exceeds 8 KiB");}while(read);
    const auto result=json::parse(response);
    if(!result.at("ok").get<bool>())throw std::runtime_error("Server did not confirm private storage.");
    const auto receipt=result.at("receipt");
    if(receipt.at("report_id")!=value.at("report_id")||receipt.at("upload_sha256")!=digest(body)||!receipt.at("stored_at").is_string())
        throw std::runtime_error("Upload receipt does not match this report. Delivery is unconfirmed.");
    return receipt;
}
void warning(const std::string& cause){OutputDebugStringA(("warn: Multiplayer reporting "+cause+"\n").c_str());}
void work(){
    auto next=Clock::now();
    for(;;){
        json snapshot,send;bool save=false,prefs=false,automatic=false,stop=false,visible_send=false,automatic_send=false;
        {
            std::unique_lock lock(state.mutex);
            state.cv.wait_until(lock,next,[]{return state.stop||state.prepare||state.submit||state.preferences_dirty||state.incident||!state.background.empty();});
            stop=state.stop;
            if(state.incident||state.prepare){
                snapshot=bundle(state.incident?"error":"manual");save=true;
                if(state.prepare||state.latest.is_null()||state.phase=="sent"){
                    state.latest=snapshot;state.phase="ready";state.error.clear();
                }
                if(state.incident&&state.automatic&&state.auto_sent<5&&state.background.size()<8){
                    auto automatic_report=snapshot;automatic_report["consent"]=true;state.background.push_back(std::move(automatic_report));++state.auto_sent;
                }
            }
            state.prepare=state.incident=false;
            if(!save)snapshot=bundle("session");
            if(state.submit){send=state.latest;state.submit=false;state.phase="sending";visible_send=true;}
            else if(!state.background.empty()){send=std::move(state.background.front());state.background.pop_front();automatic_send=true;visible_send=!state.latest.is_null()&&state.latest.at("report_id")==send.at("report_id");if(visible_send)state.phase="sending";}
            prefs=state.preferences_dirty;automatic=state.automatic;state.preferences_dirty=false;
        }
        try{
            if(prefs)atomic_write(state.root/L"preferences.json",{{"automatic",automatic}});
            if(save){atomic_write(state.root/(snapshot.at("report_id").get<std::string>()+".json"),snapshot);retain();}
            if(!send.is_null()){
                const auto id=send.at("report_id").get<std::string>();
                atomic_write(state.root/(id+".json"),send);
                retain();
                if(!stop){
                    bool permitted=true;
                    if(automatic_send){std::lock_guard lock(state.mutex);permitted=state.automatic;if(permitted)state.automatic_inflight=true;else if(visible_send)state.phase="ready";}
                    if(!permitted)continue;
                    const auto receipt=upload(send);atomic_write(state.root/(id+".receipt"),receipt);
                    std::lock_guard lock(state.mutex);state.automatic_inflight=false;if(visible_send){state.receipt=id;state.phase="sent";state.error.clear();}
                }
            }
            if(stop){
                {std::lock_guard lock(state.mutex);if(!state.background.empty()||state.submit)continue;}
                if(fs::exists(state.root/L"active.json"))fs::remove(state.root/L"active.json");return;
            }
            if(Clock::now()>=next){atomic_write(state.root/L"active.json",snapshot);next=Clock::now()+std::chrono::seconds(5);}
        }catch(const std::exception& error){warning(error.what());std::lock_guard lock(state.mutex);
            state.automatic_inflight=false;
            if(visible_send||save||prefs){state.error=error.what();state.phase="failed";}
            if(stop)return;next=Clock::now()+std::chrono::seconds(5);}
    }
}
}
bool valid_id(const std::string& value){
    static const std::regex pattern("^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$");return std::regex_match(value,pattern);
}
std::string random_id(){
    std::array<unsigned char,16> data{};if(BCryptGenRandom(nullptr,data.data(),ULONG(data.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("diagnostic identity generation failed");
    data[6]=(data[6]&15)|64;data[8]=(data[8]&63)|128;const char* hex="0123456789abcdef";std::string result;
    for(size_t i=0;i<data.size();++i){if(i==4||i==6||i==8||i==10)result+='-';result+=hex[data[i]>>4];result+=hex[data[i]&15];}return result;
}
std::string redact(std::string value){
    static const std::regex path(R"(\b[A-Za-z]:[\\/][^\r\n"<>|]*)"), unix_path(R"(/(?:home|Users)/[^\s"']+)"),
        account(R"(\d{17,})"),ip(R"(\b(?:\d{1,3}\.){3}\d{1,3}(?::\d+)?\b)"),
        secret(R"(\b(token|password|authorization|cookie|secret|session_token)\s*[:=]\s*(?:"[^"]*"|'[^']*'|\S+))",std::regex::icase),
        bearer(R"(\bBearer\s+\S+)",std::regex::icase),email(R"([\w.+-]+@[\w.-]+\.[A-Za-z]{2,})");
    value=std::regex_replace(value,path,"[path]");value=std::regex_replace(value,unix_path,"[path]");value=std::regex_replace(value,account,"[account]");
    value=std::regex_replace(value,ip,"[address]");value=std::regex_replace(value,bearer,"Bearer [redacted]");value=std::regex_replace(value,secret,"$1=[redacted]");return std::regex_replace(value,email,"[email]");
}
void initialize(const fs::path& data,const json& modules,const json& mods){
    std::lock_guard lock(state.mutex);if(state.ready)throw std::runtime_error("reporting already initialized");
    state.root=data/L"reports";private_profile::DirectoryPins pins;pins.verify(state.root,true);
    state.stop=state.open=state.prepare=state.submit=state.preferences_dirty=state.incident=false;
    state.latest=nullptr;state.events=json::array();state.background.clear();state.error.clear();state.receipt.clear();state.phase="idle";state.slot=0;state.dropped=0;state.auto_sent=0;state.last_incident={};
    state.started=Clock::now();state.session=random_id();state.client=random_id();
    json hashes=json::object();const std::map<std::string,std::string> names={{"MT2.exe","game"},{"MT2Framework.dll","framework"},{"WSOCK32.dll","loader"},{"MT2MultiplayerPlugin.dll","multiplayer"},{"steam_api64.dll","steam"}};
    for(const auto& [name,key]:names)if(modules.contains(name))hashes[key]=modules.at(name);
    auto installed=mods;
    const auto mod_root=data.parent_path().parent_path()/L"mods";
    if(installed.empty()&&fs::is_directory(mod_root)){
        for(const auto& item:fs::directory_iterator(mod_root)){
            if(installed.size()>=128)break;
            if(!item.is_directory()||item.is_symlink())continue;
            const auto manifest=item.path()/L"mod.json";
            if(!fs::is_regular_file(manifest))continue;
            try{
                const auto metadata=read_json(manifest);
                const auto id=metadata.at("id").get<std::string>(),version=metadata.at("version").get<std::string>();
                static const std::regex label("^[A-Za-z0-9_.:/ -]{1,120}$");
                if(std::regex_match(id,label)&&std::regex_match(version,label))installed.push_back({{"id",id},{"version",version}});
                else warning("installed mod manifest id/version invalid; metadata skipped");
            }catch(const std::exception& error){warning(std::string("installed mod metadata unavailable: ")+error.what());}
        }
    }
    state.build={{"multiplayer",MOD_VERSION},{"hashes",hashes},{"mods",installed}};
    if(fs::exists(state.root/L"preferences.json"))state.automatic=read_json(state.root/L"preferences.json").at("automatic").get<bool>();
    if(fs::exists(state.root/L"active.json")){
        auto prior=read_json(state.root/L"active.json");prior["report_id"]=random_id();prior["kind"]="interrupted";prior["consent"]=false;
        atomic_write(state.root/(prior.at("report_id").get<std::string>()+".json"),prior);state.latest=prior;state.phase="ready";
    }
    if(state.latest.is_null()){
        std::vector<fs::directory_entry> pending;
        for(const auto& file:fs::directory_iterator(state.root))
            if(file.is_regular_file()&&!file.is_symlink()&&file.path().extension()==L".json"&&valid_id(file.path().stem().string())&&!fs::exists(state.root/(file.path().stem().wstring()+L".receipt")))pending.push_back(file);
        if(!pending.empty()){
            const auto latest=std::max_element(pending.begin(),pending.end(),[](const auto& a,const auto& b){return a.last_write_time()<b.last_write_time();});
            state.latest=read_json(latest->path());state.phase="ready";
        }
    }
    retain();state.ready=true;state.worker=std::thread(work);
}
void shutdown(){
    {std::lock_guard lock(state.mutex);if(!state.ready)return;state.stop=true;state.cv.notify_one();}
    if(state.worker.joinable())state.worker.join();std::lock_guard lock(state.mutex);state.ready=false;
}
void observe(int level,const char* source,int line,const std::string& message){
    if(level<2)return;
    try{
        const auto safe=redact(bounded_text(message,512));const auto file=fs::path(source).filename().string();
        std::lock_guard lock(state.mutex);if(!state.ready||state.stop)return;
        if(!state.events.empty()){auto& last=state.events.back();if(last.at("source")==file&&last.at("line")==line&&last.at("cause")==safe){last["values"]["count"]=last.at("values").value("count",uint64_t{1})+1;return;}}
        if(state.events.size()>=event_limit){state.events.erase(state.events.begin());++state.dropped;}
        state.events.push_back({{"ms",milliseconds()},{"level",level==3?"error":"warn"},{"code","runtime.error"},{"source",file},{"line",std::max(0,line)},{"cause",safe},{"values",json::object()},{"action_id",""}});
        if(level==3&&(state.last_incident==Clock::time_point{}||Clock::now()-state.last_incident>=std::chrono::seconds(60))){state.incident=true;state.last_incident=Clock::now();state.cv.notify_one();}
    }catch(const std::exception& error){warning(std::string("event capture failed: ")+error.what());}
}
void event(const std::string& code,const json& values,const std::string& action_id){
    static const std::array<const char*,10> allowed={"action_sequence","revision","peer_slot","required_mask","ack_mask","channel","bytes","duration_us","count","frame_us"};
    static const std::regex action_pattern("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    if(code.size()>120||(!action_id.empty()&&!std::regex_match(action_id,action_pattern))){warning("diagnostic event identity invalid; event skipped");return;}
    json safe=json::object();for(const auto* name:allowed)if(values.contains(name)&&values.at(name).is_number_unsigned())safe[name]=values.at(name);
    std::lock_guard lock(state.mutex);if(!state.ready||state.stop)return;
    if(state.events.size()>=event_limit){state.events.erase(state.events.begin());++state.dropped;}
    state.events.push_back({{"ms",milliseconds()},{"level","info"},{"code",code},{"source",""},{"line",0},{"cause",""},{"values",safe},{"action_id",action_id}});
}
void bind_session(const std::string& id,unsigned slot){
    if(!valid_id(id)||slot<1||slot>4)throw std::runtime_error("diagnostic session binding invalid");
    std::lock_guard lock(state.mutex);if(!state.ready)return;
    const bool joined=state.session!=id||state.slot!=slot;state.session=id;state.slot=slot;
    if(joined&&state.automatic&&state.background.size()<8){
        auto sample=bundle("session");sample["consent"]=true;state.background.push_back(std::move(sample));state.cv.notify_one();
    }
}
void end_session(){
    std::lock_guard lock(state.mutex);if(!state.ready)return;
    if(state.automatic&&state.slot&&state.background.size()<8){auto sample=bundle("session");sample["consent"]=true;state.background.push_back(std::move(sample));state.cv.notify_one();}
    state.slot=0;state.session=random_id();state.events=json::array();state.dropped=0;
}
void frame(double ms){
    if(ms<0||ms>60000)return;
    std::lock_guard lock(state.mutex);if(!state.ready)return;state.frame_total+=ms;state.frame_max=std::max(state.frame_max,ms);++state.frames;
    if(state.frames%300==0){
        if(state.events.size()>=event_limit){state.events.erase(state.events.begin());++state.dropped;}
        state.events.push_back({{"ms",milliseconds()},{"level","info"},{"code","performance.frames"},{"source",""},{"line",0},{"cause",""},
            {"values",{{"count",state.frames},{"duration_us",uint64_t(state.frame_total*1000)},{"frame_us",uint64_t(state.frame_max*1000)}}},{"action_id",""}});
        state.frame_max=state.frame_total=0;state.frames=0;
    }
}
void open_report(){std::lock_guard lock(state.mutex);if(!state.ready)return;state.open=true;if(state.latest.is_null()||state.phase=="sent"){state.prepare=true;state.phase="preparing";state.cv.notify_one();}}
void send_report(const std::string& comment){
    std::lock_guard lock(state.mutex);if(!state.ready||state.latest.is_null()||state.phase=="sending"||state.phase=="sent")return;
    if(!state.latest.at("consent").get<bool>()){state.latest["comment"]=redact(bounded_text(comment,2000));state.latest["consent"]=true;}
    state.submit=true;state.phase="sending";state.error.clear();state.cv.notify_one();
}
void automatic_reports(bool enabled){std::lock_guard lock(state.mutex);if(!state.ready)return;state.automatic=enabled;if(!enabled)state.background.clear();state.preferences_dirty=true;state.cv.notify_one();}
json status(){std::lock_guard lock(state.mutex);return {{"ready",state.ready},{"phase",state.phase},{"error",state.error},{"automatic",state.automatic},{"automatic_inflight",state.automatic_inflight},
    {"sealed",!state.latest.is_null()&&state.latest.at("consent").get<bool>()},
    {"report_id",state.latest.is_null()?"":state.latest.value("report_id",std::string{})},{"receipt",state.receipt},{"session",state.session},
    {"event_count",state.latest.is_null()?state.events.size():state.latest.at("events").size()}};}
#ifdef MT2MP_REPORTING_TEST
namespace test {void set_upload(json(*transport)(const json&)){std::lock_guard lock(state.mutex);if(state.ready)throw std::runtime_error("test transport cannot change while active");test_upload=transport;}}
#endif
}
