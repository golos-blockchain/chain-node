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
        worker_fund_operation op;
        op.sponsor = "alice";
        op.amount = ASSET_GOLOS(10);
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"alice"}));
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

    BOOST_TEST_MESSAGE("-- Creating request and approving payments");

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.specification_cost = ASSET_GOLOS(4);
    wtop.development_cost = ASSET_GOLOS(12);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 4;
    wtop.worker = "carol";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        worker_request_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-request", worker_request_approve_state::approve);
        generate_block();
    }

    BOOST_TEST_MESSAGE("-- Checking cashout");

    auto init_fund = db->get_dynamic_global_properties().total_worker_fund_steem;
    auto init_block_num = db->head_block_num();

    auto* wto = db->find_worker_request(db->get_comment("bob", string("bob-request")).id);
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

        wto = db->find_worker_request(db->get_comment("bob", string("bob-request")).id);

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

        auto trop = get_last_operations<request_reward_operation>(1)[0];
        BOOST_CHECK_EQUAL(trop.author, wtop.author);
        BOOST_CHECK_EQUAL(trop.permlink, wtop.permlink);
        BOOST_CHECK_EQUAL(trop.reward, get_balance("bob") - author_balance);

        auto wrop = get_last_operations<worker_reward_operation>(1)[0];
        BOOST_CHECK_EQUAL(wrop.worker, "carol");
        BOOST_CHECK_EQUAL(wrop.worker_request_author, wtop.author);
        BOOST_CHECK_EQUAL(wrop.worker_request_permlink, wtop.permlink);
        BOOST_CHECK_EQUAL(wrop.reward, get_balance("carol") - worker_balance);

        BOOST_TEST_MESSAGE("---- state");

        if (wto->finished_payments_count < wto->payments_count) {
            BOOST_CHECK(wto->state == worker_request_state::payment);
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

    BOOST_CHECK(wto->state == worker_request_state::payment_complete);

    BOOST_TEST_MESSAGE("---- consumption");

    BOOST_CHECK_EQUAL(db->get_dynamic_global_properties().worker_consumption_per_day.amount, 0);
}

BOOST_AUTO_TEST_CASE(worker_cashout_waiting_funds) {
    BOOST_TEST_MESSAGE("Testing: worker_cashout_waiting_funds");

    ACTORS((alice)(bob)(carol))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Creating request and approving payments");

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.specification_cost = ASSET_GOLOS(4);
    wtop.development_cost = ASSET_GOLOS(12);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 8;
    wtop.worker = "carol";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        worker_request_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-request", worker_request_approve_state::approve);
        generate_block();
    }

    BOOST_TEST_MESSAGE("-- Waiting for cashout");

    {
        const auto& wto = db->get_worker_request(db->get_comment("bob", string("bob-request")).id);
        generate_blocks(wto.next_cashout_time - STEEMIT_BLOCK_INTERVAL, true); // It skips blocks - no revenue
        generate_block();
    }

    BOOST_TEST_MESSAGE("-- Check cashout skipped");

    {
        const auto& wto = db->get_worker_request(db->get_comment("bob", string("bob-request")).id);
        BOOST_CHECK_EQUAL(wto.finished_payments_count, 0);
    }

    BOOST_TEST_MESSAGE("-- Calculating required amount and generating blocks to save it");

    auto cost = wtop.specification_cost + wtop.development_cost;
    auto lack = cost - db->get_dynamic_global_properties().total_worker_fund_steem;
    auto revenue = db->get_dynamic_global_properties().worker_revenue_per_day / STEEMIT_BLOCKS_PER_DAY;
    generate_blocks(lack.amount.value / revenue.amount.value + 1);

    BOOST_TEST_MESSAGE("-- Check cashout proceed");

    {
        const auto& wto = db->get_worker_request(db->get_comment("bob", string("bob-request")).id);
        BOOST_CHECK_EQUAL(wto.finished_payments_count, 1);
    }
}

BOOST_AUTO_TEST_SUITE_END()
