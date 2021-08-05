#pragma once

#include <golos/chain/steem_object_types.hpp>
#include <boost/multi_index/composite_key.hpp>

namespace golos { namespace chain {

    using namespace golos::protocol;

    class event_object : public object<event_object_type, event_object> {
    public:
        event_object() = delete;

        template <typename Constructor, typename Allocator>
        event_object(Constructor&& c, allocator<Allocator> a)
            : serialized_op(a.get_segment_manager()) {
            c(*this);
        };

        id_type id;
        buffer_type serialized_op;
    };

    using event_index = multi_index_container<
        event_object,
        indexed_by<
            ordered_unique<
                tag<by_id>,
                member<event_object, event_object_id_type, &event_object::id>
            >
        >,
        allocator<event_object>>;

} } // golos::chain

CHAINBASE_SET_INDEX_TYPE(
    golos::chain::event_object,
    golos::chain::event_index);
