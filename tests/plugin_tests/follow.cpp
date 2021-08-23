#include <boost/test/unit_test.hpp>

#include <boost/container/flat_set.hpp>

#include "database_fixture.hpp"
#include "helpers.hpp"

#include <golos/chain/account_object.hpp>

#include <golos/plugins/follow/plugin.hpp>
#include <golos/plugins/follow/follow_operations.hpp>

using boost::container::flat_set;

using golos::plugins::json_rpc::msg_pack;

using golos::logic_exception;
using golos::missing_object;
using golos::protocol::comment_operation;
using golos::protocol::vote_operation;
using golos::protocol::tx_invalid_operation;
using golos::protocol::public_key_type;
using golos::protocol::signed_transaction;
using golos::protocol::custom_binary_operation;
using golos::chain::account_id_type;
using golos::chain::make_comment_id;

using golos::chain::account_name_set;

using namespace golos::plugins::follow;


struct follow_fixture : public golos::chain::database_fixture {
    follow_fixture() : golos::chain::database_fixture() {
        initialize<golos::plugins::follow::plugin>();
        open_database();
        startup();
    }
};


BOOST_FIXTURE_TEST_SUITE(follow_plugin, follow_fixture)

BOOST_AUTO_TEST_CASE(follow_validate) {
    BOOST_TEST_MESSAGE("Testing: follow_validate");

    ACTORS((alice)(bob));

    follow_operation op;
    op.follower = "alice";
    op.following = "bob";
    op.what = {"blog"};
    CHECK_OP_VALID(op);

    CHECK_PARAM_VALIDATION_FAIL(op, following, "alice",
        CHECK_ERROR(logic_exception, logic_errors::cannot_follow_yourself));
}

BOOST_AUTO_TEST_CASE(follow_authorities) {
    BOOST_TEST_MESSAGE("Testing: follow_authorities");

    follow_operation op;
    op.follower = "alice";
    op.following = "bob";
    op.what = {"blog"};

    CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"alice"}));
}

BOOST_AUTO_TEST_CASE(follow_apply) {
    BOOST_TEST_MESSAGE("Testing: follow_apply");

    ACTORS((alice)(bob));

    generate_blocks(60 / STEEMIT_BLOCK_INTERVAL);

    custom_binary_operation cop;
    cop.required_posting_auths.insert("alice");
    cop.id = "follow";

    boost::container::vector<follow_plugin_operation> vec;
    signed_transaction tx;

    BOOST_TEST_MESSAGE("--- success execution");
    follow_operation op;
    op.follower = "alice";
    op.following = "bob";
    op.what = {"blog"};

    vec.push_back(op);
    cop.data = fc::raw::pack(vec);

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));

    BOOST_TEST_MESSAGE("--- failed when 'blog' & 'ignore' at the same time");
    op.what = {"blog", "ignore"};

    vec.clear();
    vec.push_back(op);
    cop.data = fc::raw::pack(vec);

    GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, cop),
        CHECK_ERROR(tx_invalid_operation, 0,
            CHECK_ERROR(logic_exception, logic_errors::cannot_follow_and_ignore_simultaneously)));
}

BOOST_AUTO_TEST_CASE(reblog_validate) {
    BOOST_TEST_MESSAGE("Testing: reblog_validate");

    reblog_operation op;
    op.account = "alice";
    op.author = "bob";
    op.permlink = "foo";

    CHECK_OP_VALID(op);

    CHECK_PARAM_VALIDATION_FAIL(op, author, "alice",
        CHECK_ERROR(logic_exception, logic_errors::cannot_reblog_own_content));
}

BOOST_AUTO_TEST_CASE(reblog_authorities) {
    BOOST_TEST_MESSAGE("Testing: reblog_authorities");

    reblog_operation op;
    op.account = "alice";
    op.author = "bob";
    op.permlink = "foo";

    CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"alice"}));
}

BOOST_AUTO_TEST_CASE(reblog_apply) {
    BOOST_TEST_MESSAGE("Testing: reblog_apply");

    ACTORS((alice)(bob));

    generate_blocks(60 / STEEMIT_BLOCK_INTERVAL);
    signed_transaction tx;

    {
        comment_operation op;
        op.author = "bob";
        op.permlink = "lorem";
        op.parent_author = "";
        op.parent_permlink = "ipsum";
        op.title = "Lorem Ipsum";
        op.body = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
        op.json_metadata = "{\"foo\":\"bar\"}";
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));

        op.author = "alice";
        op.permlink = "foo";
        op.parent_author = "bob";
        op.parent_permlink = "lorem";
        op.title = "Lorem Ipsum";
        op.body = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    }

    custom_binary_operation cop;
    cop.required_posting_auths.insert("alice");
    cop.id = "follow";

    boost::container::vector<follow_plugin_operation> vec;

    BOOST_TEST_MESSAGE("--- success execution");
    reblog_operation op;
    op.account = "alice";
    op.author = "bob";
    op.permlink = "lorem";

    vec.clear();
    vec.push_back(op);
    cop.data = fc::raw::pack(vec);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));

    BOOST_TEST_MESSAGE("--- failed when comment is missing");
    op.permlink = "david";

    vec.clear();
    vec.push_back(op);
    cop.data = fc::raw::pack(vec);
    GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, cop),
        CHECK_ERROR(tx_invalid_operation, 0,
            CHECK_ERROR(missing_object, "comment", make_comment_id("bob", "david"))));

    BOOST_TEST_MESSAGE("--- failed when reblog comment");
    op.account = "bob";
    op.author = "alice";
    op.permlink = "foo";

    vec.clear();
    vec.push_back(op);
    cop.data = fc::raw::pack(vec);
    cop.required_posting_auths.clear();
    cop.required_posting_auths.insert("bob");
    GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_private_key, cop),
        CHECK_ERROR(tx_invalid_operation, 0,
            CHECK_ERROR(logic_exception, logic_errors::only_top_level_posts_reblogged)));

}

