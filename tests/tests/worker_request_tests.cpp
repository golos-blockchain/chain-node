#include <boost/test/unit_test.hpp>

#include "worker_fixture.hpp"
#include "helpers.hpp"

#include <golos/protocol/worker_operations.hpp>
#include <golos/chain/worker_objects.hpp>

using namespace golos;
using namespace golos::protocol;
using namespace golos::chain;

BOOST_FIXTURE_TEST_SUITE(worker_request_tests, worker_fixture)

BOOST_AUTO_TEST_CASE(worker_authorities) {
    BOOST_TEST_MESSAGE("Testing: worker_authorities");

    {
        worker_request_operation op;
        op.author = "bob";
        op.permlink = "bob-request";
        op.worker = "bob";
        op.required_amount_min = ASSET_GOLOS(6000);
        op.required_amount_max = ASSET_GOLOS(60000);
        op.duration = fc::days(5).to_seconds();
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"bob"}));
    }

    {
        worker_request_delete_operation op;
        op.author = "bob";
        op.permlink = "bob-request";
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"bob"}));
    }

    {
        worker_request_vote_operation op;
        op.voter = "alice";
        op.author = "bob";
        op.permlink = "bob-request";
        op.vote_percent = 50*STEEMIT_1_PERCENT;
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"alice"}));
    }
}

BOOST_AUTO_TEST_CASE(worker_request_validate) {
    BOOST_TEST_MESSAGE("Testing: worker_request_validate");

    BOOST_TEST_MESSAGE("-- Normal case");

    worker_request_operation op;
    op.author = "bob";
    op.permlink = "request-permlink";
    op.worker = "bob";
    op.required_amount_min = ASSET_GOLOS(6000);
    op.required_amount_max = ASSET_GOLOS(60000);
    op.duration = fc::days(5).to_seconds();
    CHECK_OP_VALID(op);

    BOOST_TEST_MESSAGE("-- Incorrect account or permlink case");

    CHECK_PARAM_INVALID(op, author, "");
    CHECK_PARAM_INVALID(op, permlink, std::string(STEEMIT_MAX_PERMLINK_LENGTH+1, ' '));

    BOOST_TEST_MESSAGE("-- Incorrect worker");

    CHECK_PARAM_INVALID(op, worker, "-");

    BOOST_TEST_MESSAGE("-- No worker");

    CHECK_PARAM_INVALID(op, worker, "");

    BOOST_TEST_MESSAGE("-- Different-symbol costs case");

    CHECK_PARAM_INVALID(op, required_amount_max, ASSET_GBG(6000));

    BOOST_TEST_MESSAGE("-- GBG cost case");

    op.required_amount_max = ASSET_GBG(6000);
    CHECK_PARAM_VALID(op, required_amount_min, ASSET_GBG(6000));
    op.required_amount_max = ASSET_GOLOS(60000);

    BOOST_TEST_MESSAGE("-- Non-GOLOS/GBG cost case");

    op.required_amount_max = ASSET_GESTS(60000);
    CHECK_PARAM_INVALID(op, required_amount_min, ASSET_GESTS(6000));
    op.required_amount_max = ASSET_GOLOS(60000);

    BOOST_TEST_MESSAGE("-- Zero cost case");

    CHECK_PARAM_INVALID(op, required_amount_min, ASSET_GOLOS(0));

    BOOST_TEST_MESSAGE("-- required_amount_max < required_amount_min");

    CHECK_PARAM_INVALID(op, required_amount_max, ASSET_GOLOS(1));

    BOOST_TEST_MESSAGE("-- required_amount_max = required_amount_max");

    CHECK_PARAM_VALID(op, required_amount_min, op.required_amount_max);

    BOOST_TEST_MESSAGE("-- vest_reward");

    CHECK_PARAM_VALID(op, vest_reward, true);

    op.required_amount_min = ASSET_GBG(6000);
    op.required_amount_max = ASSET_GBG(6000);
    CHECK_PARAM_INVALID(op, vest_reward, true);

    BOOST_TEST_MESSAGE("-- Duration");

    CHECK_PARAM_INVALID(op, duration, GOLOS_WORKER_REQUEST_MIN_DURATION - 1);
    CHECK_PARAM_INVALID(op, duration, fc::days(30).to_seconds() + 1);
    CHECK_PARAM_VALID(op, duration, fc::days(30).to_seconds());
}

