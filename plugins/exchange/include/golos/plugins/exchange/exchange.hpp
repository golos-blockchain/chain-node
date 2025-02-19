#pragma once

#include <golos/chain/database.hpp>
#include <appbase/plugin.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <golos/plugins/json_rpc/plugin.hpp>

#define GOLOS_SEARCH_TIMEOUT 1500

#define GOLOS_MIN_DISCRETE_STEP 200 // 2.00%

namespace golos { namespace plugins { namespace exchange {

namespace bpo = boost::program_options;
using namespace golos::chain;

using value_path = std::vector<std::string>;

struct exchange_path {
    std::vector<value_path> paths;
};

DEFINE_API_ARGS(get_exchange, json_rpc::msg_pack, fc::mutable_variant_object)
DEFINE_API_ARGS(get_exchange_path, json_rpc::msg_pack, exchange_path)

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
        (get_exchange_path)
    )
private:
    class exchange_impl;

    std::unique_ptr<exchange_impl> my;

    uint16_t hybrid_discrete_step = GOLOS_MIN_DISCRETE_STEP;
};

} } } // golos::plugins::exchange

FC_REFLECT(
    (golos::plugins::exchange::exchange_path),
    (paths)
)
