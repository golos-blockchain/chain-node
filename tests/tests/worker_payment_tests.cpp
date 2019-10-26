#include <boost/test/unit_test.hpp>

#include "worker_fixture.hpp"
#include "helpers.hpp"
#include "comment_reward.hpp"

#include <golos/protocol/worker_operations.hpp>
#include <golos/chain/worker_objects.hpp>

using namespace golos;
using namespace golos::protocol;
using namespace golos::chain;

BOOST_FIXTURE_TEST_SUITE(worker_payment_tests, worker_fixture)

BOOST_AUTO_TEST_CASE(worker_authorities) {
    BOOST_TEST_MESSAGE("Testing: worker_authorities");

    {
        worker_payment_approve_operation op;
        op.approver = "cyberfounder";
        op.worker_techspec_author = "bob";
        op.worker_techspec_permlink = "bob-techspec";
        op.state = worker_techspec_approve_state::approve;
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"cyberfounder"}));
    }

    {
        worker_fund_operation op;
        op.sponsor = "alice";
        op.amount = ASSET_GOLOS(10);
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"alice"}));
    }
}

BOOST_AUTO_TEST_CASE(worker_payment_approve_validate) {
    BOOST_TEST_MESSAGE("Testing: worker_payment_approve_validate");

    BOOST_TEST_MESSAGE("-- Normal case");

    worker_payment_approve_operation op;
    op.approver = "cyberfounder";
    op.worker_techspec_author = "bob";
    op.worker_techspec_permlink = "bob-techspec";
    op.state = worker_techspec_approve_state::approve;
    CHECK_OP_VALID(op);

    BOOST_TEST_MESSAGE("-- Incorrect account or permlink case");

    CHECK_PARAM_INVALID(op, approver, "");
    CHECK_PARAM_INVALID(op, worker_techspec_author, "");
    CHECK_PARAM_INVALID(op, worker_techspec_permlink, std::string(STEEMIT_MAX_PERMLINK_LENGTH+1, ' '));

    BOOST_TEST_MESSAGE("-- Invalid state case");

    CHECK_PARAM_INVALID(op, state, worker_techspec_approve_state::_size);
}

