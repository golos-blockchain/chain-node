#pragma once

namespace golos { namespace plugins { namespace market_history {

using callback_arg = fc::static_variant<
    order_create_operation,
    order_delete_operation,
    order_filled_operation
>;

}}} // golos::plugins::market_history

