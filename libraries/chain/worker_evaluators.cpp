#include <golos/protocol/worker_operations.hpp>
#include <golos/protocol/worker_exceptions.hpp>
#include <golos/chain/steem_evaluator.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/chain/worker_objects.hpp>

namespace golos { namespace chain {

    using int128_t = boost::multiprecision::int128_t;

    void worker_request_evaluator::do_apply(const worker_request_operation& op) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__8, "worker_request_operation");

        const auto& post = _db.get_comment_by_perm(op.author, op.permlink);

        const auto* wro = _db.find_worker_request(post.id);

        _db.get_account(op.worker);

        if (wro) {
            CHECK_REQUEST_STATE(wro->state < REQUEST_STATE::payment, "Cannot modify approved request");

            const auto& wrvo_idx = _db.get_index<worker_request_vote_index, by_request_voter>();
            GOLOS_CHECK_LOGIC(wrvo_idx.find(wro->post) == wrvo_idx.end(), 
                logic_exception::cannot_edit_voted,
                "Cannot edit request with votes");

            _db.modify(*wro, [&](auto& o) {
                o.worker = op.worker;
                o.required_amount_min = op.required_amount_min;
                o.required_amount_max = op.required_amount_max;
                o.vest_reward = op.vest_reward;
                o.duration = op.duration;
                o.vote_end_time = o.created + fc::seconds(op.duration);
            });

            return;
        }

        CHECK_POST(post);

        const auto& fee = _db.get_witness_schedule_object().median_props.worker_request_creation_fee;
        if (fee.amount != 0) {
            const auto& author = _db.get_account(post.author);
            GOLOS_CHECK_BALANCE(_db, author, MAIN_BALANCE, fee);
            _db.adjust_balance(author, -fee);
            _db.adjust_balance(_db.get_account(STEEMIT_WORKER_POOL_ACCOUNT), fee);
        }

        _db.create<worker_request_object>([&](auto& o) {
            o.post = post.id;
            o.worker = op.worker;
            o.state = worker_request_state::created;
            o.created = _db.head_block_time();
            o.required_amount_min = op.required_amount_min;
            o.required_amount_max = op.required_amount_max;
            o.vest_reward = op.vest_reward;
            o.duration = op.duration;
            o.vote_end_time = o.created + fc::seconds(op.duration);
        });

        auto* extras = _db.find_extras(op.author, post.hashlink);
        if (extras) {
            _db.modify(*extras, [&](auto& o) {
                o.has_worker_request = true;
            });
        }
    }

    void worker_request_delete_evaluator::do_apply(const worker_request_delete_operation& op) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__8, "worker_request_delete_operation");

        const auto& post = _db.get_comment_by_perm(op.author, op.permlink);
        const auto& wro = _db.get_worker_request(post.id);

        CHECK_REQUEST_STATE(wro.state < REQUEST_STATE::payment_complete, "Request already closed");
        CHECK_REQUEST_STATE(wro.state < REQUEST_STATE::payment, "Request paying, cannot delete");

        auto* extras = _db.find_extras(op.author, post.hashlink);
        if (extras) {
            _db.modify(*extras, [&](auto& o) {
                o.has_worker_request = false;
            });
        }

        _db.close_worker_request(wro, worker_request_state::closed_by_author);
    }

    void worker_request_vote_evaluator::do_apply(const worker_request_vote_operation& op) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__8, "worker_request_vote_operation");

        GOLOS_CHECK_BALANCE(_db, _db.get_account(op.voter), EFFECTIVE_VESTING, asset(1, VESTS_SYMBOL));

        const auto& post = _db.get_comment_by_perm(op.author, op.permlink);
        const auto& wro = _db.get_worker_request(post.id);

        CHECK_REQUEST_STATE(wro.state < REQUEST_STATE::payment_complete, "Request closed, cannot vote");
        CHECK_REQUEST_STATE(wro.state < REQUEST_STATE::payment, "Request already paying");

        const auto& wrvo_idx = _db.get_index<worker_request_vote_index, by_request_voter>();
        auto wrvo_itr = wrvo_idx.find(std::make_tuple(wro.post, op.voter));

        if (op.vote_percent == 0) {
            CHECK_NO_VOTE_REPEAT(wrvo_itr, wrvo_idx.end());

            _db.modify(wro, [&](auto& o) {
                o.stake_rshares -= wrvo_itr->rshares;
                o.stake_total -= wrvo_itr->stake;
            });

            _db.remove(*wrvo_itr);
            return;
        }

        auto stake = _db.get_account(op.voter).vesting_shares.amount;
        share_type rshares = static_cast<int64_t>(int128_t(stake.value) * op.vote_percent / STEEMIT_100_PERCENT);

        share_type old_rshares = 0;
        share_type old_stake = 0;

        if (wrvo_itr != wrvo_idx.end()) {
            CHECK_NO_VOTE_REPEAT(wrvo_itr->vote_percent, op.vote_percent);

            old_rshares = wrvo_itr->rshares;
            old_stake = wrvo_itr->stake;

            _db.modify(*wrvo_itr, [&](auto& o) {
                o.vote_percent = op.vote_percent;
                o.rshares = rshares;
                o.stake = stake;
            });
        } else {
            _db.create<worker_request_vote_object>([&](auto& o) {
                o.voter = op.voter;
                o.post = wro.post;
                o.vote_percent = op.vote_percent;
                o.rshares = rshares;
                o.stake = stake;
            });
        }

        _db.modify(wro, [&](auto& o) {
            o.stake_rshares += (-old_rshares + rshares);
            o.stake_total += (-old_stake + stake);
        });
    }

} } // golos::chain
