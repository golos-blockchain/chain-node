#include <boost/test/unit_test.hpp>

#include "worker_api_fixture.hpp"
#include "helpers.hpp"

#include <golos/plugins/worker_api/worker_api_plugin.hpp>

using namespace golos;
using namespace golos::protocol;
using namespace golos::plugins::worker_api;

BOOST_FIXTURE_TEST_SUITE(worker_api_plugin_request, worker_api_fixture)

BOOST_AUTO_TEST_CASE(worker_request_create) {
    BOOST_TEST_MESSAGE("Testing: worker_request_create");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_request_metadata_index, by_post>();

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    vote_operation vop;
    vop.voter = "alice";
    vop.author = "bob";
    vop.permlink = "bob-request";
    vop.weight = STEEMIT_100_PERCENT;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, vop));
    generate_block();

    worker_request_operation op;
    op.author = "bob";
    op.permlink = "bob-request";
    op.worker = "bob";
    op.required_amount_min = ASSET_GOLOS(6000);
    op.required_amount_max = ASSET_GOLOS(60000);
    op.duration = fc::days(5).to_seconds();
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    BOOST_TEST_MESSAGE("-- Checking metadata creating");

    const auto& wto_post = db->get_comment("bob", string("bob-request"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());

    BOOST_TEST_MESSAGE("-- Checking metadata has NOT filled field modified");

    BOOST_CHECK_EQUAL(wtmo_itr->modified, fc::time_point_sec::min());

    BOOST_TEST_MESSAGE("-- Checking metadata has field net_rshares filled from post");

    BOOST_CHECK_EQUAL(wtmo_itr->net_rshares, wto_post.net_rshares);

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_request_modify) {
    BOOST_TEST_MESSAGE("Testing: worker_request_modify");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_request_metadata_index, by_post>();

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    worker_request_operation op;
    op.author = "bob";
    op.permlink = "bob-request";
    op.worker = "alice";
    op.required_amount_min = ASSET_GOLOS(6000);
    op.required_amount_max = ASSET_GOLOS(60000);
    op.duration = fc::days(5).to_seconds();
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    const auto& wto_post = db->get_comment("bob", string("bob-request"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->modified, fc::time_point_sec::min());

    BOOST_TEST_MESSAGE("-- Modifying worker request");

    auto now = db->head_block_time();

    op.required_amount_min = ASSET_GBG(6000);
    op.required_amount_max = ASSET_GBG(60000);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    wtmo_itr = wtmo_idx.find(wto_post.id);

    BOOST_TEST_MESSAGE("-- Checking metadata has updated field modified");

    BOOST_CHECK_EQUAL(wtmo_itr->modified, now);

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_request_vote) {
    BOOST_TEST_MESSAGE("Testing: worker_request_vote");

    ACTORS((alice)(bob))
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

    const auto& wto_post = db->get_comment("bob", string("bob-request"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->net_rshares, 0);

    BOOST_TEST_MESSAGE("-- Voting worker request post");

    vote_operation vop;
    vop.voter = "alice";
    vop.author = "bob";
    vop.permlink = "bob-request";
    vop.weight = STEEMIT_100_PERCENT;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, vop));
    generate_block();

    const auto& wto_post_voted = db->get_comment("bob", string("bob-request"));
    wtmo_itr = wtmo_idx.find(wto_post_voted.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());

    BOOST_TEST_MESSAGE("-- Checking metadata has updated field net_rshares from post");

    BOOST_CHECK_EQUAL(wtmo_itr->net_rshares, wto_post_voted.net_rshares);

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_request_delete) {
    BOOST_TEST_MESSAGE("Testing: worker_request_delete");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_request_metadata_index, by_post>();

    BOOST_TEST_MESSAGE("-- Creating request without upvotes");

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

    const auto& wto_post = db->get_comment("bob", string("bob-request"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());

    BOOST_TEST_MESSAGE("-- Deleting it");

    worker_request_delete_operation op;
    op.author = "bob";
    op.permlink = "bob-request";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    BOOST_TEST_MESSAGE("-- Checking worker request metadata object is deleted");

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr == wtmo_idx.end());

    BOOST_TEST_MESSAGE("-- Creating request with 1 vote");

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    worker_request_vote_operation wrvop;
    wrvop.voter = "alice";
    wrvop.author = "bob";
    wrvop.permlink = "bob-request";
    wrvop.vote_percent = 10*STEEMIT_1_PERCENT;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, wrvop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Deleting it");

    op.author = "bob";
    op.permlink = "bob-request";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    BOOST_TEST_MESSAGE("-- Checking worker request metadata object exists");

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_request_approve) {
    BOOST_TEST_MESSAGE("Testing: worker_request_approve");

    ACTORS((alice)(bob)(carol)(dave))
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_request_metadata_index, by_post>();

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.worker = "bob";
    wtop.required_amount_min = ASSET_GOLOS(6);
    wtop.required_amount_max = ASSET_GOLOS(60);
    wtop.duration = fc::days(5).to_seconds();
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Checking upvotes are 0 before approving");

    const auto& wto_post = db->get_comment("bob", string("bob-request"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->upvotes, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->downvotes, 0);

    BOOST_TEST_MESSAGE("-- Upvoting worker request (after abstain)");

    worker_request_vote_operation op;
    op.voter = "alice";
    op.author = "bob";
    op.permlink = "bob-request";
    op.vote_percent = STEEMIT_100_PERCENT;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->upvotes, 1);
    BOOST_CHECK_EQUAL(wtmo_itr->downvotes, 0);

    BOOST_TEST_MESSAGE("-- Downvoting worker request (after approve)");

    op.vote_percent = -STEEMIT_100_PERCENT;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->upvotes, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->downvotes, 1);

    BOOST_TEST_MESSAGE("-- Abstaining worker request (after disapprove)");

    op.vote_percent = 0;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->upvotes, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->downvotes, 0);

    generate_block();

    BOOST_TEST_MESSAGE("-- Downvoting worker request (after abstain)");

    op.vote_percent = -STEEMIT_100_PERCENT;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->upvotes, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->downvotes, 1);

    BOOST_TEST_MESSAGE("-- Upvoting worker request (after disapprove)");

    op.vote_percent = STEEMIT_100_PERCENT;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->upvotes, 1);
    BOOST_CHECK_EQUAL(wtmo_itr->downvotes, 0);

    BOOST_TEST_MESSAGE("-- Abstaining worker request (after approve)");

    op.vote_percent = 0;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->upvotes, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->downvotes, 0);

    validate_database();
}

BOOST_AUTO_TEST_SUITE_END()
