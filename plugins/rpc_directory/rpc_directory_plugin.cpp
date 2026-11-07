#include <golos/plugins/rpc_directory/rpc_directory_plugin.hpp>

namespace golos { namespace plugins { namespace rpc_directory {
namespace bpo = boost::program_options;
static std::uint64_t now_seconds() { return fc::time_point::now().sec_since_epoch(); }
plugin::plugin() = default;
void plugin::set_program_options(bpo::options_description&, bpo::options_description& cfg) {
    cfg.add_options()
        ("rpc-directory-announce", bpo::value<std::vector<std::string>>()->composing(),
         "Explicit public HTTPS/WSS RPC URL to announce (at most 32; never inferred or contacted)")
        ("rpc-directory-ttl", bpo::value<uint32_t>()->default_value(900), "Announcement lifetime seconds (1..3600)");
}
void plugin::plugin_initialize(const bpo::variables_map& options) {
    const auto ttl = options.at("rpc-directory-ttl").as<uint32_t>();
    FC_ASSERT(ttl >= 1 && ttl <= 3600);
    std::vector<std::string> urls;
    if (options.count("rpc-directory-announce")) urls = options.at("rpc-directory-announce").as<std::vector<std::string>>();
    FC_ASSERT(urls.size() <= 32);
    for (const auto& url : urls) FC_ASSERT(network::rpc_directory::catalog::public_url(url), "Invalid RPC directory URL");
    catalog_ = std::make_shared<network::rpc_directory::catalog>();
    const auto catalog = catalog_;
    appbase::app().get_plugin<p2p::p2p_plugin>().set_hello_extension(network::rpc_directory::hello_key,
        [catalog, urls, ttl] {
            const auto now = now_seconds();
            for (const auto& url : urls) catalog->announce(url, now + ttl, now);
            return catalog->hello(now);
        },
        [catalog](const std::string& peer, const std::string& wire) { catalog->receive(peer, wire, now_seconds()); });
}
void plugin::plugin_shutdown() {
    appbase::app().get_plugin<p2p::p2p_plugin>().set_hello_extension("", {}, {});
}
std::vector<network::rpc_directory::record> plugin::get_directory() const {
    if (!catalog_) return {};
    return catalog_->snapshot(now_seconds());
}
}}}
