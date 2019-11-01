#include <golos/protocol/worker_operations.hpp>
#include <golos/protocol/worker_exceptions.hpp>
#include <golos/chain/steem_evaluator.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/chain/worker_objects.hpp>

namespace golos { namespace chain {

    void worker_request_evaluator::do_apply(const worker_request_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__8, "worker_request_operation");

        const auto& post = _db.get_comment(o.author, o.permlink);

        const auto* wto = _db.find_worker_request(post.id);

        _db.get_account(o.worker);

        if (wto) {
            CHECK_REQUEST_STATE(wto->state < REQUEST_STATE::payment, "Cannot modify approved request");

            _db.modify(*wto, [&](worker_request_object& wto) {
                wto.specification_cost = o.specification_cost;
                wto.development_cost = o.development_cost;
                wto.worker = o.worker;
            });

            return;
        }

        CHECK_POST(post);

        _db.create<worker_request_object>([&](worker_request_object& wto) {
            wto.post = post.id;
            wto.state = worker_request_state::created;
            wto.specification_cost = o.specification_cost;
            wto.development_cost = o.development_cost;
            wto.worker = o.worker;
        });
    }

    void worker_request_delete_evaluator::do_apply(const worker_request_delete_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__8, "worker_request_delete_operation");

        const auto& post = _db.get_comment(o.author, o.permlink);
        const auto& wto = _db.get_worker_request(post.id);

        CHECK_REQUEST_STATE(wto.state < REQUEST_STATE::payment_complete, "Request already closed");
        CHECK_REQUEST_STATE(wto.state < REQUEST_STATE::payment, "Request paying, cannot delete");

        _db.close_worker_request(wto, worker_request_state::closed_by_author);
    }

    void worker_request_approve_evaluator::do_apply(const worker_request_approve_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__8, "worker_request_approve_operation");

        CHECK_APPROVER_WITNESS(o.approver);

        const auto& wto_post = _db.get_comment(o.author, o.permlink);
        const auto& wto = _db.get_worker_request(wto_post.id);

        CHECK_REQUEST_STATE(wto.state < REQUEST_STATE::payment_complete, "Request closed, cannot approve");
        CHECK_REQUEST_STATE(wto.state < REQUEST_STATE::payment, "Request already approved");

        const auto& wtao_idx = _db.get_index<worker_request_approve_index, by_request_approver>();
        auto wtao_itr = wtao_idx.find(std::make_tuple(wto.post, o.approver));

        if (o.state == worker_request_approve_state::abstain) {
            CHECK_NO_VOTE_REPEAT(wtao_itr, wtao_idx.end());

            _db.remove(*wtao_itr);
            return;
        }

        if (wtao_itr != wtao_idx.end()) {
            CHECK_NO_VOTE_REPEAT(wtao_itr->state, o.state);

            _db.modify(*wtao_itr, [&](worker_request_approve_object& wtao) {
                wtao.state = o.state;
            });
        } else {
            _db.create<worker_request_approve_object>([&](worker_request_approve_object& wtao) {
                wtao.approver = o.approver;
                wtao.post = wto.post;
                wtao.state = o.state;
            });
        }

        auto approves = _db.count_worker_request_approves(wto.post);

        if (o.state == worker_request_approve_state::disapprove) {
            if (approves[o.state] < STEEMIT_SUPER_MAJOR_VOTED_WITNESSES) {
                return;
            }

            _db.close_worker_request(wto, worker_request_state::closed_by_witnesses);
        } else if (o.state == worker_request_approve_state::approve) {
            if (approves[o.state] < STEEMIT_MAJOR_VOTED_WITNESSES) {
                return;
            }

            _db.clear_worker_request_approves(wto);

            _db.modify(wto, [&](worker_request_object& wto) {
                //wto.next_cashout_time = _db.head_block_time() + wto.payments_interval;
                wto.state = worker_request_state::payment;
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
