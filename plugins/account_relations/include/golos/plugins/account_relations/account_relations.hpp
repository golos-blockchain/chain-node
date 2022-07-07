#pragma once

#include <golos/chain/database.hpp>
#include <appbase/plugin.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <golos/plugins/json_rpc/plugin.hpp>

namespace golos { namespace plugins { namespace account_relations {

namespace bpo = boost::program_options;
using namespace golos::chain;

DEFINE_API_ARGS(list_account_relations, json_rpc::msg_pack, fc::mutable_variant_object)
DEFINE_API_ARGS(get_account_relations, json_rpc::msg_pack, fc::mutable_variant_object)

class account_relations final : public appbase::plugin<account_relations> {
public:

    APPBASE_PLUGIN_REQUIRES((json_rpc::plugin)(chain::plugin))

    account_relations();

    virtual ~account_relations();

    void set_program_options(bpo::options_description& cli, bpo::options_description& cfg) override;

    void plugin_initialize(const bpo::variables_map& options) override;

    void plugin_startup() override;

    void plugin_shutdown() override;

    static const std::string& name();

    DECLARE_API(
        (list_account_relations)
        (get_account_relations)
    )
private:
    class impl;

    std::unique_ptr<impl> my;
};

} } } // golos::plugins::account_relations
