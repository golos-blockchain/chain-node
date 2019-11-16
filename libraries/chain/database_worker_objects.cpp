#include <golos/chain/database.hpp>
#include <golos/chain/account_object.hpp>
#include <golos/chain/comment_object.hpp>
#include <golos/protocol/exceptions.hpp>

namespace golos { namespace chain {

    using int128_t = boost::multiprecision::int128_t;

    const worker_request_object& database::get_worker_request(const comment_id_type& post) const { try {
        return get<worker_request_object, by_post>(post);
    } catch (const std::out_of_range &e) {
        const auto& comment = get_comment(post);
        GOLOS_THROW_MISSING_OBJECT("worker_request_object", fc::mutable_variant_object()("account",comment.author)("permlink",comment.permlink));
    } FC_CAPTURE_AND_RETHROW((post)) }

    const worker_request_object* database::find_worker_request(const comment_id_type& post) const {
        return find<worker_request_object, by_post>(post);
    }

    flat_map<bool, uint32_t> database::count_worker_request_votes(const comment_id_type& post, boost::optional<share_type&> stake_rshares, boost::optional<share_type&> stake_total) {
        flat_map<bool, uint32_t> result;

        if (stake_rshares) *stake_rshares = 0;
        if (stake_total) *stake_total = 0;

        const auto& vote_idx = get_index<worker_request_vote_index, by_request_voter>();
        auto vote_itr = vote_idx.lower_bound(post);
        for (; vote_itr != vote_idx.end() && vote_itr->post == post; ++vote_itr) {
            result[vote_itr->vote_percent > 0]++;
            if (stake_rshares)
                *stake_rshares = *stake_rshares + vote_itr->rshares;
            if (stake_total)
                *stake_total = *stake_total + vote_itr->stake;
        }

        return result;
    }

    void database::set_clear_old_worker_votes(bool clear_old_worker_votes) {
        _clear_old_worker_votes = clear_old_worker_votes;
    }

    void database::clear_worker_request_votes(const worker_request_object& wro) {
        if (!_clear_old_worker_votes) {
            return;
        }

        const auto& vote_idx = get_index<worker_request_vote_index, by_request_voter>();
        auto vote_itr = vote_idx.lower_bound(post);
        while (vote_itr != vote_idx.end() && vote_itr->post == post) {
            const auto& vote = *vote_itr;
            ++vote_itr;
            remove(vote);
        }
    }

    void database::close_worker_request(const worker_request_object& wro, worker_request_state closed_state) {
        bool has_votes = false;

        if (wro.state >= worker_request_state::payment) {
            has_votes = true;
        } else {
            const auto& wrvo_idx = get_index<worker_request_vote_index, by_request_voter>();
            auto wrvo_itr = wrvo_idx.find(wro.post);
            has_votes = wrvo_itr != wrvo_idx.end();
        }

        clear_worker_request_votes(wro);

        if (closed_state == worker_request_state::closed_by_author && !has_votes) {
            remove(wro);
        } else {
            modify(wro, [&](auto& o) {
                o.vote_end_time = time_point_sec::maximum();
                o.state = closed_state;
            });
        }
    }

    void database::send_worker_state(const comment_object& post, worker_request_state closed_state) {
        push_virtual_operation(worker_state_operation(post.author, to_string(post.permlink), closed_state));
    }