BOOST_AUTO_TEST_CASE(worker_payment_approve_apply) {
    BOOST_TEST_MESSAGE("Testing: worker_payment_approve_apply_approve");

    ACTORS((alice)(bob))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    comment_create("alice", alice_private_key, "alice-proposal", "", "alice-proposal");

    worker_proposal("alice", alice_private_key, "alice-proposal", worker_proposal_type::task);
    generate_block();

    BOOST_TEST_MESSAGE("-- Approving payment by not witness case");

    worker_payment_approve_operation op;
    op.approver = "alice";
    op.worker_techspec_author = "bob";
    op.worker_techspec_permlink = "bob-techspec";
    op.state = worker_techspec_approve_state::approve;
    GOLOS_CHECK_ERROR_MISSING(witness, "alice", alice_private_key, op);

    BOOST_TEST_MESSAGE("-- Approving payment by witness not in TOP-19 case");

    op.approver = "approver0";
    GOLOS_CHECK_ERROR_LOGIC(approver_is_not_top19_witness, private_key, op);

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    BOOST_TEST_MESSAGE("-- Approving payment without techspec post case");

    GOLOS_CHECK_ERROR_MISSING(comment, make_comment_id("bob", "bob-techspec"), private_key, op);

    BOOST_TEST_MESSAGE("-- Approving payment for non-existing techspec case");

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    GOLOS_CHECK_ERROR_MISSING(worker_techspec_object, make_comment_id("bob", "bob-techspec"), private_key, op);

    BOOST_TEST_MESSAGE("-- Creating techspec and approving it");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.worker_proposal_author = "alice";
    wtop.worker_proposal_permlink = "alice-proposal";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 40;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        worker_techspec_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-techspec", worker_techspec_approve_state::approve);
        generate_block();
    }

    BOOST_TEST_MESSAGE("-- Approving payment before work started");

    GOLOS_CHECK_ERROR_LOGIC(incorrect_techspec_state, private_key, op);

    BOOST_TEST_MESSAGE("-- Approving payment in techspec work state");

    worker_assign("bob", bob_private_key, "bob", "bob-techspec", "alice");

    GOLOS_CHECK_ERROR_LOGIC(incorrect_techspec_state, private_key, op);

    BOOST_TEST_MESSAGE("-- Approving payment in techspec complete state");

    generate_blocks(60 / STEEMIT_BLOCK_INTERVAL); // Waiting for posts window

    comment_create("bob", bob_private_key, "bob-result", "", "bob-result");
    worker_result("bob", bob_private_key, "bob-result", "bob-techspec");

    auto check_approves = [&](int approve_count, int disapprove_count) {
        auto approves = db->count_worker_payment_approves(db->get_comment("bob", string("bob-techspec")).id);
        BOOST_CHECK_EQUAL(approves[worker_techspec_approve_state::approve], approve_count);
        BOOST_CHECK_EQUAL(approves[worker_techspec_approve_state::disapprove], disapprove_count);
    };

    check_approves(0, 0);

    worker_payment_approve("approver0", private_key,
        "bob", "bob-techspec", worker_techspec_approve_state::approve);
    generate_block();

    check_approves(1, 0);

    for (auto i = 1; i < STEEMIT_MAJOR_VOTED_WITNESSES - 1; ++i) {
        worker_payment_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-techspec", worker_techspec_approve_state::approve);
        generate_block();
    }

    check_approves(STEEMIT_MAJOR_VOTED_WITNESSES - 1, 0);

    {
        const auto& wto = db->get_worker_techspec(db->get_comment("bob", string("bob-techspec")).id);
        BOOST_CHECK(wto.state != worker_techspec_state::payment);
        BOOST_CHECK_EQUAL(wto.next_cashout_time, time_point_sec::maximum());
    }

    const auto now = db->head_block_time();

    worker_payment_approve("approver" + std::to_string(STEEMIT_MAJOR_VOTED_WITNESSES - 1), private_key,
        "bob", "bob-techspec", worker_techspec_approve_state::approve);
    generate_block();

    check_approves(STEEMIT_MAJOR_VOTED_WITNESSES, 0);

    {
        const auto& wto = db->get_worker_techspec(db->get_comment("bob", string("bob-techspec")).id);
        BOOST_CHECK(wto.state == worker_techspec_state::payment);
        BOOST_CHECK_EQUAL(wto.next_cashout_time, now + wto.payments_interval);
    }

    BOOST_TEST_MESSAGE("-- Approving payment in techspec payment state");

    GOLOS_CHECK_ERROR_LOGIC(incorrect_techspec_state, private_key, op);
}

