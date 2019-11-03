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
        asset creation_fee;
        uint16_t upvotes = 0;
        uint16_t downvotes = 0;
        time_point_sec payment_beginning_time;
    };

    struct by_net_rshares;
    struct by_upvotes;
    struct by_downvotes;

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
                tag<by_upvotes>,
                composite_key<
                    worker_request_metadata_object,
                    member<worker_request_metadata_object, uint16_t, &worker_request_metadata_object::upvotes>,
                    member<worker_request_metadata_object, worker_request_metadata_id_type, &worker_request_metadata_object::id>>,
                composite_key_compare<
                    std::greater<uint16_t>,
                    std::less<worker_request_metadata_id_type>>>,
            ordered_unique<
                tag<by_downvotes>,
                composite_key<
                    worker_request_metadata_object,
                    member<worker_request_metadata_object, uint16_t, &worker_request_metadata_object::downvotes>,
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
              creation_fee(o.creation_fee),
              upvotes(o.upvotes),
              downvotes(o.downvotes),
              payment_beginning_time(o.payment_beginning_time) {
        }

        worker_request_api_object() {
        }

        void fill_worker_request(const worker_request_object& wro) {
            worker = wro.worker;
            state = wro.state;
            created = wro.created;
            required_amount_min = wro.required_amount_min;
            required_amount_max = wro.required_amount_max;
            vest_reward = wro.vest_reward;
            duration = wro.duration;
            vote_end_time = wro.vote_end_time;
            remaining_payment = wro.remaining_payment;
        }

        comment_api_object post;
        account_name_type worker;
        worker_request_state state;
        time_point_sec created;
        time_point_sec modified;
        share_type net_rshares;
        asset required_amount_min;
        asset required_amount_max;
        bool vest_reward;
        uint32_t duration;
        time_point_sec vote_end_time = time_point_sec::maximum();
        asset creation_fee;
        uint16_t upvotes = 0;
        uint16_t downvotes = 0;
        time_point_sec payment_beginning_time;
        asset remaining_payment;
    };

} } } // golos::plugins::worker_api

CHAINBASE_SET_INDEX_TYPE(
    golos::plugins::worker_api::worker_request_metadata_object,
    golos::plugins::worker_api::worker_request_metadata_index)

FC_REFLECT((golos::plugins::worker_api::worker_request_api_object),
    (post)(worker)(state)(created)(modified)(net_rshares)
    (required_amount_min)(required_amount_max)(vest_reward)
    (duration)(vote_end_time)
    (creation_fee)
    (upvotes)(downvotes)(payment_beginning_time)(remaining_payment)
)