    void database::process_worker_votes() {
        if (!has_hardfork(STEEMIT_HARDFORK_0_22__8)) {
            return;
        }

        const auto& now = head_block_time();
        const auto& wso = get_witness_schedule_object().median_props;
        const auto& props = get_dynamic_global_properties();

        const auto& wro_idx = get_index<worker_request_index, by_vote_end_time>();
        for (auto itr = wro_idx.begin(); itr != wro_idx.end() && itr->vote_end_time <= now;) {
            const auto& wro = *itr;
            ++itr;

            share_type rshares_voted = 0;
            share_type rshares_rating = 0;

            const auto& post = get_comment(wro.post);
            const auto& wrvo_idx = get_index<worker_request_vote_index, by_request_voter>();
            auto wrvo_itr = wrvo_idx.lower_bound(post.id);
            for (; wrvo_itr != wrvo_idx.end() && wrvo_itr->post == post.id; ++wrvo_itr) {
                const auto voter_stake = get_account(wrvo_itr->voter).vesting_shares.amount.value;
                rshares_voted += voter_stake;
                rshares_rating += static_cast<int64_t>(int128_t(voter_stake) * wrvo_itr->vote_percent / STEEMIT_100_PERCENT);
            }

            share_type min_voted = (uint128_t(props.total_vesting_shares.amount.value) * wso.worker_request_approve_min_percent / STEEMIT_100_PERCENT).to_uint64();
            if (rshares_voted < min_voted) {
                close_worker_request(wro, worker_request_state::closed_by_expiration);
                send_worker_state(post, worker_request_state::closed_by_expiration);
                continue;
            }

            share_type calculated_payment = static_cast<int64_t>(int128_t(wro.required_amount_max.amount.value) * rshares_rating.value / rshares_voted.value);
            if (calculated_payment < wro.required_amount_min.amount) {
                modify(wro, [&](auto& o) {
                    o.remaining_payment = asset(calculated_payment, wro.required_amount_min.symbol);
                });
                close_worker_request(wro, worker_request_state::closed_by_voters);
                send_worker_state(post, worker_request_state::closed_by_voters);
                continue;
            }

            modify(wro, [&](auto& o) {
                o.vote_end_time = time_point_sec::maximum();
                o.remaining_payment = asset(calculated_payment, wro.required_amount_min.symbol);
                o.state = worker_request_state::payment;
            });
            modify(props, [&](auto& o) {
                o.worker_requests[wro.required_amount_min.symbol]++;
            });
            send_worker_state(post, worker_request_state::payment);
        }
    }

    void database::process_worker_cashout() {
        if (head_block_num() % GOLOS_WORKER_CASHOUT_INTERVAL != 0) return;

        if (!has_hardfork(STEEMIT_HARDFORK_0_22__8)) {
            return;
        }

        const auto& props = get_dynamic_global_properties();
        auto requests = props.worker_requests;

        const auto& pool = get_account(STEEMIT_WORKER_POOL_ACCOUNT);
        flat_map<asset_symbol_type, asset> max_request_payment;
        max_request_payment[STEEM_SYMBOL] = requests[STEEM_SYMBOL] > 0 ? (pool.balance / requests[STEEM_SYMBOL]) : asset(0, STEEM_SYMBOL);
        max_request_payment[SBD_SYMBOL] = requests[SBD_SYMBOL] > 0 ? (pool.sbd_balance / requests[SBD_SYMBOL]) : asset(0, SBD_SYMBOL);

        if (!max_request_payment[STEEM_SYMBOL].amount.value && !max_request_payment[SBD_SYMBOL].amount.value) {
            return;
        }

        const auto& wro_idx = get_index<worker_request_index, by_state>();
        auto itr = wro_idx.lower_bound(worker_request_state::payment);
        for (; itr != wro_idx.end() && itr->state == worker_request_state::payment;) {
            const auto& wro = *itr;
            ++itr;

            auto payment = std::min(wro.remaining_payment, max_request_payment[wro.required_amount_max.symbol]);
            if (!payment.amount.value) {
                continue;
            }

            const auto& post = get_comment(wro.post);
            const auto& worker = get_account(wro.worker);

            adjust_balance(pool, -payment);
            asset vest_reward{0, VESTS_SYMBOL};
            if (wro.vest_reward)
                vest_reward = create_vesting(worker, payment);
            else
                adjust_balance(worker, payment);
            push_virtual_operation(worker_reward_operation(wro.worker, post.author, to_string(post.permlink), payment, vest_reward));
            modify(wro, [&](auto& o) {
                o.remaining_payment -= payment;
                if (o.remaining_payment.amount <= 0) {
                    o.remaining_payment.amount = 0;
                    o.state = worker_request_state::payment_complete;
                }
            });

            if (wro.remaining_payment.amount <= 0) {
                modify(props, [&](auto& o) {
                    o.worker_requests[payment.symbol]--;
                });
                send_worker_state(post, worker_request_state::payment_complete);
            }
        }
    }
} } // golos::chain
