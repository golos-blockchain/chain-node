#pragma once

#include <golos/chain/account_object.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/operation_notification.hpp>
#include <golos/chain/steem_object_types.hpp>
#include <golos/protocol/types.hpp>

namespace golos { namespace api {

using namespace golos::protocol;
using golos::chain::operation_notification;

// block_operation used in block_applied_callback to represent virtual operations.
// default operation type have no position info (trx, op_in_trx)
struct block_operation {

    block_operation();

    block_operation(const operation_notification& o) :
        trx_in_block(o.trx_in_block),
        op_in_trx(o.op_in_trx),
        virtual_op(o.virtual_op),
        op(o.op) {};

    uint32_t trx_in_block = 0;
    uint16_t op_in_trx = 0;
    uint32_t virtual_op = 0;
    operation op;
};

using block_operations = std::vector<block_operation>;

struct annotated_signed_block : public signed_block {

    annotated_signed_block();

    annotated_signed_block(const signed_block& block);

    annotated_signed_block(const signed_block& block, const block_operations& ops);

    annotated_signed_block(const annotated_signed_block& block) = default;

    block_id_type block_id;
    public_key_type signing_key;
    vector<transaction_id_type> transaction_ids;

    // name field starting with _ coz it's not directly related to block
    optional<block_operations> _virtual_operations;
};

struct timed_signed_block : public signed_block {
    timed_signed_block();

    timed_signed_block(const signed_block& block);

    timed_signed_block(const timed_signed_block& block) = default;

    uint64_t timestamp_msec;
    uint64_t request_time_msec = time_point::now().time_since_epoch().to_milliseconds();
};

struct timed_block_header : public block_header {
    timed_block_header();

    timed_block_header(const block_header& bh);

    timed_block_header(const timed_block_header& bh);

    uint64_t timestamp_msec;
    uint64_t request_time_msec = time_point::now().time_since_epoch().to_milliseconds();
};

} } // golos::api


FC_REFLECT(
    (golos::api::block_operation),
    (trx_in_block)(op_in_trx)(virtual_op)(op)
)

FC_REFLECT_DERIVED(
    (golos::api::annotated_signed_block), ((golos::chain::signed_block)),
    (block_id)(signing_key)(transaction_ids)(_virtual_operations)
)

FC_REFLECT_DERIVED(
    (golos::api::timed_signed_block), ((golos::chain::signed_block)),
    (timestamp_msec)(request_time_msec)
)

FC_REFLECT_DERIVED(
    (golos::api::timed_block_header), ((golos::chain::block_header)),
    (timestamp_msec)(request_time_msec)
)
