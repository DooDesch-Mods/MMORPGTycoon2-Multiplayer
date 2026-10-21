#include "node_state.h"
#include <bit>
namespace mt2mp::game::live_host_state::network {
Receipt apply(Port& port,const State& target){
    Receipt r;
    try{
        if(!valid(target,r.cause))return r;
        State before;if(!port.read(before,r.cause)||!valid(before,r.cause))return r;
        if(auto unchanged=port.confirm_no_effect(before,target))return std::move(*unchanged);
        if(before.other_graph_sha256!=target.other_graph_sha256||before.nodes.size()!=target.nodes.size()||before.pipes.size()!=target.pipes.size()||before.connections.size()!=target.connections.size()){
            r.cause="network native non-Node graph/membership differs before stores";return r;}
        for(size_t i=0;i<before.nodes.size();++i){const auto& a=before.nodes[i];const auto& b=target.nodes[i];
            if(a.region!=b.region||a.index!=b.index){r.cause="network actual node key order differs";return r;}
            if(a.flags[2]&&!b.flags[2]){r.cause="network Activated cannot transition 1 to 0";return r;}
        }
        for(size_t i=0;i<before.pipes.size();++i){const auto& a=before.pipes[i];const auto& b=target.pipes[i];
            if(a.region!=b.region||a.index!=b.index||a.current.size()!=b.current.size()){
                r.cause="network actual pipe key/current shape differs";return r;}
        }
        for(size_t i=0;i<before.connections.size();++i)if(before.connections[i].key!=target.connections[i].key){r.cause="network connection owner/rank differs";return r;}
        if(before.managers.has_value()!=target.managers.has_value()||(before.managers&&before.managers->size()!=target.managers->size())){r.cause="network complete Manager membership differs";return r;}
        if(before.managers)for(size_t i=0;i<before.managers->size();++i)if((*before.managers)[i].region!=(*target.managers)[i].region){r.cause="network Manager RegionID differs before stores";return r;}
        if(!port.preflight(before,target,r.cause))return r;
        r.activations.reserve(before.nodes.size());
        for(size_t i=0;i<before.nodes.size();++i)if(!before.nodes[i].flags[2]&&target.nodes[i].flags[2]){
            r.activations.push_back({before.nodes[i].region,before.nodes[i].index,{}});
            r.attempted=true;
            auto& event=r.activations.back().event;event=port.activate(i);
            if(!event.confirmed){r.unknown=true;r.cause="network original activation Unknown: "+event.cause;return r;}
            State fresh;
            if(!port.read(fresh,r.cause)){r.unknown=true;r.cause="network post-event read Unknown original="+r.cause;return r;}
            if(fresh.other_graph_sha256!=before.other_graph_sha256||fresh.nodes.size()!=before.nodes.size()||fresh.pipes.size()!=before.pipes.size()||fresh.connections.size()!=before.connections.size()){
                r.unknown=true;r.cause="network post-event graph/membership changed";return r;}
            for(size_t k=0;k<fresh.nodes.size();++k){const auto& a=fresh.nodes[k];const auto& b=before.nodes[k];
                if(a.region!=b.region||a.index!=b.index||a.flags[2]!=(k==i?1:b.flags[2])){
                    r.unknown=true;r.cause="network post-event key/activation ownership changed";return r;}
                for(auto value:a.values)if(!std::isfinite(value)){r.unknown=true;r.cause="network post-event nonfinite scalar";return r;}
                for(auto flag:a.flags)if(flag>1){r.unknown=true;r.cause="network post-event invalid bool scalar";return r;}
            }
            for(size_t k=0;k<fresh.pipes.size();++k){const auto& a=fresh.pipes[k];const auto& b=before.pipes[k];
                if(a.region!=b.region||a.index!=b.index||a.current.size()!=b.current.size()){
                    r.unknown=true;r.cause="network post-event pipe membership changed";return r;}
            }
            for(size_t k=0;k<fresh.connections.size();++k)if(fresh.connections[k].key!=before.connections[k].key){
                r.unknown=true;r.cause="network post-event connection membership changed";return r;}
            if(fresh.managers.has_value()!=before.managers.has_value()||(fresh.managers&&fresh.managers->size()!=before.managers->size())){r.unknown=true;r.cause="network post-event Manager membership changed";return r;}
            if(fresh.managers)for(size_t k=0;k<fresh.managers->size();++k)if((*fresh.managers)[k].region!=(*before.managers)[k].region){r.unknown=true;r.cause="network post-event Manager RegionID changed";return r;}
            // Original listeners ran synchronously; their scalar results are the next expected values.
            before=std::move(fresh);
        }
        struct ScalarScope {
            Port& port;bool active=false;
            bool begin(std::string& e){if(active)return true;active=port.begin_scalar_stores(e);return active;}
            ~ScalarScope(){if(active)port.end_scalar_stores();}
        };
        {ScalarScope scalars{port};
        for(size_t i=0;i<before.nodes.size();++i){const auto& a=before.nodes[i];const auto& b=target.nodes[i];
            for(size_t field=0;field<6;++field){
                const uint32_t old=field<4?std::bit_cast<uint32_t>(a.values[field]):a.flags[field-4];
                const uint32_t value=field<4?std::bit_cast<uint32_t>(b.values[field]):b.flags[field-4];
                if(old==value)continue;
                if(!scalars.begin(r.cause)){r.unknown=r.attempted;return r;}
                r.attempted=true;++r.attempted_stores;
                if(!port.store(i,field,old,value,r.cause)){r.unknown=true;r.cause="network scalar store Unknown node="+std::to_string(i)+" field="+std::to_string(field)+" original="+r.cause;return r;}
                ++r.completed_stores;
            }
        }
        }
        for(size_t i=0;i<before.nodes.size();++i)if(before.nodes[i].flags[1]!=target.nodes[i].flags[1]){
            r.attempted=true;++r.notification_attempts;
            if(!port.notify_supply(i,r.cause)){r.unknown=true;r.cause="network original QueueUpdateBandwidthDisplay Unknown: "+r.cause;return r;}
            ++r.notifications;
        }
        for(size_t i=0;i<before.pipes.size();++i)if(before.pipes[i]!=target.pipes[i]){
            r.attempted=true;++r.pipe_attempts;
            r.pipe_effects.push_back({target.pipes[i].region,target.pipes[i].index});
            if(!port.store_pipe(i,before.pipes[i],target.pipes[i],r.pipe_effects.back(),r.cause)){
                r.unknown=true;r.cause="network pipe arrays/display Unknown index="+std::to_string(i)+" original="+r.cause;return r;}
            ++r.pipes_completed;
        }
        {ScalarScope scalars{port};
        for(size_t i=0;i<before.connections.size();++i)for(size_t field=0;field<7;++field){
            const auto old=std::bit_cast<uint32_t>(before.connections[i].values[field]);
            const auto value=std::bit_cast<uint32_t>(target.connections[i].values[field]);if(old==value)continue;
            if(!scalars.begin(r.cause)){r.unknown=r.attempted;return r;}
            r.attempted=true;++r.connection_attempts;
            if(!port.store_connection(i,field,old,value,r.cause)){r.unknown=true;r.cause="network connection scalar Unknown index="+std::to_string(i)+" field="+std::to_string(field)+" original="+r.cause;return r;}
            ++r.connection_stores;
        }
        }
        if(before.managers)for(size_t i=0;i<before.managers->size();++i)for(size_t field=0;field<2;++field){
            const auto old=(*before.managers)[i].values[field],value=(*target.managers)[i].values[field];if(old==value)continue;
            r.attempted=true;++r.attempted_stores;if(!port.store_manager(i,field,old,value,r.cause)){r.unknown=true;r.cause="network Manager scalar Unknown RegionID="+std::to_string((*target.managers)[i].region)+" field="+std::to_string(field)+" original="+r.cause;return r;}++r.completed_stores;}
        State actual;if(!port.read(actual,r.cause)||actual!=target){r.unknown=r.attempted;if(r.cause.empty())r.cause="network complete native readback differs";return r;}
        r.actual=std::move(actual);r.actual_present=true;r.confirmed=true;return r;
    }catch(const std::exception& x){r.cause=std::string("network apply exception original=")+x.what();r.unknown=r.attempted;return r;}
    catch(...){r.cause="network apply nonstandard exception";r.unknown=r.attempted;return r;}
}
}
