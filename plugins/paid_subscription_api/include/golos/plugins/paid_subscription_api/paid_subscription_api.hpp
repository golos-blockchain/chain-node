#pragma once

#include <golos/chain/database.hpp>
#include <golos/chain/paid_subscription_objects.hpp>
#include <appbase/plugin.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <golos/plugins/json_rpc/plugin.hpp>
#include <golos/plugins/paid_subscription_api/paid_subscription_api_objects.hpp>

namespace golos { namespace plugins { namespace paid_subscription_api {

namespace bpo = boost::program_options;
using namespace golos::chain;

DEFINE_API_ARGS(get_paid_subscriptions_by_author, json_rpc::msg_pack, std::vector<paid_subscription_object>)
DEFINE_API_ARGS(get_paid_subscription_options, json_rpc::msg_pack, paid_subscription_object)
DEFINE_API_ARGS(get_paid_subscribers, json_rpc::msg_pack, std::vector<paid_subscriber_object>)
DEFINE_API_ARGS(get_paid_subscriptions, json_rpc::msg_pack, std::vector<paid_subscriber_object>)
DEFINE_API_ARGS(get_paid_subscribe, json_rpc::msg_pack, paid_subscribe_result)

class paid_subscription_api_plugin final : public appbase::plugin<paid_subscription_api_plugin> {
public:

    APPBASE_PLUGIN_REQUIRES((json_rpc::plugin)(chain::plugin))

    paid_subscription_api_plugin();

    virtual ~paid_subscription_api_plugin();

    void set_program_options(bpo::options_description& cli, bpo::options_description& cfg) override;

    void plugin_initialize(const bpo::variables_map& options) override;

    void plugin_startup() override;

    void plugin_shutdown() override;

    static const std::string& name();

    DECLARE_API(
        (get_paid_subscriptions_by_author)
        (get_paid_subscription_options)
        (get_paid_subscribers)
        (get_paid_subscriptions)
        (get_paid_subscribe)
    )
private:
    class paid_subscription_api_plugin_impl;

    std::unique_ptr<paid_subscription_api_plugin_impl> my;
};

} } } // golos::plugins::paid_subscription_api