BOOST_AUTO_TEST_CASE(worker_payment_disapprove) {
    BOOST_TEST_MESSAGE("Testing: worker_payment_disapprove");

    ACTORS((alice)(bob)(carol))
    auto private_key = create_approvers(0, STEEMIT_SUPER_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    comment_create("alice", alice_private_key, "alice-proposal", "", "alice-proposal");

    worker_proposal("alice", alice_private_key, "alice-proposal", worker_proposal_type::task);
    generate_block();

    BOOST_TEST_MESSAGE("-- Creating 2 techspecs (bob's will be disapproved before payment, carol's - on payment)");

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.worker_proposal_author = "alice";
    wtop.worker_proposal_permlink = "alice-proposal";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 40;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    comment_create("carol", carol_private_key, "carol-techspec", "", "carol-techspec");

    wtop.author = "carol";
    wtop.permlink = "carol-techspec";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, wtop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Working with bob techspec");

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        worker_techspec_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-techspec", worker_techspec_approve_state::approve);
        generate_block();
    }

    worker_assign("bob", bob_private_key, "bob", "bob-techspec", "alice");

    BOOST_TEST_MESSAGE("---- Disapproving work");

    auto check_approves = [&](const std::string& author, const std::string& permlink, int approve_count, int disapprove_count) {
        auto approves = db->count_worker_payment_approves(db->get_comment(author, permlink).id);
        BOOST_CHECK_EQUAL(approves[worker_techspec_approve_state::approve], approve_count);
        BOOST_CHECK_EQUAL(approves[worker_techspec_approve_state::disapprove], disapprove_count);
    };

    check_approves("bob", "bob-techspec", 0, 0);

    worker_payment_approve("approver0", private_key,
        "bob", "bob-techspec", worker_techspec_approve_state::disapprove);
    generate_block();

    check_approves("bob", "bob-techspec", 0, 1);

    for (auto i = 1; i < STEEMIT_SUPER_MAJOR_VOTED_WITNESSES; ++i) {
        worker_payment_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-techspec", worker_techspec_approve_state::disapprove);
        generate_block();
    }

    check_approves("bob", "bob-techspec", 0, STEEMIT_SUPER_MAJOR_VOTED_WITNESSES);

    BOOST_TEST_MESSAGE("-- Checking bob techspec is closed");

    {
        const auto& wto = db->get_worker_techspec(db->get_comment("bob", string("bob-techspec")).id);
        BOOST_CHECK(wto.state == worker_techspec_state::closed_by_witnesses);

        const auto& wpo = db->get_worker_proposal(wto.worker_proposal_post);
        BOOST_CHECK(wpo.state == worker_proposal_state::created);
        BOOST_CHECK_EQUAL(wpo.approved_techspec_post, comment_id_type(-1));

        const auto& gpo = db->get_dynamic_global_properties();
        BOOST_CHECK_EQUAL(gpo.worker_consumption_per_day.amount, 0);
    }

    BOOST_TEST_MESSAGE("-- Working with carol techspec");

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        worker_techspec_approve("approver" + std::to_string(i), private_key,
            "carol", "carol-techspec", worker_techspec_approve_state::approve);
        generate_block();
    }

    worker_assign("carol", carol_private_key, "carol", "carol-techspec", "alice");

    BOOST_TEST_MESSAGE("---- Disapproving work by 1 witness");

    worker_payment_approve("approver0", private_key,
        "carol", "carol-techspec", worker_techspec_approve_state::disapprove);
    generate_block();

    check_approves("carol", "carol-techspec", 0, 1);

    BOOST_TEST_MESSAGE("---- Publishing result");

    generate_blocks(60 / STEEMIT_BLOCK_INTERVAL); // Waiting for posts window

    comment_create("carol", carol_private_key, "carol-result", "", "carol-result");
    worker_result("carol", carol_private_key, "carol-result", "carol-techspec");

    BOOST_TEST_MESSAGE("---- Disapproving result by 1 witness");

    worker_payment_approve("approver1", private_key,
        "carol", "carol-techspec", worker_techspec_approve_state::disapprove);
    generate_block();

    check_approves("carol", "carol-techspec", 0, 2);

    BOOST_TEST_MESSAGE("---- Setting state to wip");

    worker_result_delete_operation wrdop;
    wrdop.author = "carol";
    wrdop.permlink = "carol-result";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, wrdop));
    generate_block();

    BOOST_TEST_MESSAGE("---- Disapproving wip by 1 witness");

    worker_payment_approve("approver2", private_key,
        "carol", "carol-techspec", worker_techspec_approve_state::disapprove);
    generate_block();

    check_approves("carol", "carol-techspec", 0, 3);

    BOOST_TEST_MESSAGE("---- Publishing result again");

    worker_result("carol", carol_private_key, "carol-result", "carol-techspec");

    BOOST_TEST_MESSAGE("---- Approving result by enough witnesses");

    for (auto i = 3; i < STEEMIT_MAJOR_VOTED_WITNESSES + 3; ++i) {
        worker_payment_approve("approver" + std::to_string(i), private_key,
            "carol", "carol-techspec", worker_techspec_approve_state::approve);
        generate_block();
    }

    check_approves("carol", "carol-techspec", STEEMIT_MAJOR_VOTED_WITNESSES, 3);

    BOOST_TEST_MESSAGE("---- Disapproving payment by enough witnesses");

    for (auto i = 3; i < STEEMIT_SUPER_MAJOR_VOTED_WITNESSES; ++i) {
        worker_payment_approve("approver" + std::to_string(i), private_key,
            "carol", "carol-techspec", worker_techspec_approve_state::disapprove);
        generate_block();
    }

    check_approves("carol", "carol-techspec", 0, STEEMIT_SUPER_MAJOR_VOTED_WITNESSES);

    BOOST_TEST_MESSAGE("-- Checking carol techspec is closed");

    {
        const auto& wto = db->get_worker_techspec(db->get_comment("carol", string("carol-techspec")).id);
        BOOST_CHECK(wto.state == worker_techspec_state::disapproved_by_witnesses);

        const auto& wpo = db->get_worker_proposal(wto.worker_proposal_post);
        BOOST_CHECK(wpo.state == worker_proposal_state::created);
        BOOST_CHECK_EQUAL(wpo.approved_techspec_post, comment_id_type(-1));

        const auto& gpo = db->get_dynamic_global_properties();
        BOOST_CHECK_EQUAL(gpo.worker_consumption_per_day.amount, 0);
    }
}

