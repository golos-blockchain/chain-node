#pragma once

#include <golos/protocol/authority.hpp>
#include <golos/protocol/operations.hpp>
#include <golos/protocol/steem_operations.hpp>

#include <chainbase/chainbase.hpp>

#include <golos/chain/index.hpp>
#include <golos/chain/steem_object_types.hpp>

#include <boost/multi_index/composite_key.hpp>

//
// Plugins should #define their SPACE_ID's so plugins with
// conflicting SPACE_ID assignments can be compiled into the
// same binary (by simply re-assigning some of the conflicting #defined
// SPACE_ID's in a build script).
//
// Assignment of SPACE_ID's cannot be done at run-time because
// various template automagic depends on them being known at compile
// time.
//

#ifndef EVENT_SPACE_ID
#define EVENT_SPACE_ID 16
#endif


namespace golos { namespace plugins { namespace event_plugin {

    enum event_object_types {
        op_note_object_type = (EVENT_SPACE_ID << 8),
    };

    using namespace golos::chain;
    using namespace chainbase;

    class op_note_object final: public object<op_note_object_type, op_note_object> {
    public:
        op_note_object() = delete;

        template<typename Constructor, typename Allocator>
        op_note_object(Constructor&& c, allocator <Allocator> a)
            : serialized_op(a.get_segment_manager()) {
            c(*this);
        }

        id_type id;

        transaction_id_type trx_id;
        uint32_t block = 0;
        uint32_t trx_in_block = 0;
        uint16_t op_in_trx = 0;
        uint32_t virtual_op = 0;
        time_point_sec timestamp;
        buffer_type serialized_op;
    };

    using operation_id_type = object_id<op_note_object>;

    struct by_location;
    struct by_transaction_id;
    using op_note_index = multi_index_container<
        op_note_object,
        indexed_by<
            ordered_unique<
                tag<by_id>,
                member<op_note_object, operation_id_type, &op_note_object::id>>,
            ordered_unique<
                tag<by_location>,
                composite_key<
                    op_note_object,
                    member<op_note_object, uint32_t, &op_note_object::block>,
                    member<op_note_object, uint32_t, &op_note_object::trx_in_block>,
                    member<op_note_object, uint16_t, &op_note_object::op_in_trx>,
                    member<op_note_object, uint32_t, &op_note_object::virtual_op>,
                    member<op_note_object, operation_id_type, &op_note_object::id>>>,
            ordered_unique<
                tag<by_transaction_id>,
                composite_key<
                    op_note_object,
                    member<op_note_object, transaction_id_type, &op_note_object::trx_id>,
                    member<op_note_object, operation_id_type, &op_note_object::id>>>>,
        allocator<op_note_object>>;

    struct op_note_api_object final {
        op_note_api_object();

        op_note_api_object(const op_note_object& opno)
            : trx_id(opno.trx_id),
            block(opno.block),
            trx_in_block(opno.trx_in_block),
            op_in_trx(opno.op_in_trx),
            virtual_op(opno.virtual_op),
            timestamp(opno.timestamp),
            op(fc::raw::unpack<protocol::operation>(opno.serialized_op)) {
        }

        transaction_id_type trx_id;
        uint32_t block = 0;
        uint32_t trx_in_block = 0;
        uint16_t op_in_trx = 0;
        uint64_t virtual_op = 0;
        fc::time_point_sec timestamp;
        operation op;
    };
} } } // golos::plugins::event_plugin

CHAINBASE_SET_INDEX_TYPE(
    golos::plugins::event_plugin::op_note_object,
    golos::plugins::event_plugin::op_note_index)


FC_REFLECT(
    (golos::plugins::event_plugin::op_note_api_object),
    (trx_id)(block)(trx_in_block)(op_in_trx)(virtual_op)(timestamp)(op))
