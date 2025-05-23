#pragma once

#include <golos/chain/database.hpp>
#include <appbase/plugin.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <golos/plugins/json_rpc/plugin.hpp>

namespace golos { namespace plugins { namespace exchange {

namespace bpo = boost::program_options;
using namespace golos::chain;

DEFINE_API_ARGS(get_exchange, json_rpc::msg_pack, fc::mutable_variant_object)

class exchange final : public appbase::plugin<exchange> {
public:

    APPBASE_PLUGIN_REQUIRES((json_rpc::plugin)(chain::plugin))

    exchange();

    virtual ~exchange();

    void set_program_options(bpo::options_description& cli, bpo::options_description& cfg) override;

    void plugin_initialize(const bpo::variables_map& options) override;

    void plugin_startup() override;

    void plugin_shutdown() override;

    static const std::string& name();

    DECLARE_API(
        (get_exchange)
    )
private:
    class exchange_impl;

    std::unique_ptr<exchange_impl> my;
};

} } } // golos::plugins::exchange
