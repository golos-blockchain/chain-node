#include <golos/chain/database.hpp>
#include <golos/chain/account_object.hpp>
#include <golos/chain/comment_object.hpp>
#include <golos/protocol/exceptions.hpp>

namespace golos { namespace chain {

    const worker_request_object& database::get_worker_request(const comment_id_type& post) const { try {
        return get<worker_request_object, by_post>(post);
    } catch (const std::out_of_range &e) {
        const auto& comment = get_comment(post);
        GOLOS_THROW_MISSING_OBJECT("worker_request_object", fc::mutable_variant_object()("account",comment.author)("permlink",comment.permlink));
    } FC_CAPTURE_AND_RETHROW((post)) }

    const worker_request_object* database::find_worker_request(const comment_id_type& post) const {
        return find<worker_request_object, by_post>(post);
    }

    flat_map<bool, uint32_t> database::count_worker_request_votes(const comment_id_type& post) {
        flat_map<bool, uint32_t> result;

        const auto& vote_idx = get_index<worker_request_vote_index, by_request_voter>();
        auto vote_itr = vote_idx.lower_bound(post);
        for (; vote_itr != vote_idx.end() && vote_itr->post == post; ++vote_itr) {
            result[vote_itr->vote_percent > 0]++;
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
                const auto& voter = get_account(wrvo_itr->voter);
                const auto voter_stake = voter.effective_vesting_shares().amount.value;
                rshares_voted += voter_stake;
                rshares_rating += (uint128_t(voter_stake) * wrvo_itr->vote_percent / STEEMIT_100_PERCENT).to_uint64();
            }

            share_type min_voted = (uint128_t(props.total_vesting_shares.amount.value) * wso.worker_request_approve_min_percent / STEEMIT_100_PERCENT).to_uint64();
            if (rshares_voted < min_voted) {
                close_worker_request(wro, worker_request_state::closed_by_expiration);
                send_worker_state(post, worker_request_state::closed_by_expiration);
                continue;
            }

            share_type calculated_payment = (uint128_t(wro.required_amount_max.amount.value) * rshares_rating.value / rshares_voted.value).to_uint64();
            if (calculated_payment < wro.required_amount_min.amount) {
                modify(wro, [&](auto& o) {
                    o.calculated_payment = asset(calculated_payment, wro.required_amount_min.symbol);
                });
                close_worker_request(wro, worker_request_state::closed_by_voters);
                send_worker_state(post, worker_request_state::closed_by_voters);
                continue;
            }

            modify(wro, [&](auto& o) {
                o.calculated_payment = asset(calculated_payment, wro.required_amount_min.symbol);
                o.vote_end_time = time_point_sec::maximum();
                o.state = worker_request_state::payment;
            });
            send_worker_state(post, worker_request_state::payment);
        }
    }

    void database::process_worker_cashout() {
        if (!has_hardfork(STEEMIT_HARDFORK_0_22__8)) {
            return;
        }
        // TODO
    }
} } // golos::chain