BOOST_AUTO_TEST_CASE(worker_request_apply_create) {
    BOOST_TEST_MESSAGE("Testing: worker_request_apply_create");

    ACTORS((alice)(bob)(carol)(dave)(eve)(fred)(greta))
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Create worker request with no post case");

    worker_request_operation op;
    op.author = "bob";
    op.permlink = "bob-request";
    op.worker = "bob";
    op.required_amount_min = ASSET_GOLOS(6000);
    op.required_amount_max = ASSET_GOLOS(60000);
    op.duration = fc::days(5).to_seconds();
    GOLOS_CHECK_ERROR_MISSING(comment, make_comment_id("bob", "bob-request"), bob_private_key, op);
    generate_block();

    BOOST_TEST_MESSAGE("-- Create worker request on comment instead of post case");

    comment_create("alice", alice_private_key, "the-post", "", "the-post");
    comment_create("carol", carol_private_key, "i-am-comment", "alice", "the-post");

    op.author = "carol";
    op.permlink = "i-am-comment";
    GOLOS_CHECK_ERROR_LOGIC(post_is_not_root, carol_private_key, op);
    generate_block();

    BOOST_TEST_MESSAGE("-- Creating request with not-exist worker");

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    op.author = "bob";
    op.permlink = "bob-request";
    op.worker = "notexistacc";
    GOLOS_CHECK_ERROR_MISSING(account, "notexistacc", bob_private_key, op);

    BOOST_TEST_MESSAGE("-- Create worker request without GBG for fee");

    op.worker = "bob";
    GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_private_key, op),
        CHECK_ERROR(tx_invalid_operation, 0,
            CHECK_ERROR(insufficient_funds, "bob", "fund", "100.000 GBG")));

    BOOST_TEST_MESSAGE("-- Normal create worker request case");

    fund("bob", ASSET_GBG(101));
    const auto& created = db->head_block_time();
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    BOOST_CHECK_EQUAL(db->get_balance("bob", SBD_SYMBOL), ASSET_GBG(1));
    BOOST_CHECK_EQUAL(db->get_balance("workers", SBD_SYMBOL), ASSET_GBG(100));

    const auto& wro_post = db->get_comment_by_perm("bob", string("bob-request"));
    const auto& wro = db->get_worker_request(wro_post.id);
    BOOST_CHECK_EQUAL(wro.post, wro_post.id);
    BOOST_CHECK_EQUAL(wro.worker, op.worker);
    BOOST_CHECK_EQUAL(wro.state, worker_request_state::created);
    BOOST_CHECK_EQUAL(wro.required_amount_min, op.required_amount_min);
    BOOST_CHECK_EQUAL(wro.required_amount_max, op.required_amount_max);
    BOOST_CHECK_EQUAL(wro.vest_reward, false);
    BOOST_CHECK_EQUAL(wro.duration, op.duration);
    BOOST_CHECK_EQUAL(wro.vote_end_time, created + op.duration);

    {
        BOOST_TEST_MESSAGE("-- Check cannot create worker request on post outside cashout window");

        comment_create("greta", greta_private_key, "greta-request", "", "greta-request");

        generate_blocks(db->head_block_time() + STEEMIT_CASHOUT_WINDOW_SECONDS + STEEMIT_BLOCK_INTERVAL, true);

        op.author = "greta";
        op.permlink = "greta-request";
        GOLOS_CHECK_ERROR_LOGIC(post_should_be_in_cashout_window, greta_private_key, op);
    }

    {
        BOOST_TEST_MESSAGE("-- Check can create worker request with VESTS reward");

        comment_create("dave", dave_private_key, "dave-request", "", "dave-request");

        fund("dave", ASSET_GBG(100));
        op.author = "dave";
        op.permlink = "dave-request";
        op.vest_reward = true;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, dave_private_key, op));
        BOOST_CHECK(db->get_worker_request(db->get_comment_by_perm("dave", string("dave-request")).id).vest_reward);
    }

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_request_apply_modify) {
    BOOST_TEST_MESSAGE("Testing: worker_request_apply_modify");

    ACTORS((alice)(bob)(carol))
    generate_block();

    signed_transaction tx;

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    fund("bob", ASSET_GBG(100));
    worker_request_operation op;
    op.author = "bob";
    op.permlink = "bob-request";
    op.worker = "alice";
    op.required_amount_min = ASSET_GOLOS(6000);
    op.required_amount_max = ASSET_GOLOS(60000);
    op.duration = fc::days(5).to_seconds();
    const auto& created = db->head_block_time();
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    BOOST_TEST_MESSAGE("-- Modify required_amount_min and required_amount_max");

    op.required_amount_min = ASSET_GBG(123);
    op.required_amount_max = ASSET_GBG(456);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    {
        const auto& wro_post = db->get_comment_by_perm("bob", string("bob-request"));
        const auto& wro = db->get_worker_request(wro_post.id);
        BOOST_CHECK_EQUAL(wro.required_amount_min, op.required_amount_min);
        BOOST_CHECK_EQUAL(wro.required_amount_max, op.required_amount_max);
    }

    BOOST_TEST_MESSAGE("-- Modify duration");

    op.duration = fc::days(7).to_seconds();
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    {
        const auto& wro_post = db->get_comment_by_perm("bob", string("bob-request"));
        const auto& wro = db->get_worker_request(wro_post.id);
        BOOST_CHECK_EQUAL(wro.duration, op.duration);
        BOOST_CHECK_EQUAL(wro.vote_end_time, created + op.duration);
    }

    BOOST_TEST_MESSAGE("-- Change worker");

    op.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    {
        const auto& wro_post = db->get_comment_by_perm("bob", string("bob-request"));
        const auto& wro = db->get_worker_request(wro_post.id);
        BOOST_CHECK_EQUAL(wro.worker, "bob");
    }

    BOOST_TEST_MESSAGE("-- Check can modify request if another voted");

    comment_create("carol", carol_private_key, "carol-request", "", "carol-request");

    fund("carol", ASSET_GBG(100));
    op.author = "carol";
    op.permlink = "carol-request";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, op));
    generate_block();
    worker_request_vote_operation wrvop;
    wrvop.voter = "alice";
    wrvop.author = "carol";
    wrvop.permlink = "carol-request";
    wrvop.vote_percent = -1;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, wrvop));
    generate_block();
    op.author = "bob";
    op.permlink = "bob-request";
    op.required_amount_min = ASSET_GBG(10);
    op.required_amount_max = ASSET_GBG(10);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    BOOST_TEST_MESSAGE("-- Check cannot modify voted request");

    op.author = "carol";
    op.permlink = "carol-request";
    op.required_amount_min = ASSET_GBG(10000000);
    op.required_amount_max = ASSET_GBG(10000000);
    GOLOS_CHECK_ERROR_LOGIC(cannot_edit_voted, carol_private_key, op);

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_request_vote_validate) {
    BOOST_TEST_MESSAGE("Testing: worker_request_vote_validate");

    BOOST_TEST_MESSAGE("-- Normal case");

    worker_request_vote_operation op;
    op.voter = "alice";
    op.author = "bob";
    op.permlink = "request-permlink";
    op.vote_percent = -STEEMIT_1_PERCENT;
    CHECK_OP_VALID(op);

    BOOST_TEST_MESSAGE("-- Incorrect account or permlink case");

    CHECK_PARAM_INVALID(op, voter, "");
    CHECK_PARAM_INVALID(op, author, "");
    CHECK_PARAM_INVALID(op, permlink, std::string(STEEMIT_MAX_PERMLINK_LENGTH+1, ' '));

    BOOST_TEST_MESSAGE("-- Invalid vote_percent case");

    CHECK_PARAM_INVALID(op, vote_percent, -STEEMIT_100_PERCENT-1);
    CHECK_PARAM_INVALID(op, vote_percent, STEEMIT_100_PERCENT+1);
}

