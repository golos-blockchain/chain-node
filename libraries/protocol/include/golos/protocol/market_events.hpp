#pragma once

#include <golos/protocol/base.hpp>
#include <golos/protocol/block_header.hpp>
#include <golos/protocol/asset.hpp>
#include <golos/protocol/worker_operations.hpp>

#include <fc/utf8.hpp>

namespace golos { namespace protocol {

struct order_create_operation : public virtual_operation {
	order_create_operation() {
	}

	order_create_operation(uint32_t oid, fc::time_point_sec c, fc::time_point_sec e,
		const account_name_type& s, const price& sp) :
		orderid(oid), created(c), expiration(e), seller(s), sell_price(sp) {
	}

    uint32_t orderid = 0;
    fc::time_point_sec created;
    fc::time_point_sec expiration;
    account_name_type seller;
    asset for_sale;
    price sell_price; // TODO: correct?
    // TODO: fee
};

struct order_delete_operation : public virtual_operation {
	order_delete_operation() {
	}

	order_delete_operation(uint32_t oid, const account_name_type& s, const price& sp) :
		orderid(oid), seller(s), sell_price(sp) {
	}

    uint32_t orderid = 0;
    account_name_type seller;
    price sell_price;
};

struct order_filled_operation : public virtual_operation {
	order_filled_operation() {
	}

	order_filled_operation(uint32_t oid, const account_name_type& s, const price& sp) :
		orderid(oid), seller(s), sell_price(sp) {
	}

    uint32_t orderid = 0;
    account_name_type seller;
    price sell_price;
    // TODO: more info )
};

} } //golos::protocol

FC_REFLECT((golos::protocol::order_create_operation),
    (orderid)(created)(expiration)(seller)(for_sale)(sell_price)
)

FC_REFLECT((golos::protocol::order_delete_operation),
    (orderid)(seller)(sell_price)
)

FC_REFLECT((golos::protocol::order_filled_operation),
    (orderid)(seller)(sell_price)
)