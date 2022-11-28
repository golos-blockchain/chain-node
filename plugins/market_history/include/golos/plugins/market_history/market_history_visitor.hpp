#pragma once

#include <golos/chain/database.hpp>
#include <boost/algorithm/string.hpp>
#include <golos/plugins/market_history/market_history_plugin.hpp>

namespace golos { namespace plugins { namespace market_history {
    using namespace golos::chain;

    struct operation_visitor {
        operation_visitor(market_history_plugin& plugin, database& db);
        using result_type = void;

        market_history_plugin& _plugin;

        database& _db;

        void operator()(const order_create_operation& op) const;

        void operator()(const order_delete_operation& op) const;

        template<typename Op>
        void operator()(Op&&) const {
        } /// ignore all other ops
    };

} } } // golos::plugins::market_history
