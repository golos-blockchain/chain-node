#pragma once

#include <golos/chain/database.hpp>
#include <appbase/plugin.hpp>
#include <golos/plugins/chain/plugin.hpp>

namespace golos { namespace plugins { namespace event_plugin {

namespace bpo = boost::program_options;
using namespace golos::chain;

class event_plugin final : public appbase::plugin<event_plugin> {
public:

    APPBASE_PLUGIN_REQUIRES((chain::plugin))

    event_plugin();

    virtual ~event_plugin();

    void set_program_options(bpo::options_description& cli, bpo::options_description& cfg) override;

    void plugin_initialize(const bpo::variables_map& options) override;

    void plugin_startup() override;

    void plugin_shutdown() override;

    static const std::string& name();

private:
    class event_plugin_impl;

    std::unique_ptr<event_plugin_impl> my;
};

} } } // golos::plugins::event_plugin
