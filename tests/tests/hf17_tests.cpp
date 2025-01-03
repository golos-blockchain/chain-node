#ifdef STEEMIT_BUILD_TESTNET

#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include <golos/protocol/exceptions.hpp>

#include <golos/chain/database.hpp>
#include <golos/chain/hardfork.hpp>
#include <golos/chain/steem_objects.hpp>

#include <fc/crypto/digest.hpp>

#include "database_fixture.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace golos { namespace chain {

    struct hf17_database_fixture: public clean_database_fixture_wrap {
        hf17_database_fixture() : clean_database_fixture_wrap(true, [&]() {
            initialize();
            open_database();

            generate_block();
            db->set_hardfork(STEEMIT_HARDFORK_0_16);
            startup(false);
        }) {
        }

        ~hf17_database_fixture() {
        }

        uint128_t quadratic_curve(uint128_t rshares) const {
            auto s = _db.get_content_constant_s();
            return (rshares + s) * (rshares + s) - s * s;
        }

        uint128_t linear_curve(uint128_t rshares) {
            return rshares;
        }

    }; // struct hf17_database_fixture
} } // namespace golos::chain

using namespace golos;
using namespace golos::chain;
using namespace golos::protocol;
using std::string;

// Requires build with MAX_19_VOTED_WITNESSES=TRUE
BOOST_FIXTURE_TEST_SUITE(hf17_tests, hf17_database_fixture)

    BOOST_AUTO_TEST_CASE(hf17_linear_reward_curve) {
        try {
            BOOST_TEST_MESSAGE("Testing: hf17_linear_reward_curve");

            signed_transaction tx;
            comment_operation comment_op;
            vote_operation vote_op;

            ACTORS_OLD((alice)(bob)(sam))
            generate_block();

            const auto& alice_account = _db.get_account("alice");
            const auto& bob_account = _db.get_account("bob");
            const auto& sam_account = _db.get_account("sam");

            vest("alice", ASSET("30.000 GOLOS"));
            vest("bob", ASSET("20.000 GOLOS"));
            vest("sam", ASSET("10.000 GOLOS"));
            generate_block();
            validate_database();

            const auto& gpo = _db.get_dynamic_global_properties();
            BOOST_CHECK_EQUAL(gpo.total_reward_shares2, 0);

            comment_op.author = "alice";
            comment_op.permlink = "foo";
            comment_op.parent_permlink = "test";
            comment_op.title = "bar";
            comment_op.body = "foo bar";

            tx.operations = {comment_op};
            tx.set_expiration(_db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, _db.get_chain_id());
            _db.push_transaction(tx, 0);
            generate_block();

            const auto& alice_comment = _db.get_comment_by_perm(comment_op.author, comment_op.permlink);

            comment_op.author = "bob";

            tx.operations = {comment_op};
            tx.signatures.clear();
            tx.sign(bob_private_key, _db.get_chain_id());
            _db.push_transaction(tx, 0);
            generate_block();

            const auto& bob_comment = _db.get_comment_by_perm(comment_op.author, comment_op.permlink);

            vote_op.voter = "bob";
            vote_op.author = "alice";
            vote_op.permlink = "foo";
            vote_op.weight = STEEMIT_100_PERCENT;

            tx.operations = {vote_op};
            tx.signatures.clear();
            tx.sign(bob_private_key, _db.get_chain_id());
            _db.push_transaction(tx, 0);

            vote_op.voter = "sam";
            vote_op.weight = STEEMIT_1_PERCENT * 50;

            tx.operations = {vote_op};
            tx.signatures.clear();
            tx.sign(sam_private_key, _db.get_chain_id());
            _db.push_transaction(tx, 0);
            generate_blocks(_db.head_block_time() + fc::seconds(STEEMIT_MIN_VOTE_INTERVAL_SEC), true);

            vote_op.voter = "alice";
            vote_op.author = "bob";
            vote_op.permlink = "foo";
            vote_op.weight = STEEMIT_1_PERCENT * 75;

            tx.operations = {vote_op};
            tx.signatures.clear();
            tx.sign(alice_private_key, _db.get_chain_id());
            _db.push_transaction(tx, 0);

            vote_op.voter = "sam";
            vote_op.weight = STEEMIT_1_PERCENT * 50;

            tx.operations = {vote_op};
            tx.signatures.clear();
            tx.sign(sam_private_key, _db.get_chain_id());
            _db.push_transaction(tx, 0);
            generate_block();

            validate_database();

            const auto vote_denom = STEEMIT_VOTE_REGENERATION_PER_DAY_PRE_HF_22 * STEEMIT_VOTE_REGENERATION_SECONDS / (60 * 60 * 24);

            const auto bob_power = (STEEMIT_100_PERCENT + vote_denom - 1) / vote_denom;
            const auto bob_vshares = bob_account.vesting_shares.amount.value * bob_power / STEEMIT_100_PERCENT;

            const auto sam_power = (STEEMIT_1_PERCENT * 50 + vote_denom - 1) / vote_denom;
            const auto sam_vshares = sam_account.vesting_shares.amount.value * sam_power / STEEMIT_100_PERCENT;

            const auto alice_power = (STEEMIT_1_PERCENT * 75 + vote_denom - 1) / vote_denom;
            const auto alice_vshares = alice_account.vesting_shares.amount.value * alice_power / STEEMIT_100_PERCENT;

            BOOST_CHECK_EQUAL(
                gpo.total_reward_shares2,
                quadratic_curve(sam_vshares + bob_vshares) + quadratic_curve(sam_vshares + alice_vshares));
            BOOST_CHECK_EQUAL(
                gpo.total_reward_shares2,
                quadratic_curve(alice_comment.net_rshares.value) + quadratic_curve(bob_comment.net_rshares.value));

            _db.set_hardfork(STEEMIT_HARDFORK_0_17);

            generate_block();
            validate_database();

            BOOST_CHECK_EQUAL(
                gpo.total_reward_shares2,
                linear_curve(sam_vshares + bob_vshares) + linear_curve(sam_vshares + alice_vshares));
            BOOST_CHECK_EQUAL(
                gpo.total_reward_shares2,
                linear_curve(alice_comment.net_rshares.value) + linear_curve(bob_comment.net_rshares.value));

            comment_op.author = "sam";

            tx.operations = {comment_op};
            tx.signatures.clear();
            tx.sign(sam_private_key, _db.get_chain_id());
            _db.push_transaction(tx, 0);
            generate_block();

            const auto& sam_comment = _db.get_comment_by_perm(comment_op.author, comment_op.permlink);

            vote_op.voter = "alice";
            vote_op.author = "sam";
            vote_op.permlink = "foo";
            vote_op.weight = STEEMIT_1_PERCENT * 75;

            tx.operations = {vote_op};
            tx.signatures.clear();
            tx.sign(alice_private_key, _db.get_chain_id());
            _db.push_transaction(tx, 0);
            generate_block();

            BOOST_CHECK_EQUAL(
                gpo.total_reward_shares2,
                linear_curve(sam_vshares + bob_vshares) +
                linear_curve(sam_vshares + alice_vshares) +
                linear_curve(alice_vshares));
            BOOST_CHECK_EQUAL(
                gpo.total_reward_shares2,
                linear_curve(alice_comment.net_rshares.value) +
                linear_curve(bob_comment.net_rshares.value) +
                linear_curve(sam_comment.net_rshares.value));
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(hf17_update_cashout_window) {
        try {
            BOOST_TEST_MESSAGE("Testing: hf17_update_cashout_window");

            signed_transaction tx;
            comment_operation comment_op;

            ACTORS_OLD((alice)(bob))
            generate_block();

            vest("alice", ASSET("30.000 GOLOS"));
            vest("bob", ASSET("20.000 GOLOS"));
            generate_block();
            validate_database();

            comment_op.author = "alice";
            comment_op.permlink = "foo";
            comment_op.parent_permlink = "test";
            comment_op.title = "bar";
            comment_op.body = "foo bar";

            tx.operations = { comment_op };
            tx.set_expiration(_db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, _db.get_chain_id());
            _db.push_transaction(tx, 0);
            generate_block();

            const auto& alice_comment = _db.get_comment_by_perm(comment_op.author, comment_op.permlink);

            comment_op.author = "bob";

            tx.operations = { comment_op };
            tx.signatures.clear();
            tx.sign(bob_private_key, _db.get_chain_id());
            _db.push_transaction(tx, 0);
            generate_block();

            const auto& bob_comment = _db.get_comment_by_perm(comment_op.author, comment_op.permlink);

            validate_database();

            BOOST_CHECK_EQUAL(alice_comment.cashout_time, alice_comment.created +STEEMIT_CASHOUT_WINDOW_SECONDS_PRE_HF17);
            BOOST_CHECK_EQUAL(bob_comment.cashout_time, bob_comment.created + STEEMIT_CASHOUT_WINDOW_SECONDS_PRE_HF17);

            _db.set_hardfork(STEEMIT_HARDFORK_0_17);
            generate_block();
            validate_database();

            BOOST_CHECK_EQUAL(alice_comment.cashout_time, alice_comment.created +STEEMIT_CASHOUT_WINDOW_SECONDS);
            BOOST_CHECK_EQUAL(bob_comment.cashout_time, bob_comment.created + STEEMIT_CASHOUT_WINDOW_SECONDS);

            generate_blocks(bob_comment.cashout_time);

            BOOST_CHECK_EQUAL(alice_comment.cashout_time, fc::time_point_sec::maximum());
            BOOST_CHECK_EQUAL(bob_comment.cashout_time, fc::time_point_sec::maximum());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(hf17_nested_comment) {
        try {
            BOOST_TEST_MESSAGE("Testing: hf17_nested_comment");

            signed_transaction tx;
            comment_operation comment_op;

            ACTORS_OLD((alice)(bob))
            generate_block();

            vest("alice", ASSET("30.000 GOLOS"));
            vest("bob", ASSET("20.000 GOLOS"));
            generate_block();
            validate_database();

            comment_op.author = "alice";
            comment_op.permlink = "foo";
            comment_op.parent_permlink = "test";
            comment_op.title = "bar";
            comment_op.body = "foo bar";
            tx.operations = { comment_op };
            tx.set_expiration(_db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, _db.get_chain_id());
            _db.push_transaction(tx, 0);
            generate_block();

            comment_op.parent_permlink = comment_op.permlink;
            comment_op.parent_author = comment_op.author;
            comment_op.author = "bob";
            comment_op.permlink = "bob-comment-1";
            tx.operations = { comment_op };
            tx.signatures.clear();
            tx.sign(bob_private_key, _db.get_chain_id());
            _db.push_transaction(tx, 0);
            generate_blocks(_db.head_block_time() + STEEMIT_MIN_REPLY_INTERVAL);

            comment_op.parent_permlink = comment_op.permlink;
            comment_op.parent_author = comment_op.author;
            comment_op.author = "alice";
            comment_op.permlink = "alice-comment-2";
            tx.operations = { comment_op };
            tx.signatures.clear();
            tx.sign(alice_private_key, _db.get_chain_id());
            _db.push_transaction(tx, 0);
            generate_blocks(_db.head_block_time() + STEEMIT_MIN_REPLY_INTERVAL);

            comment_op.parent_permlink = comment_op.permlink;
            comment_op.parent_author = comment_op.author;
            comment_op.author = "bob";
            comment_op.permlink = "bob-comment-3";
            tx.operations = { comment_op };
            tx.signatures.clear();
            tx.sign(bob_private_key, _db.get_chain_id());
            _db.push_transaction(tx, 0);
            generate_blocks(_db.head_block_time() + STEEMIT_MIN_REPLY_INTERVAL);

            comment_op.parent_permlink = comment_op.permlink;
            comment_op.parent_author = comment_op.author;
            comment_op.author = "alice";
            comment_op.permlink = "alice-comment-4";
            tx.operations = { comment_op };
            tx.signatures.clear();
            tx.sign(alice_private_key, _db.get_chain_id());
            _db.push_transaction(tx, 0);
            generate_blocks(_db.head_block_time() + STEEMIT_MIN_REPLY_INTERVAL);

            comment_op.parent_permlink = comment_op.permlink;
            comment_op.parent_author = comment_op.author;
            comment_op.author = "bob";
            comment_op.permlink = "bob-comment-5";
            tx.operations = { comment_op };
            tx.signatures.clear();
            tx.sign(bob_private_key, _db.get_chain_id());
            _db.push_transaction(tx, 0);
            generate_blocks(_db.head_block_time() + STEEMIT_MIN_REPLY_INTERVAL);

            comment_op.parent_permlink = comment_op.permlink;
            comment_op.parent_author = comment_op.author;
            comment_op.author = "alice";
            comment_op.permlink = "alice-comment-6";
            tx.operations = { comment_op };
            tx.signatures.clear();
            tx.sign(alice_private_key, _db.get_chain_id());
            STEEMIT_CHECK_THROW(_db.push_transaction(tx, 0), fc::exception);

            generate_block();
            _db.set_hardfork(STEEMIT_HARDFORK_0_17);
            generate_block();
            validate_database();

            _db.push_transaction(tx, 0);
        }
        FC_LOG_AND_RETHROW()
    }
BOOST_AUTO_TEST_SUITE_END()

#endif