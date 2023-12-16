#pragma once

#include <golos/plugins/private_message/private_message_operations.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/evaluator.hpp>

#define DEFINE_PLUGIN_EVALUATOR(X) \
class X ## _evaluator : public golos::chain::evaluator_impl< X ## _evaluator, private_message_plugin_operation>                     \
{                                                                                                  \
   public:                                                                                         \
      typedef X ## _operation operation_type;                                                      \
                                                                                                   \
      X ## _evaluator( database& db, private_message_plugin* plugin)                               \
         : golos::chain::evaluator_impl< X ## _evaluator, private_message_plugin_operation>( db ), \
            _plugin(plugin) {                                                                      \
      }                                                                                            \
                                                                                                   \
      void do_apply( const X ## _operation& o );                                                   \
                                                                                                   \
      private_message_plugin* _plugin;                                                             \
};

namespace golos { namespace plugins { namespace private_message {

    class private_message_plugin;

    using golos::chain::evaluator_impl;
    using golos::chain::database;

    static inline time_point_sec min_create_date() {
        return time_point_sec(1);
    }

    DEFINE_PLUGIN_EVALUATOR(private_message)
    DEFINE_PLUGIN_EVALUATOR(private_delete_message)
    DEFINE_PLUGIN_EVALUATOR(private_mark_message)
    DEFINE_PLUGIN_EVALUATOR(private_settings)
    DEFINE_PLUGIN_EVALUATOR(private_contact)
    DEFINE_PLUGIN_EVALUATOR(private_group)
    DEFINE_PLUGIN_EVALUATOR(private_group_delete)
    DEFINE_PLUGIN_EVALUATOR(private_group_member)

} } } // golos::plugins::private_message