BOOST_AUTO_TEST_CASE(worker_request_vote_apply_combinations) {
    BOOST_TEST_MESSAGE("Testing: worker_request_vote_apply_combinations");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    fund("bob", ASSET_GBG(100));
    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.worker = "alice";
    wtop.required_amount_min = ASSET_GOLOS(6000);
    wtop.required_amount_max = ASSET_GOLOS(60000);
    wtop.duration = fc::days(5).to_seconds();
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Abstaining non-voted request case");

    worker_request_vote_operation op;
    op.voter = "alice";
    op.author = "bob";
    op.permlink = "bob-request";
    op.vote_percent = 0;
    GOLOS_CHECK_ERROR_LOGIC(already_voted_in_similar_way, alice_private_key, op);

    auto check_votes = [&](int upvote_count, int downvote_count) {
        const auto& post = db->get_comment_by_perm("bob", string("bob-request"));
        uint32_t upvotes = 0;
        uint32_t downvotes = 0;
        const auto& vote_idx = db->get_index<worker_request_vote_index, by_request_voter>();
        auto vote_itr = vote_idx.lower_bound(db->get_comment_by_perm("bob", string("bob-request")).id);
        for (; vote_itr != vote_idx.end() && vote_itr->post == post.id; ++vote_itr) {
            (vote_itr->vote_percent > 0) ? upvotes++ : downvotes++;
        }
        BOOST_CHECK_EQUAL(upvotes, upvote_count);
        BOOST_CHECK_EQUAL(downvotes, downvote_count);
    };

    BOOST_TEST_MESSAGE("-- Upvoting request (after abstain)");

    check_votes(0, 0);

    op.vote_percent = 1;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    check_votes(1, 0);

    BOOST_TEST_MESSAGE("-- Repeating vote request case");

    GOLOS_CHECK_ERROR_LOGIC(already_voted_in_similar_way, alice_private_key, op);

    BOOST_TEST_MESSAGE("-- Downvoting request (after vote)");

    op.vote_percent = -1;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    check_votes(0, 1);

    BOOST_TEST_MESSAGE("-- Repeating disvote request case");

    GOLOS_CHECK_ERROR_LOGIC(already_voted_in_similar_way, alice_private_key, op);

    BOOST_TEST_MESSAGE("-- Upvoting request (after downvote)");

    op.vote_percent = 1;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    check_votes(1, 0);

    BOOST_TEST_MESSAGE("-- Abstaining request (after upvote)");

    op.vote_percent = 0;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    check_votes(0, 0);

    BOOST_TEST_MESSAGE("-- Downvoting request (after abstain)");

    op.vote_percent =-1;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    check_votes(0, 1);

    BOOST_TEST_MESSAGE("-- Abstaining request (after downvote)");

    op.vote_percent = 0;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    check_votes(0, 0);
}

