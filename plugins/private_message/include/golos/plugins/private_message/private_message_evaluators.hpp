#pragma once

#include <golos/plugins/private_message/private_message_operations.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/evaluator.hpp>

namespace golos { namespace plugins { namespace private_message {

    class private_message_plugin;

    using golos::chain::evaluator_impl;
    using golos::chain::database;

    static inline time_point_sec min_create_date() {
        return time_point_sec(1);
    }

    class private_message_evaluator final:
        public evaluator_impl<private_message_evaluator, private_message_plugin_operation>
    {
    public:
        using operation_type = private_message_operation;

        private_message_evaluator(database& db, private_message_plugin* plugin)
            : evaluator_impl<private_message_evaluator, private_message_plugin_operation>(db),
            _plugin(plugin) {
        }

        void do_apply(const private_message_operation& op);

        private_message_plugin* _plugin;
    };

    class private_delete_message_evaluator final:
        public evaluator_impl<private_delete_message_evaluator, private_message_plugin_operation>
    {
    public:
        using operation_type = private_delete_message_operation;

        private_delete_message_evaluator(database& db, private_message_plugin* plugin)
            : evaluator_impl<private_delete_message_evaluator, private_message_plugin_operation>(db),
            _plugin(plugin) {
        }

        void do_apply(const private_delete_message_operation& op);

        private_message_plugin* _plugin;
    };

    class private_mark_message_evaluator final:
        public evaluator_impl<private_mark_message_evaluator, private_message_plugin_operation>
    {
    public:
        using operation_type = private_mark_message_operation;

        private_mark_message_evaluator(database& db, private_message_plugin* plugin)
            : evaluator_impl<private_mark_message_evaluator, private_message_plugin_operation>(db),
            _plugin(plugin) {
        }

        void do_apply(const private_mark_message_operation& op);

        private_message_plugin* _plugin;
    };

    class private_settings_evaluator final:
        public evaluator_impl<private_settings_evaluator, private_message_plugin_operation>
    {
    public:
        using operation_type = private_settings_operation;

        private_settings_evaluator(database& db, private_message_plugin* plugin)
            : evaluator_impl<private_settings_evaluator, private_message_plugin_operation>(db),
            _plugin(plugin) {
        }

        void do_apply(const private_settings_operation& op);

        private_message_plugin* _plugin;
    };

    class private_contact_evaluator final:
        public evaluator_impl<private_contact_evaluator, private_message_plugin_operation>
    {
    public:
        using operation_type = private_contact_operation;

        private_contact_evaluator(database& db, private_message_plugin* plugin)
            : evaluator_impl<private_contact_evaluator, private_message_plugin_operation>(db),
            _plugin(plugin) {
        }

        void do_apply(const private_contact_operation& op);

        private_message_plugin* _plugin;
    };

} } }
