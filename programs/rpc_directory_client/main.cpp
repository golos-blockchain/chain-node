#include <golos/network/discovery_delegate.hpp>
#include <golos/network/rpc_directory.hpp>
#include <golos/network/rpc_search.hpp>
#include <golos/network/rpc_cache_file.hpp>
#include <fc/network/resolve.hpp>
#include <chrono>
#include <fstream>
#include <fc/io/json.hpp>
#include <fc/thread/thread.hpp>
#include <csignal>
#include <iostream>

namespace { volatile std::sig_atomic_t stopped = 0;
void stop(int) { stopped = 1; }
std::uint64_t now_seconds() { return fc::time_point::now().sec_since_epoch(); }
}
int main(int argc, char** argv) {
    const bool search_mode = argc>1 && std::string(argv[1])=="--search";
    if (argc < 3 || (!search_mode && argc>4) || (search_mode && argc<4)) {
        std::cerr << "Usage: golos-rpc-directory-client IP:PORT CONFIG_DIR [SECONDS=5]\n"
                     "       golos-rpc-directory-client --search CONFIG_DIR SECONDS [--seed HOST:PORT ...] [--seed-file FILE]\nResults are UNVERIFIED.\n";
        return 2;
    }
    try {
        const std::string duration = argc >= 4 ? argv[3] : "5";
        std::size_t consumed = 0;
        const auto seconds = std::stoul(duration, &consumed);
        if (consumed != duration.size() || !seconds || seconds > 60) return 2;
        const auto started=std::chrono::steady_clock::now();
        const auto deadline=started+std::chrono::seconds(seconds);
        std::signal(SIGINT, stop); std::signal(SIGTERM, stop);
        golos::network::rpc_search search;
        std::vector<std::string> seeds;
        for(int i=4;i<argc;++i) {
            std::string flag=argv[i]; if(++i>=argc) return 2;
            if(flag=="--seed") seeds.emplace_back(argv[i]);
            else if(flag=="--seed-file") {
                std::ifstream file(argv[i]); if(!file) return 2;
                std::string line;
                char ch;
                while(seeds.size()<64 && file.get(ch)) {
                    if(ch=='\n') { if(!line.empty()) seeds.push_back(line); line.clear(); }
                    else { if(line.size()>=253) return 2; line.push_back(ch); }
                }
                if(!line.empty() && seeds.size()<64) seeds.push_back(line);
            } else return 2;
            if(seeds.size()>64) return 2;
        }
        if(search_mode && argc>4 && seeds.empty()) return 2; // An explicit empty seed file never enables public defaults.
        if(!search_mode) seeds.push_back(argv[1]);
        // Authentic ordinary seeds pinned from share/golosd/seednodes. Explicit seeds override these.
        if(seeds.empty()) seeds={"65.109.83.252:4243","23.88.107.61:2001","116.202.30.93:2001","167.235.139.111:2001","95.216.200.20:3001","65.21.6.225:2000","65.108.213.75:4243","65.109.200.83:4243","91.107.161.243:4243","95.217.228.80:4243","95.217.59.180:30218","37.192.123.64:9243"};
        std::vector<std::string> cache_candidates;
        const auto cache_path=fc::path(argv[2])/"rpc_search_supporters.v2";
        if(search_mode) try {
            std::ifstream file(cache_path.to_native_ansi_path(),std::ios::binary);
            std::string bytes(8193,'\0'); file.read(&bytes[0],bytes.size()); bytes.resize(file.gcount());
            for(const auto& ep:golos::network::rpc_search::cache_decode(bytes,now_seconds())) search.add(ep,false,golos::network::rpc_search::source::cache);
        } catch(...) { /* Optional cache never grants RPC trust. */ }
        for(const auto& seed:seeds) {
            if(seed.size()>253) return 2;
            fc::optional<fc::ip::endpoint> numeric;
            try { numeric=fc::ip::endpoint::from_string(seed); }
            catch(const fc::exception&) { /* FC forbids yielding inside a catch: resolve below. */ }
            if(numeric) { if(!numeric->port()) return 2; search.add(std::string(*numeric),true); }
            else {
                auto colon=seed.find(':'); if(colon==std::string::npos) return 2;
                size_t used=0; auto port=std::stoul(seed.substr(colon+1),&used);
                if(!port || port>65535 || used!=seed.size()-colon-1) return 2;
                auto resolution=fc::async([seed,colon,port] { return fc::resolve(seed.substr(0,colon),port); },"catalog bootstrap DNS");
                while(!resolution.ready() && !stopped && std::chrono::steady_clock::now()<deadline) fc::usleep(fc::milliseconds(25));
                if(!resolution.ready()) { resolution.cancel_and_wait(); break; }
                try {
                    auto answers=resolution.wait();
                    for(size_t i=0;i<answers.size() && i<64;++i) search.add(std::string(answers[i]));
                } catch(const fc::exception&) { /* Failed bootstrap must not prevent later seeds. */ }
            }
        }

        golos::network::rpc_directory::catalog catalog;
        golos::network::discovery_delegate delegate;
        golos::network::node node("golos-rpc-directory-client/1");
        std::signal(SIGINT, stop);
        std::signal(SIGTERM, stop);
        node.load_configuration(fc::path(argv[2]));
        node.set_node_delegate(&delegate);
        node.set_discovery_only(true); // MUST precede every connection/listener.
        node.accept_incoming_connections(false);
        node.disable_peer_advertising();
        node.set_hello_extension(golos::network::rpc_directory::hello_key,
            [&catalog] { return catalog.hello(now_seconds()); },
            {});
        node.set_hello_endpoint_observer([&catalog,&cache_candidates](const std::string& peer, const std::string& endpoint, const std::string& wire) {
            if(catalog.receive(peer, wire, now_seconds()) && cache_candidates.size()<64 &&
                golos::network::rpc_search::public_endpoint(endpoint) &&
                std::find(cache_candidates.begin(),cache_candidates.end(),endpoint)==cache_candidates.end())
                cache_candidates.push_back(endpoint);
        });
        node.connect_to_p2p_network(); // Discovery mode starts no fetch/advertisement/reconnect loops.
        std::vector<std::pair<std::string,std::chrono::steady_clock::time_point>> active;
        while (!stopped && std::chrono::steady_clock::now() < deadline) {
            auto now=std::chrono::steady_clock::now();
            auto addresses=node.take_discovery_addresses();
            if(search_mode && !addresses.empty()) {
                search.referrals(addresses);
            }
            for(auto it=active.begin();it!=active.end();) {
                if(now>=it->second) { node.disconnect_discovery_peer(fc::ip::endpoint::from_string(it->first)); it=active.erase(it); }
                else ++it;
            }
            if(active.size()<2) {
                auto next=search.next(std::chrono::duration_cast<std::chrono::milliseconds>(now-started).count());
                if(!next.empty()) try {
                    node.connect_to_endpoint(fc::ip::endpoint::from_string(next));
                    active.emplace_back(next,now+std::chrono::seconds(2));
                } catch(const fc::exception&) { /* Attempt is consumed, no retry. */ }
            }
            fc::usleep(fc::milliseconds(25));
        }
        search.cancel();
        node.close(); // Synchronously clears callbacks and closes owned sockets before catalog destruction.
        if(search_mode && !stopped && !cache_candidates.empty()) try {
            fc::temp_file pending(fc::path(argv[2]),true);
            std::ofstream file(pending.path().to_native_ansi_path(),std::ios::binary|std::ios::trunc);
            file << golos::network::rpc_search::cache_encode(cache_candidates,now_seconds());
            file.close();
            if(file) {
#ifdef _WIN32
                golos::network::atomic_replace(pending.path().preferred_wstring(),cache_path.preferred_wstring());
#else
                golos::network::atomic_replace(pending.path().to_native_ansi_path(),cache_path.to_native_ansi_path());
#endif
            }
        } catch(...) { /* Cache failure cannot alter the search result. */ }
        if (stopped) return 130;
        fc::variants entries;
        for (const auto& r : catalog.snapshot(now_seconds())) {
            entries.emplace_back(fc::mutable_variant_object("url", r.url)("expires", r.expires)("trust", "UNVERIFIED_PEER_ANNOUNCEMENT"));
        }
        std::cout << fc::json::to_string(fc::variant(entries)) << '\n';
        return entries.empty() ? 3 : 0;
    } catch (const fc::exception& error) {
        std::cerr << "P2P catalog search failed: " << error.to_detail_string().substr(0,512) << '\n';
    } catch (const std::exception&) {
        std::cerr << "Invalid search configuration\n";
    }
    return 1;
}