BOOST_AUTO_TEST_CASE(vote_reputation) {
    try {
        BOOST_TEST_MESSAGE("Testing: vote_reputation");

        ACTORS((alice)(bob)(carol)(dave))
        generate_block();

        signed_transaction tx;
        vote_operation op;
        const auto& idx = db->get_index<reputation_index, by_account>();

        BOOST_TEST_MESSAGE("-- Creating post by alice");

        comment_create("alice", alice_private_key, "test", "", "test");

        BOOST_TEST_MESSAGE("-- Upvoting alice account");

        op.voter = "bob";
        op.author = "alice";
        op.permlink = "";
        op.weight = STEEMIT_100_PERCENT;
        BOOST_TEST_MESSAGE("-- validating that operation");
        BOOST_CHECK_NO_THROW(op.validate());
        BOOST_TEST_MESSAGE("-- upvoting");
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
        generate_block();

        // TODO: check virtual operation

        BOOST_TEST_MESSAGE("-- Checking that alice now has some reputation");

        int64_t rep_for_acc_vote = 0;
        {
            auto itr = idx.find("alice");
            BOOST_CHECK(itr != idx.end());
            BOOST_CHECK_GT(itr->reputation.value, 0);
            rep_for_acc_vote = itr->reputation.value;
        }

        BOOST_TEST_MESSAGE("-- Upvoting alice post");
        op.voter = "carol";
        op.permlink = "test";
        BOOST_TEST_MESSAGE("-- validating that operation");
        BOOST_CHECK_NO_THROW(op.validate());
        BOOST_TEST_MESSAGE("-- upvoting");
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, op));
        generate_block();

        BOOST_TEST_MESSAGE("-- Checking that alicce reputation growed, and this delta is same as in account upvote");

        {
            auto itr = idx.find("alice");
            BOOST_CHECK(itr != idx.end());
            BOOST_CHECK_GT(itr->reputation.value, 0);
            BOOST_CHECK_EQUAL(itr->reputation.value, rep_for_acc_vote * 2);
        }

        BOOST_TEST_MESSAGE("-- Upvoting alice account again (to check modify case) by dave");

        op.voter = "dave";
        op.permlink = "";
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, dave_private_key, op));
        generate_block();

        // TODO: check virtual operation

        BOOST_TEST_MESSAGE("-- Checking that alice reputation growed again");

        {
            auto itr = idx.find("alice");
            BOOST_CHECK(itr != idx.end());
            BOOST_CHECK_GT(itr->reputation.value, 0);
            BOOST_CHECK_EQUAL(itr->reputation.value, rep_for_acc_vote * 3);
        }

        BOOST_TEST_MESSAGE("-- Checking that cannot vote with 0% weight");

        op.weight = 0;
        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, dave_private_key, op),
            CHECK_ERROR(tx_invalid_operation, 0));

        validate_database();
        generate_block();
    }
    FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(downvote_reputation) {
    try {
        BOOST_TEST_MESSAGE("Testing: downvote_reputation");

        ACTORS((alice)(bob)(carol)(dave))
        generate_block();

        signed_transaction tx;
        vote_operation op;
        const auto& idx = db->get_index<reputation_index, by_account>();

        BOOST_TEST_MESSAGE("-- Downvoting alice account (by bob, who have 0 reputation)");

        op.voter = "bob";
        op.author = "alice";
        op.permlink = "";
        op.weight = -STEEMIT_1_PERCENT;
        BOOST_TEST_MESSAGE("-- validating that operation");
        BOOST_CHECK_NO_THROW(op.validate());
        BOOST_TEST_MESSAGE("-- voting");
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
        generate_block();

        // TODO: check virtual operation

        BOOST_TEST_MESSAGE("-- Checking that alice still has 0 reputation");

        {
            auto itr = idx.find("alice");
            BOOST_CHECK(itr == idx.end());
        }

        validate_database();
        generate_block();

        BOOST_TEST_MESSAGE("-- Upvote bob with carol");

        op.voter = "carol";
        op.author = "bob";
        op.permlink = "";
        op.weight = STEEMIT_1_PERCENT;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, op));
        generate_block();

        BOOST_TEST_MESSAGE("-- Try downvote alice by bob again");

        op.voter = "bob";
        op.author = "alice";
        op.permlink = "";
        op.weight = -STEEMIT_1_PERCENT;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
        generate_block();

        BOOST_TEST_MESSAGE("-- Checking that alice now has negative reputation");

        int64_t rep_for_acc_vote = 0;
        {
            auto itr = idx.find("alice");
            BOOST_CHECK(itr != idx.end());
        }
    }
    FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
