#pragma once
#include <golos/plugins/p2p/p2p_plugin.hpp>
#include <golos/network/rpc_directory.hpp>

namespace golos { namespace plugins { namespace rpc_directory {
class plugin final : public appbase::plugin<plugin> {
    std::shared_ptr<network::rpc_directory::catalog> catalog_;
public:
    APPBASE_PLUGIN_REQUIRES((p2p::p2p_plugin))
    plugin();
    static const std::string& name() { static const std::string n = "rpc_directory"; return n; }
    void set_program_options(boost::program_options::options_description& cli,
                             boost::program_options::options_description& cfg) override;
    void plugin_initialize(const boost::program_options::variables_map& options) override;
    void plugin_startup() override {}
    void plugin_shutdown() override;
    // Bounded read-only copies. No RPC listener, URL connection or automatic trust selection.
    std::vector<network::rpc_directory::record> get_directory() const;
};
}}}
