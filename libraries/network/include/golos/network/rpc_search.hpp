#pragma once
#include <string>
#include <vector>
#include <deque>
#include <set>
#include <cstdint>
#include <sstream>
#include <functional>

namespace golos { namespace network {
// Transport-independent, finite search policy. Time is monotonic milliseconds supplied by owner.
// Only explicit numeric seeds may bypass public-address policy; referrals NEVER inherit seed trust.
class rpc_search {
public:
    enum class source { cache, bootstrap, referral };
private:
    // Reserved admission: no class can crowd either of the other two out.
    // The three bounds sum to the unchanged global 64 queued endpoints.
    std::deque<std::string> queues_[3];
    unsigned turn_ = 0; // Cached supporters retain the first attempt.
    std::set<std::string> seen_;
    unsigned attempts_ = 0, responses_ = 0;
    uint64_t next_at_ = 0;
    bool canceled_ = false;
    std::function<bool(const std::string&)> policy_;
public:
    explicit rpc_search(std::function<bool(const std::string&)> policy = public_endpoint) : policy_(std::move(policy)) {}
    static bool public_endpoint(const std::string& endpoint) {
        const auto colon = endpoint.find(':');
        if (colon == std::string::npos || !colon || endpoint[colon-1]=='.' || endpoint.size() > 21) return false;
        unsigned a[4] = {}, port = 0;
        std::istringstream ip(endpoint.substr(0, colon));
        for (int i=0; i<4; ++i) {
            std::string part;
            if (!std::getline(ip, part, '.') || part.empty() || part.size()>3 || (part.size()>1 && part[0]=='0')) return false;
            for (char c: part) { if (c<'0'||c>'9') return false; a[i]=a[i]*10+c-'0'; }
            if (a[i]>255) return false;
        }
        if (ip.peek()!=std::char_traits<char>::eof()) return false;
        const auto p=endpoint.substr(colon+1);
        if (p.empty() || p.size()>5 || (p.size()>1 && p[0]=='0')) return false;
        for(char c:p) { if(c<'0'||c>'9') return false; port=port*10+c-'0'; }
        if (!port || port>65535) return false;
        // Fail closed for special-use, documentation, benchmarking and multicast space.
        return a[0]!=0 && a[0]!=10 && a[0]!=127 && a[0]<224 &&
            !(a[0]==100 && a[1]>=64 && a[1]<=127) &&
            !(a[0]==169 && a[1]==254) && !(a[0]==172 && a[1]>=16 && a[1]<=31) &&
            !(a[0]==192 && (a[1]==0 || a[1]==168 || (a[1]==88 && a[2]==99))) &&
            !(a[0]==198 && (a[1]==18 || a[1]==19 || (a[1]==51 && a[2]==100))) &&
            !(a[0]==203 && a[1]==0 && a[2]==113);
    }
    bool add(const std::string& endpoint, bool trusted_explicit_numeric_seed=false, source origin=source::bootstrap) {
        const auto index=static_cast<unsigned>(origin);
        if (index>=3 || canceled_ ||
            (!(trusted_explicit_numeric_seed && origin==source::bootstrap) && !policy_(endpoint))) return false;
        if (seen_.count(endpoint)) {
            // Fresh valid referrals may prioritize pending work, never retry attempted work.
            // Keep its existing reservation/class, even when referral admission is full.
            if (origin==source::referral) for (auto& queue:queues_) {
                for (auto it=queue.begin(); it!=queue.end(); ++it) if (*it==endpoint) {
                    queue.erase(it); queue.push_front(endpoint); return true;
                }
            }
            return false;
        }
        if (queues_[index].size()>=(index==0 ? 22u : 21u) || seen_.size()>=128) return false;
        seen_.insert(endpoint); queues_[index].push_back(endpoint); return true;
    }
    void referrals(const std::vector<std::string>& endpoints) {
        if (canceled_ || endpoints.empty() || responses_++>=16) return;
        for (size_t i=0; i<endpoints.size() && i<64; ++i) add(endpoints[i],false,source::referral);
    }
    std::string next(uint64_t elapsed_ms) {
        if(canceled_ || attempts_>=16 || elapsed_ms>=60000 || elapsed_ms<next_at_ || !queued()) return {};
        for(unsigned i=0;i<3;++i) {
            const auto index=(turn_+i)%3;
            if(queues_[index].empty()) continue;
            ++attempts_; next_at_=elapsed_ms+250; turn_=(index+1)%3;
            auto endpoint=queues_[index].front(); queues_[index].pop_front(); return endpoint;
        }
        return {};
    }
    static std::string cache_encode(const std::vector<std::string>& endpoints, uint64_t now) {
        std::string out="supporters-v2\n"+std::to_string(now)+"\n";
        std::set<std::string> unique;
        for(const auto& ep:endpoints) if(unique.size()<64 && public_endpoint(ep) && unique.insert(ep).second) out+=ep+"\n";
        return out;
    }
    static std::vector<std::string> cache_decode(const std::string& bytes, uint64_t now) {
        if(bytes.size()>8192) return {};
        std::istringstream in(bytes); std::string line;
        if(!std::getline(in,line) || line!="supporters-v2") return {};
        if(!std::getline(in,line) || line.empty() || line.size()>12) return {};
        uint64_t stamp=0;
        for(char c:line) { if(c<'0'||c>'9') return {}; stamp=stamp*10+c-'0'; }
        if(stamp>now || now-stamp>3600) return {};
        std::vector<std::string> result; std::set<std::string> unique;
        while(std::getline(in,line)) {
            if(result.size()>=64 || !public_endpoint(line)) return {};
            if(unique.insert(line).second) result.push_back(line);
        }
        return result;
    }
    size_t queued() const { return queues_[0].size()+queues_[1].size()+queues_[2].size(); }
    void cancel() { canceled_=true; for(auto& queue:queues_) queue.clear(); }
};
}}