BOOST_AUTO_TEST_CASE(worker_fund_filling) {
    BOOST_TEST_MESSAGE("Testing: worker_fund_filling");

    generate_block();

    auto old_fund = db->get_dynamic_global_properties().total_worker_fund_steem;

    comment_fund fund(*db);

    generate_block();

    const auto& gpo = db->get_dynamic_global_properties();
    BOOST_CHECK_EQUAL(gpo.total_worker_fund_steem, fund.worker_fund());
    BOOST_CHECK_EQUAL(gpo.worker_revenue_per_day, (gpo.total_worker_fund_steem - old_fund) * STEEMIT_BLOCKS_PER_DAY);
}

BOOST_AUTO_TEST_CASE(worker_fund_validate) {
    BOOST_TEST_MESSAGE("Testing: worker_fund_validate");

    BOOST_TEST_MESSAGE("-- Normal case");

    worker_fund_operation op;
    op.sponsor = "alice";
    op.amount = ASSET_GOLOS(10);
    CHECK_OP_VALID(op);

    BOOST_TEST_MESSAGE("-- Incorrect account case");

    CHECK_PARAM_INVALID(op, sponsor, "");

    BOOST_TEST_MESSAGE("-- Invalid symbol cases");

    CHECK_PARAM_INVALID(op, amount, ASSET_GBG(10));
    CHECK_PARAM_INVALID(op, amount, ASSET_GESTS(10));

    BOOST_TEST_MESSAGE("-- Invalid amount cases");

    CHECK_PARAM_INVALID(op, amount, ASSET_GOLOS(0));
    CHECK_PARAM_INVALID(op, amount, ASSET_GOLOS(-10));
}

BOOST_AUTO_TEST_CASE(worker_fund_apply) {
    BOOST_TEST_MESSAGE("Testing: worker_fund_apply");

    signed_transaction tx;

    ACTORS((alice))
    fund("alice", 15000);

    BOOST_TEST_MESSAGE("-- Normal funding");

    const auto init_balance = db->get_balance("alice", STEEM_SYMBOL);
    const auto init_fund = db->get_dynamic_global_properties().total_worker_fund_steem;

    generate_block();
    const auto block_add = db->get_dynamic_global_properties().total_worker_fund_steem - init_fund;

    worker_fund_operation op;
    op.sponsor = "alice";
    op.amount = ASSET_GOLOS(10);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    BOOST_CHECK_EQUAL(db->get_balance("alice", STEEM_SYMBOL), init_balance - op.amount);
    BOOST_CHECK_EQUAL(db->get_dynamic_global_properties().total_worker_fund_steem, init_fund + block_add*2 + op.amount);

    BOOST_TEST_MESSAGE("-- Trying fund more than have");

    op.amount = db->get_balance("alice", STEEM_SYMBOL);
    op.amount += ASSET_GOLOS(1);
    GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
        CHECK_ERROR(tx_invalid_operation, 0,
            CHECK_ERROR(insufficient_funds, "alice", "fund", op.amount)));
}