BOOST_AUTO_TEST_CASE(worker_request_vote_apply_approve) {
    BOOST_TEST_MESSAGE("Testing: worker_request_vote_apply_approve");

    ACTORS((alice)(bob)(carol)(dave)(frad))
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Creating bob request");

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    fund("bob", ASSET_GBG(100));
    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.worker = "bob";
    wtop.required_amount_min = ASSET_GOLOS(6);
    wtop.required_amount_max = ASSET_GOLOS(60);
    wtop.duration = fc::days(5).to_seconds();
    const auto& created = db->head_block_time();
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    BOOST_CHECK_EQUAL(db->get_worker_request(db->get_comment_by_perm("bob", string("bob-request")).id).vote_end_time,
        created + wtop.duration);

    BOOST_TEST_MESSAGE("-- Creating carol request in same block");

    comment_create("carol", carol_private_key, "carol-request", "", "carol-request");

    fund("carol", ASSET_GBG(100));
    wtop.author = "carol";
    wtop.permlink = "carol-request";
    wtop.worker = "carol";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, wtop));
    BOOST_CHECK_EQUAL(db->get_worker_request(db->get_comment_by_perm("carol", string("carol-request")).id).vote_end_time,
        created + wtop.duration);

    BOOST_TEST_MESSAGE("-- Creating dave request in same block");

    comment_create("dave", dave_private_key, "dave-request", "", "dave-request");

    fund("dave", ASSET_GBG(100));
    wtop.author = "dave";
    wtop.permlink = "dave-request";
    wtop.worker = "dave";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, dave_private_key, wtop));

    BOOST_TEST_MESSAGE("-- Creating frad request in same block");

    comment_create("frad", frad_private_key, "frad-request", "", "frad-request");

    fund("frad", ASSET_GBG(100));
    wtop.author = "frad";
    wtop.permlink = "frad-request";
    wtop.worker = "frad";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, frad_private_key, wtop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Upvoting bob request by alice");

    worker_request_vote_operation op;
    op.voter = "alice";
    op.author = "bob";
    op.permlink = "bob-request";
    op.vote_percent = STEEMIT_100_PERCENT;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

    BOOST_TEST_MESSAGE("-- Upvoting carol request by enough stake-holders");

    auto upvote_request = [&](account_name_type author, std::string permlink, int16_t vote_percent) {
        op.voter = "bob";
        op.author = author;
        op.permlink = permlink;
        op.vote_percent = vote_percent;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
        op.voter = "carol";
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, op));
        op.voter = "alice";
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    };
    upvote_request("carol", "carol-request", STEEMIT_100_PERCENT);

    BOOST_TEST_MESSAGE("-- Upvoting dave request by enough stake-holders");

    upvote_request("dave", "dave-request", 50*STEEMIT_1_PERCENT);

    BOOST_TEST_MESSAGE("-- Upvoting frad request by enough stake-holders, but too low weight");

    upvote_request("frad", "frad-request", 5*STEEMIT_1_PERCENT);

    BOOST_TEST_MESSAGE("-- Enabling clearing of votes and waiting for (vote_term - 1 block)");

    generate_blocks(db->get_comment_by_perm("bob", string("bob-request")).created
        + wtop.duration - STEEMIT_BLOCK_INTERVAL, true);

    db->set_clear_old_worker_votes(true);

    BOOST_TEST_MESSAGE("-- Checking bob request opened and vote still exists");

    {
        const auto& wro_post = db->get_comment_by_perm("bob", string("bob-request"));
        const auto& wro = db->get_worker_request(wro_post.id);
        BOOST_CHECK(wro.state != worker_request_state::closed_by_expiration);

        const auto& wrvo_idx = db->get_index<worker_request_vote_index, by_request_voter>();
        BOOST_CHECK(wrvo_idx.find(wro_post.id) != wrvo_idx.end());
    }

    BOOST_TEST_MESSAGE("-- Waiting for 1 block to expire both requests vote term");

    generate_block();

    BOOST_TEST_MESSAGE("-- Checking bob request closed and vote cleared");

    {
        const auto& wro_post = db->get_comment_by_perm("bob", string("bob-request"));
        const auto& wro = db->get_worker_request(wro_post.id);
        BOOST_CHECK_EQUAL(wro.state, worker_request_state::closed_by_expiration);

        const auto& wrvo_idx = db->get_index<worker_request_vote_index, by_request_voter>();
        BOOST_CHECK(wrvo_idx.find(wro_post.id) == wrvo_idx.end());

        BOOST_TEST_MESSAGE("-- Checking virtual operation pushed");

        auto wsop = get_last_operations<worker_state_operation>(4)[3];
        BOOST_CHECK_EQUAL(wsop.author, wro_post.author);
        BOOST_CHECK_EQUAL(wsop.hashlink, wro_post.hashlink);
        BOOST_CHECK_EQUAL(wsop.state, worker_request_state::closed_by_expiration);
    }

    BOOST_TEST_MESSAGE("-- Checking carol request is not closed and has votes");

    {
        const auto& wro_post = db->get_comment_by_perm("carol", string("carol-request"));
        const auto& wro = db->get_worker_request(wro_post.id);
        BOOST_CHECK_EQUAL(wro.state, worker_request_state::payment);
        BOOST_CHECK_EQUAL(wro.remaining_payment, wro.required_amount_max);

        BOOST_TEST_MESSAGE("-- Checking carol request has votes (it should, clearing is enabled after final vote)");

        const auto& wrvo_idx = db->get_index<worker_request_vote_index, by_request_voter>();
        BOOST_CHECK(wrvo_idx.find(wro_post.id) != wrvo_idx.end());

        BOOST_TEST_MESSAGE("-- Checking virtual operation pushed");

        auto wsop = get_last_operations<worker_state_operation>(4)[2];
        BOOST_CHECK_EQUAL(wsop.author, wro_post.author);
        BOOST_CHECK_EQUAL(wsop.hashlink, wro_post.hashlink);
        BOOST_CHECK_EQUAL(wsop.state, worker_request_state::payment);
    }

    BOOST_TEST_MESSAGE("-- Checking dave request is not closed and has votes");

    {
        const auto& wro_post = db->get_comment_by_perm("dave", string("dave-request"));
        const auto& wro = db->get_worker_request(wro_post.id);
        BOOST_CHECK_EQUAL(wro.state, worker_request_state::payment);
        BOOST_CHECK_EQUAL(wro.remaining_payment, ASSET_GOLOS(30));

        BOOST_TEST_MESSAGE("-- Checking dave request has votes (it should, clearing is enabled after final vote)");

        const auto& wrvo_idx = db->get_index<worker_request_vote_index, by_request_voter>();
        BOOST_CHECK(wrvo_idx.find(wro_post.id) != wrvo_idx.end());

        BOOST_TEST_MESSAGE("-- Checking virtual operation pushed");

        auto wsop = get_last_operations<worker_state_operation>(4)[1];
        BOOST_CHECK_EQUAL(wsop.author, wro_post.author);
        BOOST_CHECK_EQUAL(wsop.hashlink, wro_post.hashlink);
        BOOST_CHECK_EQUAL(wsop.state, worker_request_state::payment);
    }

    BOOST_TEST_MESSAGE("-- Checking frad request is not closed and has votes");

    {
        const auto& wro_post = db->get_comment_by_perm("frad", string("frad-request"));
        const auto& wro = db->get_worker_request(wro_post.id);
        BOOST_CHECK_EQUAL(wro.state, worker_request_state::closed_by_voters);
        BOOST_CHECK_EQUAL(wro.remaining_payment, ASSET_GOLOS(3));

        BOOST_TEST_MESSAGE("-- Checking frad request has votes (it should, clearing is enabled after final vote)");

        const auto& wrvo_idx = db->get_index<worker_request_vote_index, by_request_voter>();
        BOOST_CHECK(wrvo_idx.find(wro_post.id) != wrvo_idx.end());

        BOOST_TEST_MESSAGE("-- Checking virtual operation pushed");

        auto wsop = get_last_operations<worker_state_operation>(4)[0];
        BOOST_CHECK_EQUAL(wsop.author, wro_post.author);
        BOOST_CHECK_EQUAL(wsop.hashlink, wro_post.hashlink);
        BOOST_CHECK_EQUAL(wsop.state, worker_request_state::closed_by_voters);
    }
}

