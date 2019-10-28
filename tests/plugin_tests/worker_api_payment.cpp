#include <boost/test/unit_test.hpp>

#include "worker_api_fixture.hpp"
#include "helpers.hpp"

#include <golos/plugins/worker_api/worker_api_plugin.hpp>

using namespace golos;
using namespace golos::protocol;
using namespace golos::plugins::worker_api;

BOOST_FIXTURE_TEST_SUITE(worker_api_plugin_payment, worker_api_fixture)

BOOST_AUTO_TEST_CASE(worker_payment_approving) {
    BOOST_TEST_MESSAGE("Testing: worker_payment_approving");

    ACTORS((alice)(bob))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_request_metadata_index, by_post>();

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 40;
    wtop.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    BOOST_TEST_MESSAGE("-- Checking payment_beginning_time is updating on final payment approve");

    auto wtmo_itr = wtmo_idx.find(db->get_comment("bob", string("bob-request")).id);
    BOOST_CHECK_EQUAL(wtmo_itr->payment_beginning_time, time_point_sec::min());

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        worker_request_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-request", worker_request_approve_state::approve);
        generate_block();
    }

    wtmo_itr = wtmo_idx.find(db->get_comment("bob", string("bob-request")).id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_NE(wtmo_itr->payment_beginning_time, time_point_sec::min());
    BOOST_CHECK_NE(wtmo_itr->consumption_per_day.amount, 0);
}

BOOST_AUTO_TEST_CASE(worker_payment_storing_consumption) {
    BOOST_TEST_MESSAGE("Testing: worker_payment_storing_consumption");

    ACTORS((alice)(bob)(carol))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_request_metadata_index, by_post>();

    BOOST_TEST_MESSAGE("-- Creating request and approving payments");

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.specification_cost = ASSET_GOLOS(4);
    wtop.development_cost = ASSET_GOLOS(12);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 4;
    wtop.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        worker_request_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-request", worker_request_approve_state::approve);
        generate_block();
    }

    BOOST_TEST_MESSAGE("-- Checking cashout");

    auto* wto = db->find_worker_request(db->get_comment("bob", string("bob-request")).id);
    do
    {
        BOOST_TEST_MESSAGE("-- Checking consumption");

        auto wtmo_itr = wtmo_idx.find(db->get_comment("bob", string("bob-request")).id);
        BOOST_CHECK_NE(wtmo_itr->consumption_per_day.amount, 0);

        auto next_cashout_time = wto->next_cashout_time;

        BOOST_TEST_MESSAGE("-- Cashout");

        // Generating blocks without skipping to save revenue
        while (db->get_dynamic_global_properties().total_worker_fund_steem < wtop.specification_cost + wtop.development_cost) {
            generate_block();
        }

        generate_blocks(next_cashout_time - STEEMIT_BLOCK_INTERVAL, true); // Here is no revenue - blocks skipping
        generate_block();

        wto = db->find_worker_request(db->get_comment("bob", string("bob-request")).id);
    } while (wto->next_cashout_time != time_point_sec::maximum());

    auto wtmo_itr = wtmo_idx.find(db->get_comment("bob", string("bob-request")).id);
    BOOST_CHECK_EQUAL(wtmo_itr->consumption_per_day.amount, 0);
}

BOOST_AUTO_TEST_SUITE_END()
