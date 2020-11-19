#pragma once

#include <appbase/plugin.hpp>
#include <golos/chain/database.hpp>
#include <golos/plugins/account_notes/account_notes_api_objects.hpp>

#include <golos/plugins/json_rpc/plugin.hpp>

namespace golos { namespace plugins { namespace account_notes {

    using namespace golos::chain;

    DEFINE_API_ARGS(get_value, json_rpc::msg_pack, std::string)
    DEFINE_API_ARGS(get_values_settings, json_rpc::msg_pack, account_notes_settings_api_object)

    /**
     *   This plugin provides the support of key-value storage for additional data for the accounts.
     *
     */
    class account_notes_plugin final : public appbase::plugin<account_notes_plugin> {
    public:
        APPBASE_PLUGIN_REQUIRES((json_rpc::plugin))

        account_notes_plugin();

        ~account_notes_plugin();

        void set_program_options(
            boost::program_options::options_description& cli,
            boost::program_options::options_description& cfg) override;

        void plugin_initialize(const boost::program_options::variables_map& options) override;

        void plugin_startup() override;

        void plugin_shutdown() override;

        static const std::string& name();

        DECLARE_API(
            (get_value)
            (get_values_settings)
        )

    private:
        class account_notes_plugin_impl;

        std::unique_ptr<account_notes_plugin_impl> my;
    };

} } } //golos::plugins::account_notes
