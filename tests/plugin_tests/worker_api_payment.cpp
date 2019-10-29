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
    wtop.worker = "bob";
    wtop.required_amount_min = ASSET_GOLOS(6000);
    wtop.required_amount_max = ASSET_GOLOS(60000);
    wtop.duration = fc::days(5).to_seconds();
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
}

BOOST_AUTO_TEST_SUITE_END()
