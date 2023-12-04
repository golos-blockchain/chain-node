#pragma once

#include <golos/chain/database.hpp>
#include <appbase/plugin.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <golos/plugins/json_rpc/plugin.hpp>
#include <golos/plugins/cryptor/cryptor_api_objects.hpp>
#include <golos/plugins/paid_subscription_api/paid_subscription_api.hpp>
#include <golos/plugins/social_network/social_network.hpp>

namespace golos { namespace plugins { namespace cryptor {

namespace bpo = boost::program_options;
using namespace golos::chain;

DEFINE_API_ARGS(encrypt_body, json_rpc::msg_pack, encrypted_api_object)
DEFINE_API_ARGS(decrypt_comments, json_rpc::msg_pack, decrypted_api_object)

class cryptor final : public appbase::plugin<cryptor> {
public:

    APPBASE_PLUGIN_REQUIRES((json_rpc::plugin)(chain::plugin)(social_network::social_network)(paid_subscription_api::paid_subscription_api_plugin))

    cryptor();

    virtual ~cryptor();

    void set_program_options(bpo::options_description& cli, bpo::options_description& cfg) override;

    void plugin_initialize(const bpo::variables_map& options) override;

    void plugin_startup() override;

    void plugin_shutdown() override;

    static const std::string& name();

    DECLARE_API(
        (encrypt_body)
        (decrypt_comments)
    )
private:
    class cryptor_impl;

    std::unique_ptr<cryptor_impl> my;
};

} } } // golos::plugins::cryptor
