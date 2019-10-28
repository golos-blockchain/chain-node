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

    template<typename ApproveMultiIndex, typename ApproveIndex>
    flat_map<worker_request_approve_state, int32_t> count_worker_approves(const database& _db, const comment_id_type& post) {
        flat_map<worker_request_approve_state, int32_t> result;

        const auto& approve_idx = _db.get_index<ApproveMultiIndex, ApproveIndex>();
        auto approve_itr = approve_idx.lower_bound(post);
        for (; approve_itr != approve_idx.end() && approve_itr->post == post; ++approve_itr) {
            auto witness = _db.find_witness(approve_itr->approver);
            if (witness && witness->schedule == witness_object::top19) {
                result[approve_itr->state]++;
            }
        }

        return result;
    }

    flat_map<worker_request_approve_state, int32_t> database::count_worker_request_approves(const comment_id_type& post) {
        return count_worker_approves<worker_request_approve_index, by_request_approver>(*this, post);
    }

    asset database::calculate_worker_request_consumption_per_day(const worker_request_object& wto) {
        auto payments_period = int64_t(wto.payments_interval) * wto.payments_count;
        uint128_t cost(wto.development_cost.amount.value);
        cost += wto.specification_cost.amount.value;
        cost *= fc::days(1).to_seconds();
        cost /= payments_period;
        return asset(cost.to_uint64(), STEEM_SYMBOL);
    }

    void database::process_worker_cashout() {
        if (!has_hardfork(STEEMIT_HARDFORK_0_22__8)) {
            return;
        }

        const auto now = head_block_time();

        const auto& wto_idx = get_index<worker_request_index, by_next_cashout_time>();

        for (auto wto_itr = wto_idx.begin(); wto_itr != wto_idx.end() && wto_itr->next_cashout_time <= now; ++wto_itr) {
            auto remaining_payments_count = wto_itr->payments_count - wto_itr->finished_payments_count;

            auto author_reward = wto_itr->specification_cost / wto_itr->payments_count;
            auto worker_reward = wto_itr->development_cost / wto_itr->payments_count;

            if (remaining_payments_count == 1) {
                author_reward = wto_itr->specification_cost - (author_reward * wto_itr->finished_payments_count);
                worker_reward = wto_itr->development_cost - (worker_reward * wto_itr->finished_payments_count);
            }

            const auto& gpo = get_dynamic_global_properties();

            if (gpo.total_worker_fund_steem < (author_reward + worker_reward)) {
                return;
            }

            if (remaining_payments_count == 1) {
                modify(gpo, [&](dynamic_global_property_object& gpo) {
                    gpo.worker_consumption_per_day -= calculate_worker_request_consumption_per_day(*wto_itr);
                });

                modify(*wto_itr, [&](worker_request_object& wto) {
                    wto.finished_payments_count++;
                    wto.next_cashout_time = time_point_sec::maximum();
                    wto.state = worker_request_state::payment_complete;
                });
            } else {
                modify(*wto_itr, [&](worker_request_object& wto) {
                    wto.finished_payments_count++;
                    wto.next_cashout_time = now + wto.payments_interval;
                });
            }

            modify(gpo, [&](dynamic_global_property_object& gpo) {
                gpo.total_worker_fund_steem -= (author_reward + worker_reward);
            });

            const auto& wto_post = get_comment(wto_itr->post);

            adjust_balance(get_account(wto_post.author), author_reward);
            adjust_balance(get_account(wto_itr->worker), worker_reward);

            push_virtual_operation(request_reward_operation(wto_post.author, to_string(wto_post.permlink), author_reward));
            push_virtual_operation(worker_reward_operation(wto_itr->worker, wto_post.author, to_string(wto_post.permlink), worker_reward));
        }
    }

    void database::set_clear_old_worker_approves(bool clear_old_worker_approves) {
        _clear_old_worker_approves = clear_old_worker_approves;
    }

    template<typename ApproveMultiIndex, typename ApproveIndex>
    void clear_worker_approves(database& _db, const comment_id_type& post) {
        const auto& approve_idx = _db.get_index<ApproveMultiIndex, ApproveIndex>();
        auto approve_itr = approve_idx.lower_bound(post);
        while (approve_itr != approve_idx.end() && approve_itr->post == post) {
            const auto& approve = *approve_itr;
            ++approve_itr;
            _db.remove(approve);
        }
    }

    void database::clear_worker_request_approves(const worker_request_object& wto) {
        if (!_clear_old_worker_approves) {
            return;
        }

        clear_worker_approves<worker_request_approve_index, by_request_approver>(*this, wto.post);
    }

    void database::close_worker_request(const worker_request_object& wto, worker_request_state closed_state) {
        bool has_approves = false;

        if (wto.state >= worker_request_state::payment) {
            const auto& gpo = get_dynamic_global_properties();
            modify(gpo, [&](dynamic_global_property_object& gpo) {
                gpo.worker_consumption_per_day -= calculate_worker_request_consumption_per_day(wto);
            });

            has_approves = true;
        } else {
            const auto& wtao_idx = get_index<worker_request_approve_index, by_request_approver>();
            auto wtao_itr = wtao_idx.find(wto.post);
            has_approves = wtao_itr != wtao_idx.end();
        }

        clear_worker_request_approves(wto);

        if (closed_state == worker_request_state::closed_by_author && !has_approves) {
            remove(wto);
        } else {
            modify(wto, [&](worker_request_object& wto) {
                wto.next_cashout_time = time_point_sec::maximum();
                wto.state = closed_state;
            });
        }
    }

    void database::clear_expired_worker_objects() {
        if (!has_hardfork(STEEMIT_HARDFORK_0_22__8)) {
            return;
        }

        const auto& mprops = get_witness_schedule_object().median_props;

        const auto now = head_block_time();

        const auto& idx = get_index<worker_request_index, by_post>();
        for (auto itr = idx.begin(); itr != idx.end(); itr++) {
            if (itr->state != worker_request_state::created) {
                continue;
            }

            const auto& wto_post = get_comment(itr->post);
            if (wto_post.created + mprops.worker_request_approve_term_sec > now) {
                break;
            }

            close_worker_request(*itr, worker_request_state::closed_by_expiration);

            push_virtual_operation(request_expired_operation(wto_post.author, to_string(wto_post.permlink)));
        }
    }

} } // golos::chain