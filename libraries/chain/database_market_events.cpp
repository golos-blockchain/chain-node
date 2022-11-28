#include <golos/chain/database.hpp>
#include <golos/chain/account_object.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/protocol/exceptions.hpp>

namespace golos { namespace chain {

void database::push_order_create_event(const limit_order_object& order) {
	order_create_operation op;
	op.orderid = order.orderid;
	op.created = order.created;
	op.expiration = order.expiration;
	op.seller = order.seller;
	op.for_sale = asset(order.for_sale, order.symbol);
	op.sell_price = order.sell_price;
	push_event(op);
}

void database::push_order_delete_event(const limit_order_object& order) {
	order_delete_operation op;
	op.orderid = order.orderid;
	push_event(op);
}

} } // golos::chain
