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
    op.specification_cost = ASSET_GOLOS(6);
    op.development_cost = ASSET_GOLOS(60);
    op.payments_interval = 60*60*24*2;
    op.payments_count = 2;
    op.worker = "bob";
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
    op.specification_cost = ASSET_GOLOS(6);
    op.development_cost = ASSET_GOLOS(60);
    op.payments_interval = 60*60*24*2;
    op.payments_count = 2;
    op.worker = "alice";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    const auto& wto_post = db->get_comment("bob", string("bob-request"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->modified, fc::time_point_sec::min());

    BOOST_TEST_MESSAGE("-- Modifying worker request");

    auto now = db->head_block_time();

    op.payments_count = 3;
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
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24*2;
    wtop.payments_count = 2;
    wtop.worker = "bob";
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
    auto private_key = create_approvers(0, 1);
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_request_metadata_index, by_post>();

    BOOST_TEST_MESSAGE("-- Creating request without approves");

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24*2;
    wtop.payments_count = 2;
    wtop.worker = "bob";
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

    BOOST_TEST_MESSAGE("-- Creating request with 1 approve");

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    worker_request_approve_operation wtaop;
    wtaop.approver = "approver0";
    wtaop.author = "bob";
    wtaop.permlink = "bob-request";
    wtaop.state = worker_request_approve_state::approve;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, wtaop));
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
    auto approver_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_request_metadata_index, by_post>();

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24*2;
    wtop.payments_count = 2;
    wtop.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Checking approves are 0 before approving");

    const auto& wto_post = db->get_comment("bob", string("bob-request"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->approves, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, 0);

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    BOOST_TEST_MESSAGE("-- Approving worker request (after abstain)");

    worker_request_approve_operation op;
    op.approver = "approver0";
    op.author = "bob";
    op.permlink = "bob-request";
    op.state = worker_request_approve_state::approve;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, 1);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, 0);

    BOOST_TEST_MESSAGE("-- Disapproving worker request (after approve)");

    op.state = worker_request_approve_state::disapprove;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, 1);

    BOOST_TEST_MESSAGE("-- Abstaining worker request (after disapprove)");

    op.state = worker_request_approve_state::abstain;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, 0);

    generate_block();

    BOOST_TEST_MESSAGE("-- Disapproving worker request (after abstain)");

    op.state = worker_request_approve_state::disapprove;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, 1);

    BOOST_TEST_MESSAGE("-- Approving worker request (after disapprove)");

    op.state = worker_request_approve_state::approve;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, 1);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, 0);

    BOOST_TEST_MESSAGE("-- Abstaining worker request (after approve)");

    op.state = worker_request_approve_state::abstain;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, 0);

    {
        BOOST_TEST_MESSAGE("-- Approving request and checking metadata fields");

        comment_create("dave", dave_private_key, "dave-request", "", "dave-request");

        wtop.author = "dave";
        wtop.permlink = "dave-request";
        wtop.worker = "dave";
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, dave_private_key, wtop));
        generate_block();

        auto now = approve_request_final("dave", "dave-request", approver_key);

        wtmo_itr = wtmo_idx.find(db->get_comment("dave", string("dave-request")).id);
        BOOST_CHECK_NE(wtmo_itr->payment_beginning_time, now + (wtop.payments_interval / wtop.payments_count));
    }

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_request_approve_premade) {
    BOOST_TEST_MESSAGE("Testing: worker_request_approve_premade");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_request_metadata_index, by_post>();

    comment_create("alice", alice_private_key, "alice-premade", "", "alice-premade");

    worker_request_operation wtop;
    wtop.author = "alice";
    wtop.permlink = "alice-premade";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 40;
    wtop.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, wtop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Approving request and checking metadata fields");

    auto approver_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_blocks(STEEMIT_MAX_WITNESSES);
    auto now = approve_request_final("alice", "alice-premade", approver_key);

    auto wtmo_itr = wtmo_idx.find(db->get_comment("alice", string("alice-premade")).id);
    BOOST_CHECK_EQUAL(wtmo_itr->payment_beginning_time, now + wtop.payments_interval);
}

BOOST_AUTO_TEST_SUITE_END()
