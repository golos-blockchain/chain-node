#include <golos/protocol/worker_operations.hpp>
#include <golos/protocol/worker_exceptions.hpp>
#include <golos/chain/steem_evaluator.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/chain/worker_objects.hpp>

namespace golos { namespace chain {

    void worker_proposal_evaluator::do_apply(const worker_proposal_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__1013, "worker_proposal_operation");

        const auto& post = _db.get_comment(o.author, o.permlink);

        const auto* wpo = _db.find_worker_proposal(post.id);

        if (wpo) {
            CHECK_PROPOSAL_HAS_NO_TECHSPECS((*wpo), "Cannot edit worker proposal with techspecs");

            _db.modify(*wpo, [&](worker_proposal_object& wpo) {
                wpo.type = o.type;
            });
            return;
        }

        CHECK_POST(post);

        _db.create<worker_proposal_object>([&](worker_proposal_object& wpo) {
            wpo.post = post.id;
            wpo.type = o.type;
            wpo.state = worker_proposal_state::created;
        });
    }

    void worker_proposal_delete_evaluator::do_apply(const worker_proposal_delete_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__1013, "worker_proposal_delete_operation");

        const auto& post = _db.get_comment(o.author, o.permlink);

        const auto& wpo = _db.get_worker_proposal(post.id);

        CHECK_PROPOSAL_HAS_NO_TECHSPECS(wpo, "Cannot delete worker proposal with techspecs");