BOOST_AUTO_TEST_CASE(worker_cashout) {
    BOOST_TEST_MESSAGE("Testing: worker_cashout");

    ACTORS((alice)(bob)(carol))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Creating techspec and approving payments");

    comment_create("alice", alice_private_key, "alice-proposal", "", "alice-proposal");

    worker_proposal("alice", alice_private_key, "alice-proposal", worker_proposal_type::task);
    generate_block();

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.worker_proposal_author = "alice";
    wtop.worker_proposal_permlink = "alice-proposal";
    wtop.specification_cost = ASSET_GOLOS(4);
    wtop.development_cost = ASSET_GOLOS(12);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 4;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        worker_techspec_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-techspec", worker_techspec_approve_state::approve);
        generate_block();
    }

    worker_assign("bob", bob_private_key, "bob", "bob-techspec", "carol");

    comment_create("bob", bob_private_key, "bob-result", "", "bob-result");
    worker_result("bob", bob_private_key, "bob-result", "bob-techspec");

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        worker_payment_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-techspec", worker_techspec_approve_state::approve);
        generate_block();
    }

    BOOST_TEST_MESSAGE("-- Checking cashout");

    auto init_fund = db->get_dynamic_global_properties().total_worker_fund_steem;
    auto init_block_num = db->head_block_num();

    auto* wto = db->find_worker_techspec(db->get_comment("bob", string("bob-techspec")).id);
    do
    {
        auto next_cashout_time = wto->next_cashout_time;
        auto finished_payments_count = wto->finished_payments_count;

        auto author_balance = get_balance("bob");
        auto worker_balance = get_balance("carol");

        auto fund = db->get_dynamic_global_properties().total_worker_fund_steem;
        auto block_num = db->head_block_num();

        BOOST_TEST_MESSAGE("-- Cashout");

        // Generating blocks without skipping to save revenue
        while (db->get_dynamic_global_properties().total_worker_fund_steem < wtop.specification_cost + wtop.development_cost) {
            generate_block();
        }

        generate_blocks(next_cashout_time - STEEMIT_BLOCK_INTERVAL, true); // Here is no revenue - blocks skipping
        generate_block();

        BOOST_TEST_MESSAGE("-- Checking");

        wto = db->find_worker_techspec(db->get_comment("bob", string("bob-techspec")).id);

        BOOST_TEST_MESSAGE("---- finished_payments_count");

        BOOST_CHECK_EQUAL(wto->finished_payments_count, finished_payments_count + 1);

        BOOST_TEST_MESSAGE("---- next_cashout_time");

        if (wto->finished_payments_count < wto->payments_count) {
            BOOST_CHECK_EQUAL(wto->next_cashout_time, next_cashout_time + wtop.payments_interval);
        }

        BOOST_TEST_MESSAGE("---- worker fund");

        if (wto->finished_payments_count < wto->payments_count) {
            auto reward = (wtop.specification_cost + wtop.development_cost) / wtop.payments_count;
            const auto& gpo = db->get_dynamic_global_properties();
            auto revenue = gpo.worker_revenue_per_day * (db->head_block_num() - block_num) / STEEMIT_BLOCKS_PER_DAY;
            BOOST_CHECK_EQUAL(gpo.total_worker_fund_steem, fund - reward + revenue);
        }

        BOOST_TEST_MESSAGE("---- balances");

        if (wto->finished_payments_count < wto->payments_count) {
            BOOST_CHECK_EQUAL(get_balance("bob"), author_balance + (wtop.specification_cost / wtop.payments_count));
            BOOST_CHECK_EQUAL(get_balance("carol"), worker_balance + (wtop.development_cost / wtop.payments_count));
        }

        BOOST_TEST_MESSAGE("---- virtual operations");

        auto trop = get_last_operations<techspec_reward_operation>(1)[0];
        BOOST_CHECK_EQUAL(trop.author, wtop.author);
        BOOST_CHECK_EQUAL(trop.permlink, wtop.permlink);
        BOOST_CHECK_EQUAL(trop.reward, get_balance("bob") - author_balance);

        auto wrop = get_last_operations<worker_reward_operation>(1)[0];
        BOOST_CHECK_EQUAL(wrop.worker, "carol");
        BOOST_CHECK_EQUAL(wrop.worker_techspec_author, wtop.author);
        BOOST_CHECK_EQUAL(wrop.worker_techspec_permlink, wtop.permlink);
        BOOST_CHECK_EQUAL(wrop.reward, get_balance("carol") - worker_balance);

        BOOST_TEST_MESSAGE("---- state");

        if (wto->finished_payments_count < wto->payments_count) {
            BOOST_CHECK(wto->state == worker_techspec_state::payment);
        }

        BOOST_TEST_MESSAGE("---- consumption");

        if (wto->finished_payments_count < wto->payments_count) {
            BOOST_CHECK_NE(db->get_dynamic_global_properties().worker_consumption_per_day.amount, 0);
        }
    } while (wto->next_cashout_time != time_point_sec::maximum());

    BOOST_TEST_MESSAGE("-- Final checking");

    BOOST_TEST_MESSAGE("---- finished_payments_count");

    BOOST_CHECK_EQUAL(wto->finished_payments_count, wto->payments_count);

    BOOST_TEST_MESSAGE("---- worker fund");

    auto cost = wto->specification_cost + wto->development_cost;
    const auto& gpo = db->get_dynamic_global_properties();
    auto revenue = gpo.worker_revenue_per_day * (db->head_block_num() - init_block_num) / STEEMIT_BLOCKS_PER_DAY;
    BOOST_CHECK_EQUAL(gpo.total_worker_fund_steem, init_fund - cost + revenue);

    BOOST_TEST_MESSAGE("---- balances");

    BOOST_CHECK_EQUAL(get_balance("bob"), wto->specification_cost);
    BOOST_CHECK_EQUAL(get_balance("carol"), wto->development_cost);

    BOOST_TEST_MESSAGE("---- state");

    BOOST_CHECK(wto->state == worker_techspec_state::payment_complete);

    BOOST_TEST_MESSAGE("---- consumption");

    BOOST_CHECK_EQUAL(db->get_dynamic_global_properties().worker_consumption_per_day.amount, 0);
}

