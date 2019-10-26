#include <boost/test/unit_test.hpp>

#include "worker_fixture.hpp"
#include "helpers.hpp"

#include <golos/plugins/worker_api/worker_api_plugin.hpp>

using namespace golos;
using namespace golos::protocol;
using namespace golos::plugins::worker_api;

struct worker_api_fixture : public golos::chain::worker_fixture {
    worker_api_fixture() : golos::chain::worker_fixture() {
        database_fixture::initialize<worker_api_plugin>();
        open_database();
        startup();
    }
};

BOOST_FIXTURE_TEST_SUITE(worker_api_plugin_payment, worker_api_fixture)

BOOST_AUTO_TEST_CASE(worker_payment_approving) {
    BOOST_TEST_MESSAGE("Testing: worker_payment_approving");

    ACTORS((alice)(bob))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_techspec_metadata_index, by_post>();

    comment_create("alice", alice_private_key, "alice-proposal", "", "alice-proposal");

    worker_proposal("alice", alice_private_key, "alice-proposal", worker_proposal_type::task);
    generate_block();

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

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        worker_techspec_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-techspec", worker_techspec_approve_state::approve);
        generate_block();
    }

    worker_assign("bob", bob_private_key, "bob", "bob-techspec", "alice");

    comment_create("bob", bob_private_key, "bob-result", "", "bob-result");
    worker_result("bob", bob_private_key, "bob-result", "bob-techspec");
    generate_block();

    BOOST_TEST_MESSAGE("-- Checking approves are 0 before approving");

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->worker_payment_approves, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->worker_payment_disapproves, 0);

    BOOST_CHECK_EQUAL(wtmo_itr->payment_beginning_time, time_point_sec::min());

    BOOST_TEST_MESSAGE("-- Checking approves are updating when approving");

    worker_payment_approve("approver0", private_key,
        "bob", "bob-techspec", worker_techspec_approve_state::disapprove);
    generate_block();

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->worker_payment_approves, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->worker_payment_disapproves, 1);

    worker_payment_approve("approver0", private_key,
        "bob", "bob-techspec", worker_techspec_approve_state::approve);
    generate_block();

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->worker_payment_approves, 1);
    BOOST_CHECK_EQUAL(wtmo_itr->worker_payment_disapproves, 0);

    BOOST_TEST_MESSAGE("-- Checking payment_beginning_time is updating on final payment approve");

    BOOST_CHECK_EQUAL(wtmo_itr->payment_beginning_time, time_point_sec::min());

    for (auto i = 1; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        worker_payment_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-techspec", worker_techspec_approve_state::approve);
        generate_block();
    }

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_NE(wtmo_itr->payment_beginning_time, time_point_sec::min());
    BOOST_CHECK_NE(wtmo_itr->consumption_per_day.amount, 0);
}

BOOST_AUTO_TEST_CASE(worker_payment_closed_by_witnesses) {
    BOOST_TEST_MESSAGE("Testing: worker_payment_closed_by_witnesses");

    ACTORS((alice)(bob))
    auto private_key = create_approvers(0, STEEMIT_SUPER_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_techspec_metadata_index, by_post>();

    comment_create("alice", alice_private_key, "alice-proposal", "", "alice-proposal");

    worker_proposal("alice", alice_private_key, "alice-proposal", worker_proposal_type::task);
    generate_block();

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

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        worker_techspec_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-techspec", worker_techspec_approve_state::approve);
        generate_block();
    }

    worker_assign("bob", bob_private_key, "bob", "bob-techspec", "alice");

    comment_create("bob", bob_private_key, "bob-result", "", "bob-result");
    worker_result("bob", bob_private_key, "bob-result", "bob-techspec");
    generate_block();

    for (auto i = 0; i < STEEMIT_SUPER_MAJOR_VOTED_WITNESSES; ++i) {
        worker_payment_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-techspec", worker_techspec_approve_state::disapprove);
        generate_block();
    }

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->payment_beginning_time, time_point_sec::min());
    BOOST_CHECK_EQUAL(wtmo_itr->consumption_per_day.amount, 0);
}

BOOST_AUTO_TEST_CASE(worker_payment_disapproved_by_witnesses) {
    BOOST_TEST_MESSAGE("Testing: worker_payment_disapproved_by_witnesses");

    ACTORS((alice)(bob))
    auto private_key = create_approvers(0, STEEMIT_SUPER_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_techspec_metadata_index, by_post>();

    comment_create("alice", alice_private_key, "alice-proposal", "", "alice-proposal");

    worker_proposal("alice", alice_private_key, "alice-proposal", worker_proposal_type::task);
    generate_block();

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

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        worker_techspec_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-techspec", worker_techspec_approve_state::approve);
        generate_block();
    }

    worker_assign("bob", bob_private_key, "bob", "bob-techspec", "alice");

    comment_create("bob", bob_private_key, "bob-result", "", "bob-result");
    worker_result("bob", bob_private_key, "bob-result", "bob-techspec");
    generate_block();

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        worker_payment_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-techspec", worker_techspec_approve_state::approve);
        generate_block();
    }

    for (auto i = 0; i < STEEMIT_SUPER_MAJOR_VOTED_WITNESSES; ++i) {
        worker_payment_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-techspec", worker_techspec_approve_state::disapprove);
        generate_block();
    }

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->consumption_per_day.amount, 0);
}

BOOST_AUTO_TEST_CASE(worker_payment_storing_consumption) {
    BOOST_TEST_MESSAGE("Testing: worker_payment_storing_consumption");

    ACTORS((alice)(bob)(carol))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_techspec_metadata_index, by_post>();

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

    auto* wto = db->find_worker_techspec(db->get_comment("bob", string("bob-techspec")).id);
    do
    {
        BOOST_TEST_MESSAGE("-- Checking consumption");

        auto wtmo_itr = wtmo_idx.find(db->get_comment("bob", string("bob-techspec")).id);
        BOOST_CHECK_NE(wtmo_itr->consumption_per_day.amount, 0);

        auto next_cashout_time = wto->next_cashout_time;

        BOOST_TEST_MESSAGE("-- Cashout");

        // Generating blocks without skipping to save revenue
        while (db->get_dynamic_global_properties().total_worker_fund_steem < wtop.specification_cost + wtop.development_cost) {
            generate_block();
        }

        generate_blocks(next_cashout_time - STEEMIT_BLOCK_INTERVAL, true); // Here is no revenue - blocks skipping
        generate_block();

        wto = db->find_worker_techspec(db->get_comment("bob", string("bob-techspec")).id);
    } while (wto->next_cashout_time != time_point_sec::maximum());

    auto wtmo_itr = wtmo_idx.find(db->get_comment("bob", string("bob-techspec")).id);
    BOOST_CHECK_EQUAL(wtmo_itr->consumption_per_day.amount, 0);
}

BOOST_AUTO_TEST_SUITE_END()
