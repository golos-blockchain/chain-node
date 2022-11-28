#include <golos/plugins/market_history/market_history_visitor.hpp>

#define NOTIFY_SIGNAL(signal, ...)                                                   \
   try                                                                               \
   {                                                                                 \
      signal( __VA_ARGS__ );                                                         \
   }                                                                                 \
   catch (const fc::exception& e)                                                    \
   {                                                                                 \
      elog("Caught exception in market signals: ${e}", ("e", e.to_detail_string())); \
   }                                                                                 \
   catch (...)                                                                       \
   {                                                                                 \
      elog("Caught unexpected exception in market signals");                         \
   }

namespace golos { namespace plugins { namespace market_history {

    operation_visitor::operation_visitor(market_history_plugin& plugin, database& db)
        : _plugin(plugin), _db(db) {
    }

    void operation_visitor::operator()(const order_create_operation& op) const {
         callback_arg arg = op;
         NOTIFY_SIGNAL(_plugin.create_order_signal, arg)
    }

    void operation_visitor::operator()(const order_delete_operation& op) const {
         callback_arg arg = op;
         NOTIFY_SIGNAL(_plugin.create_order_signal, arg)
    }

} } } // golos::plugins::market_history
