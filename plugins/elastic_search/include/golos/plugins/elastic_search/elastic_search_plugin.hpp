#pragma once

#include <golos/chain/database.hpp>
#include <appbase/plugin.hpp>
#include <golos/plugins/chain/plugin.hpp>

namespace golos { namespace plugins { namespace elastic_search {

namespace bpo = boost::program_options;
using namespace golos::chain;

class elastic_search_plugin final : public appbase::plugin<elastic_search_plugin> {
public:

    APPBASE_PLUGIN_REQUIRES((chain::plugin))

    elastic_search_plugin();

    virtual ~elastic_search_plugin();

    void set_program_options(bpo::options_description& cli, bpo::options_description& cfg) override;

    void plugin_initialize(const bpo::variables_map& options) override;

    void plugin_startup() override;

    void plugin_shutdown() override;

    static const std::string& name();

private:
    class elastic_search_plugin_impl;

    std::unique_ptr<elastic_search_plugin_impl> my;
};

} } } // golos::plugins::elastic_search
