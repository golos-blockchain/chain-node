#include <boost/test/unit_test.hpp>
#include <boost/container/flat_set.hpp>

#include "database_fixture.hpp"
#include "helpers.hpp"

#include <golos/chain/comment_object.hpp>
#include <golos/chain/account_object.hpp>
#include <golos/plugins/chain/plugin.hpp>

using golos::protocol::comment_operation;
using golos::protocol::vote_operation;
using golos::protocol::public_key_type;
using golos::protocol::signed_transaction;
using golos::chain::account_id_type;
using golos::chain::account_name_set;

using namespace golos::plugins::chain;


struct chain_fixture : public golos::chain::database_fixture {

    void initialize(const plugin_options& opts = {}) {
        database_fixture::initialize(opts);
        open_database();
        startup();
    }

    fc::ecc::private_key vote_key;
    uint32_t current_voter = 0;
    static const uint32_t cashout_blocks = STEEMIT_CASHOUT_WINDOW_SECONDS / STEEMIT_BLOCK_INTERVAL;

    void generate_voters(uint32_t n) {
        fc::ecc::private_key private_key = generate_private_key("test");
        fc::ecc::private_key post_key = generate_private_key("test_post");
        vote_key = post_key;
        for (uint32_t i = 0; i < n; i++) {
            const auto name = "voter" + std::to_string(i);
            GOLOS_CHECK_NO_THROW(account_create(name, private_key.get_public_key(), private_key.get_public_key(), post_key.get_public_key(), private_key.get_public_key()));
        }
        generate_block();
        validate_database();
    }

    void vote_sequence(const std::string& author, const std::string& permlink, uint32_t n_votes, uint32_t interval = 0) {
        uint32_t end = current_voter + n_votes;
        for (; current_voter < end; current_voter++) {
            const auto name = "voter" + std::to_string(current_voter);
            vote_operation op;
            op.voter = name;
            op.author = author;
            op.permlink = permlink;
            op.weight = STEEMIT_100_PERCENT;
            signed_transaction tx;
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, vote_key, op));
            if (interval > 0) {
                generate_blocks(interval);
            }
        }
        validate_database();
    }

    void post(const std::string& permlink = "post", const std::string& parent_permlink = "test") {
        ACTOR_OLD(alice);
        comment_operation op;
        op.author = "alice";
        op.permlink = permlink;
        op.parent_author = parent_permlink == "test" ? "" : "alice";
        op.parent_permlink = parent_permlink;
        op.title = "foo";
        op.body = "bar";
        signed_transaction tx;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
        validate_database();
    }

    uint32_t count_stored_votes() {
        const auto n = db->get_index<golos::chain::comment_vote_index>().indices().size();
        return n;
    }
};


BOOST_FIXTURE_TEST_SUITE(chain_plugin, chain_fixture)

BOOST_AUTO_TEST_SUITE(clear_votes)

BOOST_AUTO_TEST_CASE(clear_votes_default) {
    BOOST_TEST_MESSAGE("Testing: clear_votes_default");
    initialize();
    generate_voters(10);
    post();
    BOOST_TEST_MESSAGE("--- vote 9 times and check votes stored");
    vote_sequence("alice", "post", 5);
    auto interval = cashout_blocks / 5;
    vote_sequence("alice", "post", 4, interval);
    BOOST_CHECK_EQUAL(9, count_stored_votes());

    BOOST_TEST_MESSAGE("--- go to 1 block before cashout and check 9 votes stored");
    generate_blocks(interval - 1);
    const auto& post = db->get_comment_by_perm("alice", std::string("post"));
    BOOST_CHECK(post.mode != golos::chain::archived);
    BOOST_CHECK_EQUAL(9, count_stored_votes());

    BOOST_TEST_MESSAGE("--- go to cashout block and check 9 votes stored");
    generate_blocks(1);
    {
        const auto& post = db->get_comment_by_perm("alice", std::string("post"));
        BOOST_CHECK_EQUAL(post.mode, golos::chain::archived);
    }
    BOOST_CHECK_EQUAL(9, count_stored_votes());

    BOOST_TEST_MESSAGE("--- go to 2 * cashout time forward and check 9 votes stored");
    generate_blocks(cashout_blocks * 2);
    BOOST_CHECK_EQUAL(9, count_stored_votes());
}

