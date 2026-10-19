#include "additions.h"
#include "mt2mp/core/hash.h"
#include <algorithm>
namespace mt2mp::subscriber_lifecycle::additions {
namespace {
bool fail(std::string& e,const std::string& s){e=s;return false;}
std::string digest(const std::string& s){return mt2mp::sha256(std::vector<unsigned char>(s.begin(),s.end()));}
Json token(const Json& t){const int kind=t.at("type");if(!kind)return Json::array({0});return Json::array({kind,t.at(kind==1||kind==2?"text":"bits")});}
Json expand(const Json& t){if(!t.is_array()||t.empty()||!t[0].is_number_integer())throw std::runtime_error("Subscriber compact token invalid");const int kind=t[0];if(kind<0||kind>4||t.size()!=(kind?2u:1u))throw std::runtime_error("Subscriber compact token arity invalid");Json v={{"type",kind}};if(kind)v[kind==1||kind==2?"text":"bits"]=t[1];return v;}
bool ids_valid(const std::vector<int32_t>& ids){std::set<int32_t> set;for(auto id:ids)if(id<0||!set.insert(id).second)return false;return true;}
}
bool frame_binding(const Json& graph,uint64_t session,const std::string& snapshot,uint64_t epoch,std::string& e)try{
    const auto& b=graph.at("frame_binding");
    if(!session||!epoch||snapshot.empty()||!b.is_object()||b.size()!=3||!b.at("session").is_number_unsigned()||!b.at("epoch").is_number_unsigned()||!b.at("snapshot_uuid").is_string()||b.at("session").get<uint64_t>()!=session||b.at("epoch").get<uint64_t>()!=epoch||b.at("snapshot_uuid").get<std::string>()!=snapshot)return fail(e,"Subscriber authenticated Frame session/snapshot/epoch differs");
    e.clear();return true;
}catch(const std::exception& x){return fail(e,std::string("Subscriber Frame provenance: ")+x.what());}
bool pack(const Json& artifact,std::string& out,std::string& e){out.clear();try{first_append::Payload p;if(!first_append::parse_payload(artifact,p,e))return false;Json header=artifact;header.erase("records");Json rows=Json::array();for(const auto& row:artifact.at("records")){Json tokens=Json::array();for(const auto& t:row.at("tokens"))tokens.push_back(token(t));rows.push_back(Json::array({row.at("parent"),row.at("child_index"),token(row.at("label")),tokens}));}out=Json::array({header,rows}).dump();return true;}catch(const std::exception& x){return fail(e,std::string("Subscriber compact pack: ")+x.what());}}
bool unpack(const std::string& wire,Json& artifact,first_append::Payload& p,std::string& e){artifact={};try{const auto root=Json::parse(wire,[](int depth,Json::parse_event_t,Json&){if(depth>6)throw std::runtime_error("Subscriber compact nesting exceeds6");return true;});if(!root.is_array()||root.size()!=2||!root[0].is_object()||!root[1].is_array())return fail(e,"Subscriber compact root shape invalid");artifact=root[0];if(artifact.contains("records"))return fail(e,"Subscriber compact header repeats records");Json rows=Json::array();for(const auto& r:root[1]){if(!r.is_array()||r.size()!=4||!r[3].is_array())return fail(e,"Subscriber compact row shape invalid");Json tokens=Json::array();for(const auto& t:r[3])tokens.push_back(expand(t));rows.push_back({{"parent",r[0]},{"child_index",r[1]},{"label",expand(r[2])},{"tokens",tokens}});}artifact["records"]=std::move(rows);return first_append::parse_payload(artifact,p,e);}catch(const std::exception& x){return fail(e,std::string("Subscriber compact unpack: ")+x.what());}}
bool valid(const State& state,std::string& e){
    if(!state.available)return (state.next_id==-1&&state.baseline.empty()&&state.graphs.empty()&&state.retired.empty())||fail(e,"unavailable Subscriber state contains data");
    if(state.next_id<0||!ids_valid(state.baseline))return fail(e,"Subscriber state baseline/nextID invalid");
    std::set<int32_t> dead;for(const auto& row:state.retired)if(row.id<0||row.id>=state.next_id||row.reason<0||!dead.insert(row.id).second)return fail(e,"Subscriber retirement ID/reason duplicate or outside nextID");
    std::set<int32_t> ids;for(auto id:state.baseline){if(id>=state.next_id)return fail(e,"Subscriber baseline ID outside nextID");ids.insert(id);}
    for(const auto& graph:state.graphs){Json artifact;first_append::Payload parsed;if(!unpack(graph,artifact,parsed,e))return false;if(parsed.next_id>state.next_id||parsed.id>=state.next_id||dead.contains(parsed.id)||!ids.insert(parsed.id).second)return fail(e,"Subscriber graph ID overlaps baseline or exceeds Host nextID");}
    e.clear();return true;
}
bool Cache::host(const std::vector<int32_t>& actual,int32_t next,uint64_t revision,Capture capture,State& out,std::string& e,const std::vector<Retirement>& receipts)try{
    out={};if(locked_||!revision||next<0||!ids_valid(actual))return fail(e,"Subscriber Host cache locked/invalid actual cohort");
    if(!bound_){baseline_=actual;bound_=true;}
    std::map<int32_t,int32_t> incoming;
    for(const auto& row:receipts){if(row.id<0||row.id>=next||row.reason<0||!incoming.emplace(row.id,row.reason).second||std::count(actual.begin(),actual.end(),row.id))return fail(e,"Subscriber Host original retirement receipt invalid/live/duplicate");auto old=retired_.find(row.id);if(old!=retired_.end()&&old->second!=row.reason)return fail(e,"Subscriber Host original retirement reason changed");}
    for(const auto& [id,reason]:retired_)if(!incoming.contains(id))return fail(e,"Subscriber Host original retirement receipt disappeared");
    for(auto id:baseline_)if(!std::count(actual.begin(),actual.end(),id)&&!incoming.contains(id))return fail(e,"Subscriber baseline removal needs original D0 receipt");
    for(const auto& [id,row]:entries_)if(!std::count(actual.begin(),actual.end(),id)&&!incoming.contains(id))return fail(e,"Subscriber added member removal needs original D0 receipt");
    retired_=std::move(incoming);
    for(auto id:actual){if(id>=next)return fail(e,"Subscriber actual ID outside Host nextID");if(std::find(baseline_.begin(),baseline_.end(),id)!=baseline_.end()||entries_.contains(id))continue;
        Json artifact;std::string wire;if(!capture(id,artifact,e)||!pack(artifact,wire,e)){locked_=true;return false;}
        first_append::Payload parsed;Json roundtrip;if(!unpack(wire,roundtrip,parsed,e)||parsed.id!=id||parsed.next_id!=next){locked_=true;return fail(e,e.empty()?"Subscriber captured cohort moved during Frame":e);}
        entries_.emplace(id,Entry{id,digest(wire),std::move(wire),0,revision});
    }
    out.available=true;out.next_id=next;out.baseline=baseline_;for(const auto& [id,reason]:retired_)out.retired.push_back({id,reason});for(const auto& [id,row]:entries_)if(!row.graph.empty()&&!retired_.contains(id))out.graphs.push_back(row.graph);e.clear();return true;
}
catch(const std::exception& x){locked_=true;return fail(e,std::string("Subscriber Host capture exception original=")+x.what());}
catch(...){locked_=true;return fail(e,"Subscriber Host capture nonstandard exception; no replay");}
bool Cache::client(const std::vector<int32_t>& actual,const std::vector<int32_t>& target,const State& state,Append append,Json& receipt,std::string& e,Retire retire,AppendBatch batch)try{
    receipt={{"attempted",false},{"confirmed",false},{"unknown",false},{"created_ids",Json::array()},{"retired_ids",Json::array()},{"skipped_retired_ids",Json::array()},{"native_receipts",Json::array()}};
    if(locked_||!state.available||!ids_valid(actual)||!ids_valid(target)||!valid(state,e))return fail(e,e.empty()?"Subscriber Client membership locked/invalid":e);
    if(!bound_){baseline_=actual;bound_=true;}
    if(baseline_!=state.baseline)return fail(e,"Subscriber actual bound initial baseline IDs differ from Host baseline");
    std::set<int32_t> present(actual.begin(),actual.end()),wanted(target.begin(),target.end());
    std::map<int32_t,int32_t> dead;for(const auto& row:state.retired){dead.emplace(row.id,row.reason);if(wanted.contains(row.id))return fail(e,"Subscriber retired ID remains a live target");}
    for(const auto& [id,reason]:retired_)if(!dead.contains(id)||dead.at(id)!=reason||wanted.contains(id))return fail(e,"Subscriber confirmed retirement disappeared/changed/reused");
    for(auto id:actual)if(!wanted.contains(id)&&!dead.contains(id))return fail(e,"Subscriber local removal lacks original Host retirement receipt");
    for(const auto& [id,reason]:dead)if(!present.contains(id)&&!retired_.contains(id)&&(std::count(baseline_.begin(),baseline_.end(),id)||entries_.contains(id)))return fail(e,"Subscriber missing local known ID has no prior retirement receipt");
    std::map<int32_t,std::pair<Json,Entry>> prepared;std::map<int32_t,std::vector<int32_t>> dependencies;
    for(const auto& wire:state.graphs){Json artifact;first_append::Payload p;if(!unpack(wire,artifact,p,e))return false;const auto sha=digest(wire);
        if(!wanted.contains(p.id)||dead.contains(p.id)||std::count(baseline_.begin(),baseline_.end(),p.id)||prepared.contains(p.id))return fail(e,"Subscriber constructor graph ID invalid/reused/retired");
        auto old=entries_.find(p.id);if(old!=entries_.end()&&old->second.sha!=sha)return fail(e,"Subscriber repeated graph conflicts with confirmed constructor source");
        for(const auto& ref:p.external)if(ref.key.kind==first_append::ReferenceKind::SubscriberToon)dependencies[p.id].push_back(ref.key.subscriber_id);
        artifact["next_subscriber_id"]=state.next_id;prepared.emplace(p.id,std::make_pair(std::move(artifact),Entry{p.id,sha,{}}));
    }
    std::vector<int32_t> missing;for(auto id:target){if(id<0||id>=state.next_id)return fail(e,"Subscriber live target outside Host nextID");if(!present.contains(id)){if(retired_.contains(id)||!prepared.contains(id))return fail(e,"Subscriber absent live ID lacks its original constructor graph");missing.push_back(id);}}
    for(const auto& [id,row]:prepared)if(!entries_.contains(id)&&present.contains(id))return fail(e,"Subscriber preexisting non-baseline ID has no local constructor receipt");
    auto available=present;if(batch)available.insert(missing.begin(),missing.end());for(auto id:missing){for(auto dependency:dependencies[id])if(!available.contains(dependency))return fail(e,"Subscriber Toon dependency absent before all native effects subscriber_id="+std::to_string(id)+" owner_id="+std::to_string(dependency));available.insert(id);}
    if(!batch&&!append&&!missing.empty())return fail(e,"Subscriber original constructor callback unavailable");
    for(auto id:actual)if(!wanted.contains(id)&&!retire)return fail(e,"Subscriber original Manager retirement callback unavailable");
    auto unknown=[&](){locked_=receipt.value("attempted",false);receipt["unknown"]=locked_;};
    if(batch){
        std::vector<Json> graphs;for(auto id:missing)graphs.push_back(prepared.at(id).first);
        Json native;const bool okay=batch(graphs,native,e);
        bool effect=native.value("attempted",false)||native.value("unknown",false)||native.value("confirmed",false)||native.value("subscriber_creation_attempted",false);
        if(native.contains("rows")&&native.at("rows").is_array())for(const auto& row:native.at("rows"))
            if(row.is_object())effect=effect||row.value("state",std::string{})=="confirmed"||row.value("state",std::string{})=="unknown";
        receipt["attempted"]=receipt.value("attempted",false)||effect;
        receipt["native_receipts"].push_back(native);
        if(!okay||!native.value("confirmed",false)||native.value("unknown",false)||!native.contains("rows")||
           !native.at("rows").is_array()||native.at("rows").size()!=missing.size()){
            unknown();return fail(e,e.empty()?"Subscriber grouped original constructor receipt unconfirmed":e);
        }
        for(size_t i=0;i<missing.size();++i){const auto& row=native.at("rows")[i];
            if(!row.is_object()||!row.contains("id")||!row.at("id").is_number_integer()||row.at("id")!=missing[i]||
               row.value("state",std::string{})!="confirmed"){
                unknown();return fail(e,"Subscriber grouped original constructor row/order unconfirmed");
            }
        }
        for(auto id:missing){receipt["created_ids"].push_back(id);entries_[id]=prepared.at(id).second;}
    }else{
    for(auto id:missing){Json native;const bool ok=append(prepared.at(id).first,native,e);const auto outcome=native.value("state",std::string{});receipt["attempted"]=receipt.value("attempted",false)||native.value("subscriber_creation_attempted",false)||native.value("attempted",false)||outcome=="unknown"||outcome=="confirmed";if(!ok||outcome!="confirmed"){receipt["native_receipts"].push_back(native);unknown();return fail(e,e.empty()?"Subscriber original C1 receipt unconfirmed":e);}receipt["native_receipts"].push_back(native);receipt["created_ids"].push_back(id);entries_[id]=prepared.at(id).second;}
    }
    for(auto id:actual)if(!wanted.contains(id)){Json native;const bool ok=retire(id,dead.at(id),native,e);receipt["attempted"]=receipt.value("attempted",false)||native.value("attempted",false)||native.value("unknown",false)||native.value("confirmed",false);if(!ok||!native.value("confirmed",false)){receipt["native_receipts"].push_back(native);unknown();return fail(e,e.empty()?"Subscriber original Manager retirement receipt unconfirmed":e);}receipt["native_receipts"].push_back(native);receipt["retired_ids"].push_back(id);retired_[id]=dead.at(id);}
    for(const auto& [id,reason]:dead)if(!retired_.contains(id)){receipt["skipped_retired_ids"].push_back(id);retired_[id]=reason;}
    receipt["confirmed"]=true;e.clear();return true;
}catch(const std::exception& x){locked_=true;receipt["unknown"]=true;return fail(e,std::string("Subscriber membership exception original=")+x.what());}
catch(...){locked_=true;receipt["unknown"]=true;return fail(e,"Subscriber membership nonstandard exception; no replay");}
Json Cache::acknowledgement(const State& state){Json rows=Json::array();for(const auto& wire:state.graphs){Json j;first_append::Payload p;std::string e;if(!unpack(wire,j,p,e))throw std::runtime_error(e);rows.push_back({{"id",p.id},{"sha256",digest(wire)}});}return rows;}
bool Cache::acknowledge(uint8_t peer,uint8_t peers,uint64_t revision,const Json& rows,std::string& e)try{
    if(locked_||peer<1||peer>4||!(peers&(1u<<(peer-1)))||!rows.is_array())return fail(e,"Subscriber ACK peer/cache/rows invalid");std::set<int32_t> seen;
    for(const auto& r:rows){if(!r.is_object()||r.size()!=2||!r.at("id").is_number_integer()||(r.at("id").is_number_unsigned()?r.at("id").get<uint64_t>()>INT32_MAX:r.at("id").get<int64_t>()<0||r.at("id").get<int64_t>()>INT32_MAX)||!r.at("sha256").is_string())return fail(e,"Subscriber ACK row invalid");auto id=r.at("id").get<int32_t>();auto found=entries_.find(id);if(!seen.insert(id).second||found==entries_.end()||r.at("sha256")!=found->second.sha||revision<found->second.first_frame)return fail(e,"Subscriber ACK ID/hash/Frame differs");}
    for(const auto& r:rows){auto& row=entries_.at(r.at("id").get<int32_t>());row.acknowledgements|=uint8_t(1u<<(peer-1));if((row.acknowledgements&peers)==peers)row.graph.clear();}e.clear();return true;
}
catch(const std::exception& x){return fail(e,std::string("Subscriber ACK original=")+x.what());}

}