BOOST_AUTO_TEST_CASE(worker_cashout_waiting_funds) {
    BOOST_TEST_MESSAGE("Testing: worker_cashout_waiting_funds");

    ACTORS((alice)(bob)(carol))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Creating techspec and approving payments");

    comment_create("alice", alice_private_key, "alice-proposal", "", "alice-proposal");

    worker_proposal("alice", alice_private_key, "alice-proposal", worker_proposal_type::task);
    generate_block();

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.worker_proposal_author = "alice";
    wtop.worker_proposal_permlink = "alice-proposal";
    wtop.specification_cost = ASSET_GOLOS(4);
    wtop.development_cost = ASSET_GOLOS(12);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 8;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        worker_techspec_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-techspec", worker_techspec_approve_state::approve);
        generate_block();
    }

    worker_assign("bob", bob_private_key, "bob", "bob-techspec", "carol");

    comment_create("bob", bob_private_key, "bob-result", "", "bob-result");
    worker_result("bob", bob_private_key, "bob-result", "bob-techspec");

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        worker_payment_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-techspec", worker_techspec_approve_state::approve);
        generate_block();
    }

    BOOST_TEST_MESSAGE("-- Waiting for cashout");

    {
        const auto& wto = db->get_worker_techspec(db->get_comment("bob", string("bob-techspec")).id);
        generate_blocks(wto.next_cashout_time - STEEMIT_BLOCK_INTERVAL, true); // It skips blocks - no revenue
        generate_block();
    }

    BOOST_TEST_MESSAGE("-- Check cashout skipped");

    {
        const auto& wto = db->get_worker_techspec(db->get_comment("bob", string("bob-techspec")).id);
        BOOST_CHECK_EQUAL(wto.finished_payments_count, 0);
    }

    BOOST_TEST_MESSAGE("-- Calculating required amount and generating blocks to save it");

    auto cost = wtop.specification_cost + wtop.development_cost;
    auto lack = cost - db->get_dynamic_global_properties().total_worker_fund_steem;
    auto revenue = db->get_dynamic_global_properties().worker_revenue_per_day / STEEMIT_BLOCKS_PER_DAY;
    generate_blocks(lack.amount.value / revenue.amount.value + 1);

    BOOST_TEST_MESSAGE("-- Check cashout proceed");

    {
        const auto& wto = db->get_worker_techspec(db->get_comment("bob", string("bob-techspec")).id);
        BOOST_CHECK_EQUAL(wto.finished_payments_count, 1);
    }
}

BOOST_AUTO_TEST_SUITE_END()
