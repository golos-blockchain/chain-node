#include <golos/api/block_objects.hpp>

namespace golos { namespace api {

block_operation::block_operation() = default;

annotated_signed_block::annotated_signed_block() = default;

annotated_signed_block::annotated_signed_block(const signed_block& block)
        : signed_block(block) {
    block_id = id();
    signing_key = signee();
    transaction_ids.reserve(transactions.size());
    for (const signed_transaction& tx : transactions) {
        transaction_ids.push_back(tx.id());
    }
}

annotated_signed_block::annotated_signed_block(const signed_block& block, const block_operations& ops)
        : annotated_signed_block(block) {
    _virtual_operations = ops;
}

timed_signed_block::timed_signed_block() = default;

timed_signed_block::timed_signed_block(const signed_block& block)
        : signed_block(block) {
    timestamp_msec = time_point(block.timestamp).time_since_epoch().to_milliseconds();
}

timed_block_header::timed_block_header() = default;

timed_block_header::timed_block_header(const block_header& bh)
        : block_header(bh) {
    timestamp_msec = time_point(bh.timestamp).time_since_epoch().to_milliseconds();
}

} } // golos::api