        _db.remove(wpo);
    }

    void worker_techspec_evaluator::do_apply(const worker_techspec_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__1013, "worker_techspec_operation");

        const auto& wpo_post = _db.get_comment(o.worker_proposal_author, o.worker_proposal_permlink);
        const auto& wpo = _db.get_worker_proposal(wpo_post.id);

        CHECK_PROPOSAL_STATE(wpo.state < PROPOSAL_STATE::techspec, "Proposal already has approved techspec");

        if (wpo.type == worker_proposal_type::premade_work) {
            GOLOS_CHECK_LOGIC(o.worker.size(),
                logic_exception::worker_not_set,
                "Premade techspec requires worker set");
        }

        if (o.worker.size()) {
            _db.get_account(o.worker);
        }

        const auto& post = _db.get_comment(o.author, o.permlink);

        const auto* wto = _db.find_worker_techspec(post.id);

        if (wto) {
            GOLOS_CHECK_LOGIC(wto->worker_proposal_post == wpo_post.id,
                logic_exception::techspec_already_used_for_another_proposal,
                "This worker techspec is already used for another worker proposal");

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

        if (wpo.type == worker_proposal_type::premade_work) {
            GOLOS_CHECK_LOGIC(o.author == wpo_post.author,
                logic_exception::you_are_not_proposal_author,
                "Premade techspec can be created only by proposal author");
        }

        _db.create<worker_techspec_object>([&](worker_techspec_object& wto) {
            wto.post = post.id;
            wto.worker_proposal_post = wpo.post;
            wto.state = worker_techspec_state::created;
            wto.specification_cost = o.specification_cost;
            wto.development_cost = o.development_cost;
            wto.worker = o.worker;
            wto.payments_count = o.payments_count;
            wto.payments_interval = o.payments_interval;
        });
    }

    void worker_techspec_delete_evaluator::do_apply(const worker_techspec_delete_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__1013, "worker_techspec_delete_operation");

        const auto& post = _db.get_comment(o.author, o.permlink);
        const auto& wto = _db.get_worker_techspec(post.id);

        CHECK_TECHSPEC_STATE(wto.state < TECHSPEC_STATE::payment_complete, "Techspec already closed");
        CHECK_TECHSPEC_STATE(wto.state < TECHSPEC_STATE::payment, "Techspec paying, cannot delete");

        _db.close_worker_techspec(wto, worker_techspec_state::closed_by_author);
    }

    void worker_techspec_approve_evaluator::do_apply(const worker_techspec_approve_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__1013, "worker_techspec_approve_operation");

        CHECK_APPROVER_WITNESS(o.approver);

        const auto& wto_post = _db.get_comment(o.author, o.permlink);
        const auto& wto = _db.get_worker_techspec(wto_post.id);

        const auto& wpo = _db.get_worker_proposal(wto.worker_proposal_post);

        CHECK_TECHSPEC_STATE(wto.state < TECHSPEC_STATE::payment_complete, "Techspec closed, cannot approve");
        CHECK_TECHSPEC_STATE(wto.state < TECHSPEC_STATE::approved, "Techspec already approved");

        CHECK_PROPOSAL_STATE(wpo.state < PROPOSAL_STATE::techspec, "Another techspec approved for this proposal");

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

            _db.modify(wpo, [&](worker_proposal_object& wpo) {
                wpo.approved_techspec_post = wto_post.id;
                wpo.state = worker_proposal_state::techspec;
            });

            _db.clear_worker_techspec_approves(wto);

            if (wpo.type == worker_proposal_type::premade_work) {
                _db.modify(wto, [&](worker_techspec_object& wto) {
                    wto.next_cashout_time = _db.head_block_time() + wto.payments_interval;
                    wto.state = worker_techspec_state::payment;
                });

                return;
            }

            _db.modify(wto, [&](worker_techspec_object& wto) {
                if (wto.worker.size()) {
                    wto.state = worker_techspec_state::work;
                } else {
                    wto.state = worker_techspec_state::approved;
                }
            });
        }
    }

    void worker_result_check_post(const database& _db, const comment_object& post) {
        const auto* wto_result = _db.find_worker_result(post.id);
        GOLOS_CHECK_LOGIC(!wto_result,
            logic_exception::post_is_already_used,
            "This post already used as worker result");

        const auto* wto = _db.find_worker_techspec(post.id);
        GOLOS_CHECK_LOGIC(!wto,
            logic_exception::post_is_already_used,
            "This post already used as worker techspec");
    }

    void worker_result_evaluator::do_apply(const worker_result_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__1013, "worker_result_operation");

        const auto& post = _db.get_comment(o.author, o.permlink);

        CHECK_POST(post);
        worker_result_check_post(_db, post);

        const auto& wto_post = _db.get_comment(o.author, o.worker_techspec_permlink);
        const auto& wto = _db.get_worker_techspec(wto_post.id);

        CHECK_TECHSPEC_STATE(wto.state < TECHSPEC_STATE::complete, "Techspec already complete");
        CHECK_TECHSPEC_STATE(wto.state >= TECHSPEC_STATE::work, "Techspec not yet in work");

        _db.modify(wto, [&](worker_techspec_object& wto) {
            wto.worker_result_post = post.id;
            wto.state = worker_techspec_state::complete;
        });
    }

    void worker_result_delete_evaluator::do_apply(const worker_result_delete_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__1013, "worker_result_delete_operation");

        const auto& worker_result_post = _db.get_comment(o.author, o.permlink);
        const auto& wto = _db.get_worker_result(worker_result_post.id);

        CHECK_TECHSPEC_STATE(wto.state < TECHSPEC_STATE::payment_complete, "Techspec closed, cannot delete result");
        CHECK_TECHSPEC_STATE(wto.state < TECHSPEC_STATE::payment, "Techspec paying, cannot delete result");

        _db.modify(wto, [&](worker_techspec_object& wto) {
            wto.worker_result_post = comment_id_type(-1);
            wto.state = worker_techspec_state::wip;
        });
    }

    void worker_payment_approve_evaluator::do_apply(const worker_payment_approve_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__1013, "worker_payment_approve_operation");

        CHECK_APPROVER_WITNESS(o.approver);

        const auto& wto_post = _db.get_comment(o.worker_techspec_author, o.worker_techspec_permlink);
        const auto& wto = _db.get_worker_techspec(wto_post.id);

        CHECK_TECHSPEC_STATE(wto.state != TECHSPEC_STATE::payment_complete, "Techspec payed out, nothing to approve");

        CHECK_TECHSPEC_STATE(wto.state <= TECHSPEC_STATE::payment, "Techspec closed, cannot approve payments");
        CHECK_TECHSPEC_STATE(wto.state >= TECHSPEC_STATE::work, "Techspec not yet in work, nothing to approve");

        if (o.state == worker_techspec_approve_state::approve) {
            CHECK_TECHSPEC_STATE(wto.state < TECHSPEC_STATE::payment, "Payments already approved");
            CHECK_TECHSPEC_STATE(wto.state == TECHSPEC_STATE::complete, "Techspec still in work, nothing to approve");
        }

        const auto& wpao_idx = _db.get_index<worker_payment_approve_index, by_techspec_approver>();
        auto wpao_itr = wpao_idx.find(std::make_tuple(wto_post.id, o.approver));

        if (o.state == worker_techspec_approve_state::abstain) {
            CHECK_NO_VOTE_REPEAT(wpao_itr, wpao_idx.end());

            _db.remove(*wpao_itr);
            return;
        }

        if (wpao_itr != wpao_idx.end()) {
            CHECK_NO_VOTE_REPEAT(wpao_itr->state, o.state);

            _db.modify(*wpao_itr, [&](worker_payment_approve_object& wpao) {
                wpao.state = o.state;
            });
        } else {
            _db.create<worker_payment_approve_object>([&](worker_payment_approve_object& wpao) {
                wpao.approver = o.approver;
                wpao.post = wto_post.id;
                wpao.state = o.state;
            });
        }

        auto approves = _db.count_worker_payment_approves(wto_post.id);

        if (o.state == worker_techspec_approve_state::disapprove) {
            if (approves[o.state] < STEEMIT_SUPER_MAJOR_VOTED_WITNESSES) {
                return;
            }

            if (wto.state == worker_techspec_state::payment) {
                _db.close_worker_techspec(wto, worker_techspec_state::disapproved_by_witnesses);
                return;
            }

            _db.close_worker_techspec(wto, worker_techspec_state::closed_by_witnesses);
        } else if (o.state == worker_techspec_approve_state::approve) {
            if (approves[o.state] < STEEMIT_MAJOR_VOTED_WITNESSES) {
                return;
            }

            _db.modify(wto, [&](worker_techspec_object& wto) {
                wto.next_cashout_time = _db.head_block_time() + wto.payments_interval;
                wto.state = worker_techspec_state::payment;
            });
        }
    }

    void worker_assign_evaluator::do_apply(const worker_assign_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__1013, "worker_assign_operation");

        const auto& wto_post = _db.get_comment(o.worker_techspec_author, o.worker_techspec_permlink);
        const auto& wto = _db.get_worker_techspec(wto_post.id);

        if (!o.worker.size()) { // Unassign worker
            CHECK_TECHSPEC_STATE(wto.state < TECHSPEC_STATE::complete, "Techspec already complete, cannot unassign");
            CHECK_TECHSPEC_STATE(wto.state >= TECHSPEC_STATE::work, "No worker assigned, nothing to unassign");

            CHECK_TECHSPEC_STATE(wto.state != TECHSPEC_STATE::wip, "Techspec in WIP, cannot unassign");

            GOLOS_CHECK_LOGIC(o.assigner == wto.worker || o.assigner == wto_post.author,
                logic_exception::you_are_not_techspec_author_or_worker,
                "Worker can be unassigned only by techspec author or himself");

            _db.modify(wto, [&](worker_techspec_object& wto) {
                wto.worker = account_name_type();
                wto.state = worker_techspec_state::approved;
            });

            return;
        }

        CHECK_TECHSPEC_STATE(wto.state < TECHSPEC_STATE::work, "Worker already assigned");
        CHECK_TECHSPEC_STATE(wto.state >= TECHSPEC_STATE::approved, "Techspec not yet approved, cannot assign worker");

        _db.get_account(o.worker);

        _db.modify(wto, [&](worker_techspec_object& wto) {
            wto.worker = o.worker;
            wto.state = worker_techspec_state::work;
        });
    }

    void worker_fund_evaluator::do_apply(const worker_fund_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22__1107, "worker_fund_operation");

        const auto& sponsor = _db.get_account(o.sponsor);

        GOLOS_CHECK_BALANCE(sponsor, MAIN_BALANCE, o.amount);
        _db.adjust_balance(sponsor, -o.amount);

        const auto& props = _db.get_dynamic_global_properties();
        _db.modify(props, [&](dynamic_global_property_object& p) {
            p.total_worker_fund_steem += o.amount;
        });
    }

} } // golos::chain
