#pragma once
#include <golos/plugins/private_message/private_message_operations.hpp>
#include <golos/plugins/private_message/private_message_api_objects.hpp>

#include <appbase/plugin.hpp>
#include <golos/chain/database.hpp>

#include <golos/plugins/json_rpc/utility.hpp>
#include <golos/plugins/json_rpc/plugin.hpp>

#include <boost/multi_index/composite_key.hpp>

#include <fc/thread/future.hpp>
#include <fc/api.hpp>

namespace golos { namespace plugins { namespace private_message {
    using namespace golos::chain;

    DEFINE_API_ARGS(get_inbox,         json_rpc::msg_pack, std::vector<message_api_object>)
    DEFINE_API_ARGS(get_outbox,        json_rpc::msg_pack, std::vector<message_api_object>)
    DEFINE_API_ARGS(get_thread,        json_rpc::msg_pack, std::vector<message_api_object>)
    DEFINE_API_ARGS(get_settings ,     json_rpc::msg_pack, settings_api_object)
    DEFINE_API_ARGS(get_contact_info,  json_rpc::msg_pack, contact_api_object)
    DEFINE_API_ARGS(get_contacts_size, json_rpc::msg_pack, contacts_size_api_object)
    DEFINE_API_ARGS(get_contacts,      json_rpc::msg_pack, std::vector<contact_api_object>)
    DEFINE_API_ARGS(set_callback,      json_rpc::msg_pack, json_rpc::void_type)

    /**
     *   This plugin scans the blockchain for custom operations containing a valid message and authorized
     *   by the posting key.
     *
     */
    class private_message_plugin final : public appbase::plugin<private_message_plugin> {
    public:
        APPBASE_PLUGIN_REQUIRES((json_rpc::plugin))

        private_message_plugin();

        ~private_message_plugin();

        void set_program_options(
            boost::program_options::options_description& cli,
            boost::program_options::options_description& cfg) override;

        void plugin_initialize(const boost::program_options::variables_map& options) override;

        void plugin_startup() override;

        void plugin_shutdown() override;

        static const std::string& name();

        bool is_tracked_account(const account_name_type& name);

        bool can_call_callbacks();

        void call_callbacks(
            callback_event_type event, const account_name_type& from, const account_name_type& to, fc::variant r);

        DECLARE_API(
            (get_inbox)
            (get_outbox)
            (get_thread)
            (get_settings)
            (get_contact_info)
            (get_contacts_size)
            (get_contacts)
            (set_callback)
        )

    private:
        class private_message_plugin_impl;
        friend class private_message_plugin_impl;

        std::unique_ptr<private_message_plugin_impl> my;
    };

} } } //golos::plugins::private_message
