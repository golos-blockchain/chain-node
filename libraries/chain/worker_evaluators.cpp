#include <golos/protocol/worker_operations.hpp>
#include <golos/protocol/worker_exceptions.hpp>
#include <golos/chain/steem_evaluator.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/chain/worker_objects.hpp>

namespace golos { namespace chain {

    void worker_techspec_evaluator::do_apply(const worker_techspec_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__8, "worker_techspec_operation");

        const auto& post = _db.get_comment(o.author, o.permlink);

        const auto* wto = _db.find_worker_techspec(post.id);

        _db.get_account(o.worker);

        if (wto) {
            CHECK_TECHSPEC_STATE(wto->state < TECHSPEC_STATE::payment, "Cannot modify approved techspec");

            _db.modify(*wto, [&](worker_techspec_object& wto) {
                wto.specification_cost = o.specification_cost;
                wto.development_cost = o.development_cost;
                wto.worker = o.worker;
                wto.payments_count = o.payments_count;
                wto.payments_interval = o.payments_interval;
            });

            return;
        }

        CHECK_POST(post);

        _db.create<worker_techspec_object>([&](worker_techspec_object& wto) {
            wto.post = post.id;
            wto.state = worker_techspec_state::created;
            wto.specification_cost = o.specification_cost;
            wto.development_cost = o.development_cost;
            wto.worker = o.worker;
            wto.payments_count = o.payments_count;
            wto.payments_interval = o.payments_interval;
        });
    }

    void worker_techspec_delete_evaluator::do_apply(const worker_techspec_delete_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__8, "worker_techspec_delete_operation");

        const auto& post = _db.get_comment(o.author, o.permlink);
        const auto& wto = _db.get_worker_techspec(post.id);

        CHECK_TECHSPEC_STATE(wto.state < TECHSPEC_STATE::payment_complete, "Techspec already closed");
        CHECK_TECHSPEC_STATE(wto.state < TECHSPEC_STATE::payment, "Techspec paying, cannot delete");

        _db.close_worker_techspec(wto, worker_techspec_state::closed_by_author);
    }

    void worker_techspec_approve_evaluator::do_apply(const worker_techspec_approve_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__8, "worker_techspec_approve_operation");

        CHECK_APPROVER_WITNESS(o.approver);

        const auto& wto_post = _db.get_comment(o.author, o.permlink);
        const auto& wto = _db.get_worker_techspec(wto_post.id);

        CHECK_TECHSPEC_STATE(wto.state < TECHSPEC_STATE::payment_complete, "Techspec closed, cannot approve");
        CHECK_TECHSPEC_STATE(wto.state < TECHSPEC_STATE::payment, "Techspec already approved");

        const auto& wtao_idx = _db.get_index<worker_techspec_approve_index, by_techspec_approver>();
        auto wtao_itr = wtao_idx.find(std::make_tuple(wto.post, o.approver));

        if (o.state == worker_techspec_approve_state::abstain) {
            CHECK_NO_VOTE_REPEAT(wtao_itr, wtao_idx.end());

            _db.remove(*wtao_itr);
            return;
        }

        if (wtao_itr != wtao_idx.end()) {
            CHECK_NO_VOTE_REPEAT(wtao_itr->state, o.state);

            _db.modify(*wtao_itr, [&](worker_techspec_approve_object& wtao) {
                wtao.state = o.state;
            });
        } else {
            _db.create<worker_techspec_approve_object>([&](worker_techspec_approve_object& wtao) {
                wtao.approver = o.approver;
                wtao.post = wto.post;
                wtao.state = o.state;
            });
        }

        auto approves = _db.count_worker_techspec_approves(wto.post);

        if (o.state == worker_techspec_approve_state::disapprove) {
            if (approves[o.state] < STEEMIT_SUPER_MAJOR_VOTED_WITNESSES) {
                return;
            }

            _db.close_worker_techspec(wto, worker_techspec_state::closed_by_witnesses);
        } else if (o.state == worker_techspec_approve_state::approve) {
            auto day_sec = fc::days(1).to_seconds();
            auto payments_period = int64_t(wto.payments_interval) * wto.payments_count;

            auto consumption = _db.calculate_worker_techspec_consumption_per_day(wto);

            const auto& gpo = _db.get_dynamic_global_properties();

            uint128_t revenue_funds(gpo.worker_revenue_per_day.amount.value);
            revenue_funds = revenue_funds * payments_period / day_sec;
            revenue_funds += gpo.total_worker_fund_steem.amount.value;

            auto consumption_funds = uint128_t(gpo.worker_consumption_per_day.amount.value) + consumption.amount.value;
            consumption_funds = consumption_funds * payments_period / day_sec;

            GOLOS_CHECK_LOGIC(revenue_funds >= consumption_funds,
                logic_exception::insufficient_funds_to_approve,
                "Insufficient funds to approve techspec");

            if (approves[o.state] < STEEMIT_MAJOR_VOTED_WITNESSES) {
                return;
            }

            _db.modify(gpo, [&](dynamic_global_property_object& gpo) {
                gpo.worker_consumption_per_day += consumption;
            });

            _db.clear_worker_techspec_approves(wto);

            _db.modify(wto, [&](worker_techspec_object& wto) {
                wto.next_cashout_time = _db.head_block_time() + wto.payments_interval;
                wto.state = worker_techspec_state::payment;
            });

            return;
        }
    }

    void worker_fund_evaluator::do_apply(const worker_fund_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__8, "worker_fund_operation");

        const auto& sponsor = _db.get_account(o.sponsor);

        GOLOS_CHECK_BALANCE(sponsor, MAIN_BALANCE, o.amount);
        _db.adjust_balance(sponsor, -o.amount);

        const auto& props = _db.get_dynamic_global_properties();
        _db.modify(props, [&](dynamic_global_property_object& p) {
            p.total_worker_fund_steem += o.amount;
        });
    }

} } // golos::chain
