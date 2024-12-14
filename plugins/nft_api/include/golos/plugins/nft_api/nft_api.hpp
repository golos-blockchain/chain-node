#pragma once

#include <golos/chain/database.hpp>
#include <appbase/plugin.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <golos/plugins/json_rpc/plugin.hpp>
#include <golos/plugins/nft_api/nft_api_objects.hpp>

namespace golos { namespace plugins { namespace nft_api {

namespace bpo = boost::program_options;
using namespace golos::chain;

DEFINE_API_ARGS(get_nft_collections, json_rpc::msg_pack, std::vector<nft_collection_api_object>)
DEFINE_API_ARGS(get_nft_tokens, json_rpc::msg_pack, std::vector<nft_extended_api_object>)
DEFINE_API_ARGS(get_nft_orders, json_rpc::msg_pack, std::vector<nft_order_api_object>)
DEFINE_API_ARGS(get_nft_bets, json_rpc::msg_pack, std::vector<nft_extended_bet_api_object>)

class nft_api_plugin final : public appbase::plugin<nft_api_plugin> {
public:

    APPBASE_PLUGIN_REQUIRES((json_rpc::plugin)(chain::plugin))

    nft_api_plugin();

    virtual ~nft_api_plugin();

    void set_program_options(bpo::options_description& cli, bpo::options_description& cfg) override;

    void plugin_initialize(const bpo::variables_map& options) override;

    void plugin_startup() override;

    void plugin_shutdown() override;

    static const std::string& name();

    DECLARE_API(
        (get_nft_collections)
        (get_nft_tokens)
        (get_nft_orders)
        (get_nft_bets)
    )
private:
    class nft_api_plugin_impl;

    std::unique_ptr<nft_api_plugin_impl> my;
};

} } } // golos::plugins::nft_api
