#pragma once

#include <golos/chain/worker_objects.hpp>
#include <golos/api/comment_api_object.hpp>

namespace golos { namespace plugins { namespace worker_api {

    using namespace golos::chain;
    using golos::api::comment_api_object;

#ifndef WORKER_API_SPACE_ID
#define WORKER_API_SPACE_ID 15
#endif

    enum worker_api_plugin_object_type {
        worker_request_metadata_object_type = (WORKER_API_SPACE_ID << 8)
    };

    class worker_request_metadata_object : public object<worker_request_metadata_object_type, worker_request_metadata_object> {
    public:
        worker_request_metadata_object() = delete;

        template <typename Constructor, typename Allocator>
        worker_request_metadata_object(Constructor&& c, allocator <Allocator> a) {
            c(*this);
        };

        id_type id;

        comment_id_type post;
        time_point_sec modified;
        share_type net_rshares;
        uint16_t approves = 0;
        uint16_t disapproves = 0;
        time_point_sec payment_beginning_time;
    };

    struct by_net_rshares;
    struct by_approves;
    struct by_disapproves;

    using worker_request_metadata_id_type = object_id<worker_request_metadata_object>;

    using worker_request_metadata_index = multi_index_container<
        worker_request_metadata_object,
        indexed_by<
            ordered_unique<
                tag<by_id>,
                member<worker_request_metadata_object, worker_request_metadata_id_type, &worker_request_metadata_object::id>>,
            ordered_unique<
                tag<by_post>,
                member<worker_request_metadata_object, comment_id_type, &worker_request_metadata_object::post>>,
            ordered_unique<
                tag<by_net_rshares>,
                composite_key<
                    worker_request_metadata_object,
                    member<worker_request_metadata_object, share_type, &worker_request_metadata_object::net_rshares>,
                    member<worker_request_metadata_object, worker_request_metadata_id_type, &worker_request_metadata_object::id>>,
                composite_key_compare<
                    std::greater<share_type>,
                    std::less<worker_request_metadata_id_type>>>,
            ordered_unique<
                tag<by_approves>,
                composite_key<
                    worker_request_metadata_object,
                    member<worker_request_metadata_object, uint16_t, &worker_request_metadata_object::approves>,
                    member<worker_request_metadata_object, worker_request_metadata_id_type, &worker_request_metadata_object::id>>,
                composite_key_compare<
                    std::greater<uint16_t>,
                    std::less<worker_request_metadata_id_type>>>,
            ordered_unique<
                tag<by_disapproves>,
                composite_key<
                    worker_request_metadata_object,
                    member<worker_request_metadata_object, uint16_t, &worker_request_metadata_object::disapproves>,
                    member<worker_request_metadata_object, worker_request_metadata_id_type, &worker_request_metadata_object::id>>,
                composite_key_compare<
                    std::greater<uint16_t>,
                    std::less<worker_request_metadata_id_type>>>>,
        allocator<worker_request_metadata_object>>;

    struct worker_request_api_object {
        worker_request_api_object(const worker_request_metadata_object& o, const comment_api_object& p)
            : post(p),
              modified(o.modified),
              net_rshares(o.net_rshares),
              approves(o.approves),
              disapproves(o.disapproves),
              payment_beginning_time(o.payment_beginning_time) {
        }

        worker_request_api_object() {
        }

        void fill_worker_request(const worker_request_object& wto) {
            worker = wto.worker;
            state = wto.state;
            required_amount_min = wto.required_amount_min;
            required_amount_max = wto.required_amount_max;
            duration = wto.duration;
            next_cashout_time = wto.next_cashout_time;
        }

        comment_api_object post;
        account_name_type worker;
        worker_request_state state;
        time_point_sec modified;
        share_type net_rshares;
        asset required_amount_min;
        asset required_amount_max;
        uint32_t duration;
        uint16_t approves = 0;
        uint16_t disapproves = 0;
        time_point_sec payment_beginning_time;
        time_point_sec next_cashout_time = time_point_sec::maximum();
    };

} } } // golos::plugins::worker_api

CHAINBASE_SET_INDEX_TYPE(
    golos::plugins::worker_api::worker_request_metadata_object,
    golos::plugins::worker_api::worker_request_metadata_index)

FC_REFLECT((golos::plugins::worker_api::worker_request_api_object),
    (post)(worker)(state)(modified)(net_rshares)(required_amount_min)(required_amount_max)
    (duration)(approves)(disapproves)(payment_beginning_time)(next_cashout_time)
)