BOOST_AUTO_TEST_CASE(clear_votes_before_block) {
    BOOST_TEST_MESSAGE("Testing: clear_votes_before_block");
    int before_block = 4 + cashout_blocks * 2;
    initialize({
        {"clear-votes-before-block", std::to_string(before_block)},
    });

    generate_voters(15);
    post();
    BOOST_TEST_MESSAGE("--- vote 9 times and check votes stored");
    vote_sequence("alice", "post", 5);
    auto interval = cashout_blocks / 5;
    vote_sequence("alice", "post", 4, interval);
    BOOST_CHECK_EQUAL(9, count_stored_votes());

    BOOST_TEST_MESSAGE("--- go to 1 block before cashout and check 9 votes stored");
    generate_blocks(interval - 1);
    const auto& post = db->get_comment_by_perm("alice", std::string("post"));
    BOOST_CHECK(post.mode != golos::chain::archived);
    BOOST_CHECK_EQUAL(9, count_stored_votes());

    BOOST_TEST_MESSAGE("--- go to cashout block and check votes removed");
    generate_blocks(1);
    {
        const auto& post = db->get_comment_by_perm("alice", std::string("post"));
        BOOST_CHECK_EQUAL(post.mode, golos::chain::archived);
    }
    BOOST_CHECK_EQUAL(0, count_stored_votes());

    BOOST_TEST_MESSAGE("--- go to just before 'clear-votes-before-block', vote and check 1 vote stored");
    generate_blocks(cashout_blocks - 1);
    vote_sequence("alice", "post", 1);
    BOOST_CHECK_EQUAL(1, count_stored_votes());

    BOOST_TEST_MESSAGE("--- check, vote removed in the next block");
    generate_blocks(1);
    BOOST_CHECK_EQUAL(0, count_stored_votes());

    BOOST_TEST_MESSAGE("--- vote 5 times and check 5 votes stored");
    vote_sequence("alice", "post", 5, 10);
    BOOST_CHECK_EQUAL(5, count_stored_votes());

    BOOST_TEST_MESSAGE("--- go to future and check 5 votes still stored");
    generate_blocks(cashout_blocks);
    BOOST_CHECK_EQUAL(5, count_stored_votes());
}

BOOST_AUTO_TEST_CASE(clear_votes_before_cashout_block) {
    BOOST_TEST_MESSAGE("Testing: clear_votes_before_cashout_block");
    int before_block = cashout_blocks / 2;
    initialize({
        {"clear-votes-before-block", std::to_string(before_block)},
    });

    generate_voters(10);
    post();
    BOOST_TEST_MESSAGE("--- vote 9 times and check votes stored");
    vote_sequence("alice", "post", 5);
    auto interval = cashout_blocks / 5;
    vote_sequence("alice", "post", 4, interval);
    BOOST_CHECK_EQUAL(9, count_stored_votes());

    BOOST_TEST_MESSAGE("--- go to 1 block before cashout and check 9 votes stored");
    generate_blocks(interval - 1);
    const auto& post = db->get_comment_by_perm("alice", std::string("post"));
    BOOST_CHECK(post.mode != golos::chain::archived);
    BOOST_CHECK_EQUAL(9, count_stored_votes());

    BOOST_TEST_MESSAGE("--- go to cashout block and check 9 votes still stored");
    generate_blocks(1);
    {
        const auto& post = db->get_comment_by_perm("alice", std::string("post"));
        BOOST_CHECK_EQUAL(post.mode, golos::chain::archived);
    }
    BOOST_CHECK_EQUAL(9, count_stored_votes());
}