BOOST_AUTO_TEST_CASE(worker_request_delete_validate) {
    BOOST_TEST_MESSAGE("Testing: worker_request_delete_validate");

    BOOST_TEST_MESSAGE("-- Normal case");

    worker_request_delete_operation op;
    op.author = "bob";
    op.permlink = "bob-request";
    CHECK_OP_VALID(op);

    BOOST_TEST_MESSAGE("-- Incorrect account or permlink case");

    CHECK_PARAM_INVALID(op, author, "");
    CHECK_PARAM_INVALID(op, permlink, std::string(STEEMIT_MAX_PERMLINK_LENGTH+1, ' '));
}

BOOST_AUTO_TEST_CASE(worker_request_delete_apply) {
    BOOST_TEST_MESSAGE("Testing: worker_request_delete_apply");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Checking cannot delete request with non-existant post");

    worker_request_delete_operation op;
    op.author = "bob";
    op.permlink = "bob-request";
    GOLOS_CHECK_ERROR_MISSING(comment, make_comment_id("bob", "bob-request"), bob_private_key, op);
    generate_block();

    BOOST_TEST_MESSAGE("-- Checking cannot delete non-existant request");

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    GOLOS_CHECK_ERROR_MISSING(worker_request_object, make_comment_id("bob", "bob-request"), bob_private_key, op);
    generate_block();

    BOOST_TEST_MESSAGE("-- Creating request");

    fund("bob", ASSET_GBG(100));
    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.worker = "bob";
    wtop.required_amount_min = ASSET_GOLOS(6000);
    wtop.required_amount_max = ASSET_GOLOS(60000);
    wtop.duration = fc::days(5).to_seconds();
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    {
        const auto* wro = db->find_worker_request(db->get_comment_by_perm("bob", string("bob-request")).id);
        BOOST_CHECK(wro);
    }

    BOOST_TEST_MESSAGE("-- Checking cannot delete post with request");

    delete_comment_operation dcop;
    dcop.author = "bob";
    dcop.permlink = "bob-request";
    GOLOS_CHECK_ERROR_LOGIC(cannot_delete_post_with_worker_request, bob_private_key, dcop);
    generate_block();

    BOOST_TEST_MESSAGE("-- Deleting request");

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    {
        const auto* wro = db->find_worker_request(db->get_comment_by_perm("bob", string("bob-request")).id);
        BOOST_CHECK(!wro);
    }

    BOOST_TEST_MESSAGE("-- Checking can delete post now");

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, dcop));
    generate_block();

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_request_delete_apply_closing_cases) {
    BOOST_TEST_MESSAGE("Testing: worker_request_delete_apply_closing_cases");

    ACTORS((alice)(bob)(carol)(dave))
    generate_block();

    signed_transaction tx;

    db->set_clear_old_worker_votes(true);

    worker_request_operation wtop;

    BOOST_TEST_MESSAGE("-- Creating request without votes");

    {
        comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

        fund("bob", ASSET_GBG(100));
        wtop.author = "bob";
        wtop.permlink = "bob-request";
        wtop.worker = "bob";
        wtop.required_amount_min = ASSET_GOLOS(6000);
        wtop.required_amount_max = ASSET_GOLOS(60000);
        wtop.duration = fc::days(5).to_seconds();
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
        generate_block();

        BOOST_TEST_MESSAGE("-- Deleting it");

        worker_request_delete_operation op;
        op.author = "bob";
        op.permlink = "bob-request";
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
        generate_block();

        {
            BOOST_TEST_MESSAGE("-- Checking it is deleted");

            const auto& wro_post = db->get_comment_by_perm("bob", string("bob-request"));
            const auto* wro = db->find_worker_request(wro_post.id);
            BOOST_CHECK(!wro);
        }
    }

    BOOST_TEST_MESSAGE("-- Creating request with 1 vote");

    {
        fund("bob", ASSET_GBG(100));
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
        generate_block();

        worker_request_vote("alice", alice_private_key, "bob", "bob-request", STEEMIT_100_PERCENT);
        generate_block();

        BOOST_TEST_MESSAGE("-- Deleting it");

        worker_request_delete_operation op;
        op.author = "bob";
        op.permlink = "bob-request";
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
        generate_block();

        check_request_closed(db->get_comment_by_perm("bob", string("bob-request")).id,
            worker_request_state::closed_by_author, ASSET_GOLOS(0));
    }

    BOOST_TEST_MESSAGE("-- Checking cannot close closed request");

    {
        worker_request_delete_operation op;
        op.author = "bob";
        op.permlink = "bob-request";
        GOLOS_CHECK_ERROR_LOGIC(incorrect_request_state, bob_private_key, op);
    }

    validate_database();
}

BOOST_AUTO_TEST_SUITE_END()