BOOST_AUTO_TEST_CASE(clear_votes_older_n_larger_cashout) {
    BOOST_TEST_MESSAGE("Testing: clear_votes_older_n_larger_cashout");
    int n_blocks = cashout_blocks * 2;
    initialize({
        {"clear-votes-older-n-blocks", std::to_string(n_blocks)},
    });

    generate_voters(15);
    post();
    BOOST_TEST_MESSAGE("--- vote 9 times and check votes stored");
    vote_sequence("alice", "post", 5);
    auto interval = cashout_blocks / 5;
    vote_sequence("alice", "post", 4, interval);
    BOOST_CHECK_EQUAL(9, count_stored_votes());

    BOOST_TEST_MESSAGE("--- go to 1 block before cashout and check 9 votes stored");
    generate_blocks(interval - 1);
    const auto& post = db->get_comment_by_perm("alice", std::string("post"));
    BOOST_CHECK(post.mode != golos::chain::archived);
    BOOST_CHECK_EQUAL(9, count_stored_votes());

    BOOST_TEST_MESSAGE("--- go to cashout block and check 9 votes still stored");
    generate_blocks(1);
    BOOST_CHECK_EQUAL(9, count_stored_votes());
    {
        const auto& post = db->get_comment_by_perm("alice", std::string("post"));
        BOOST_CHECK_EQUAL(post.mode, golos::chain::archived);
    }

    BOOST_TEST_MESSAGE("--- go to block just before 'clear_votes_older_n', vote and check 10 votes stored");
    generate_blocks(cashout_blocks);
    vote_sequence("alice", "post", 1);
    BOOST_CHECK_EQUAL(10, count_stored_votes());

    // 6 = 5 instant created and 1 from first interval
    BOOST_TEST_MESSAGE("--- go to block just after 'clear_votes_older_n' and check 6 votes removed");
    generate_blocks(1);
    BOOST_CHECK_EQUAL(4, count_stored_votes());

    BOOST_TEST_MESSAGE("--- vote 5 times and check 9 votes stored");
    vote_sequence("alice", "post", 4, 10);
    vote_sequence("alice", "post", 1);
    BOOST_CHECK_EQUAL(9, count_stored_votes());

    BOOST_TEST_MESSAGE("--- go to 'clear_votes_older_n' blocks forward and check 1 vote stored");
    generate_blocks(n_blocks);
    BOOST_CHECK_EQUAL(1, count_stored_votes());

    BOOST_TEST_MESSAGE("--- go to 1 block forward and check all votes removed");
    generate_blocks(1);
    BOOST_CHECK_EQUAL(0, count_stored_votes());
}

BOOST_AUTO_TEST_CASE(clear_votes_older_n_smaller_cashout) {
    BOOST_TEST_MESSAGE("Testing: clear_votes_older_n_smaller_cashout");
    int n_blocks = cashout_blocks / 4;
    initialize({
        {"clear-votes-older-n-blocks", std::to_string(n_blocks)},
    });

    generate_voters(15);
    post();
    BOOST_TEST_MESSAGE("--- vote 10 times and check votes stored");
    vote_sequence("alice", "post", 5);
    auto interval = cashout_blocks / 5;
    vote_sequence("alice", "post", 4, interval);
    vote_sequence("alice", "post", 1);
    BOOST_CHECK_EQUAL(10, count_stored_votes());

    BOOST_TEST_MESSAGE("--- go to 1 block before cashout and check 10 votes stored");
    generate_blocks(interval - 1);
    const auto& post = db->get_comment_by_perm("alice", std::string("post"));
    BOOST_CHECK(post.mode != golos::chain::archived);
    BOOST_CHECK_EQUAL(10, count_stored_votes());

    BOOST_TEST_MESSAGE("--- go to cashout block and check 8 votes removed");
    generate_blocks(1);
    BOOST_CHECK_EQUAL(1, count_stored_votes());
    {
        const auto& post = db->get_comment_by_perm("alice", std::string("post"));
        BOOST_CHECK_EQUAL(post.mode, golos::chain::archived);
    }

    BOOST_TEST_MESSAGE("--- go forward just before last vote should be removed and check it stored");
    generate_blocks(n_blocks - interval);
    BOOST_CHECK_EQUAL(1, count_stored_votes());

    BOOST_TEST_MESSAGE("--- go 1 block forward and check all votes removed");
    generate_blocks(1);
    BOOST_CHECK_EQUAL(0, count_stored_votes());
}

BOOST_AUTO_TEST_SUITE_END() // clear_votes

BOOST_AUTO_TEST_SUITE_END()
