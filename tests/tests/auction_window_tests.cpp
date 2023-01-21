#ifdef STEEMIT_BUILD_TESTNET

#include <boost/test/unit_test.hpp>

#include <golos/chain/database.hpp>

#include "database_fixture.hpp"
#include "comment_reward.hpp"
#include "helpers.hpp"

using namespace golos;
using namespace golos::chain;
using namespace golos::protocol;

struct votes_extended_fixture : public golos::chain::clean_database_fixture {

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
            GOLOS_CHECK_NO_THROW(account_create(name, private_key.get_public_key(), post_key.get_public_key()));
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
        ACTOR(alice);
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

BOOST_FIXTURE_TEST_SUITE(auction_window_tests, votes_extended_fixture)

    BOOST_AUTO_TEST_CASE(auction_window_tokens_to_reward_fund) {
        try {
            BOOST_TEST_MESSAGE("Testing: auction_window_tokens_to_reward_fund");

            BOOST_TEST_MESSAGE("Test changing auction window size");

            db_plugin->debug_update([=](database &db) {
                auto &wso = db.get_witness_schedule_object();
                BOOST_CHECK_EQUAL(wso.median_props.auction_window_size, STEEMIT_REVERSE_AUCTION_WINDOW_SECONDS);

                db.modify(wso, [&](witness_schedule_object &w) {
                    w.median_props.auction_window_size = STEEMIT_REVERSE_AUCTION_WINDOW_SECONDS / 2;
                });

                BOOST_CHECK_EQUAL(wso.median_props.auction_window_size, STEEMIT_REVERSE_AUCTION_WINDOW_SECONDS / 2);
            }, database::skip_witness_signature);

            BOOST_TEST_MESSAGE("Auction window reward goes to reward fund.");

            // Needed to be sured, that auction window's been already enabled.
            generate_blocks(fc::time_point_sec(STEEMIT_HARDFORK_0_6_REVERSE_AUCTION_TIME), true);

            auto &wso = db->get_witness_schedule_object();
            db->modify(wso, [&](witness_schedule_object &w) {
                w.median_props.auction_window_size = 10 * 60;
            });

            generate_block();

            ACTOR(alice);
            int voters_count = 10;

            comment_operation comment;
            comment_options_operation cop;
            signed_transaction tx;
            set_price_feed(price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG")));

            BOOST_TEST_MESSAGE("Create a post.");
            comment.author = "alice";
            comment.permlink = "testauw";
            comment.parent_permlink = "test";
            comment.title = "test";
            comment.body = "Let's test auction window improvements!";

            tx.operations.clear();
            tx.signatures.clear();
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, comment));
            generate_block();

            protocol::comment_auction_window_reward_destination dest;
            dest.destination = protocol::to_reward_fund;

            cop.author = comment.author;
            cop.permlink = comment.permlink;

            cop.extensions.insert(dest);

            tx.operations.clear();
            tx.signatures.clear();

            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
            generate_block();
            auto& alice_post = db->get_comment_by_perm(comment.author, comment.permlink);
            generate_voters(20);

            BOOST_CHECK_EQUAL(alice_post.cashout_time, alice_post.created + STEEMIT_CASHOUT_WINDOW_SECONDS);

            vote_sequence(comment.author, comment.permlink, voters_count / 2, 5);

            const auto& alice_bill = db->get_comment_bill(alice_post.id);
            generate_blocks((alice_post.created + alice_bill.auction_window_size), true);

            vote_sequence(comment.author, comment.permlink, voters_count / 2, 5);

            generate_blocks((alice_post.cashout_time - STEEMIT_BLOCK_INTERVAL), true);

            comment_fund total_comment_fund(*db);
            comment_reward alice_post_reward(*db, total_comment_fund, alice_post);

            generate_block();

            share_type rwd = 0;
            for (int i = 0; i < voters_count; i++) {
                std::string acc_name = "voter" + std::to_string(i);
                auto& account = db->get_account(acc_name);
                rwd += account.curation_rewards;
            }
            auto& alice_acc = db->get_account(alice_post.author);
            BOOST_CHECK_EQUAL(rwd.value, alice_post_reward.total_curators_reward());
            BOOST_CHECK_EQUAL(alice_acc.posting_rewards.value, alice_post_reward.total_author_reward());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(auction_window_tokens_to_reward_fund_empty_case) {
        try {
            BOOST_TEST_MESSAGE("Testing: Auction window is 0.");

            // Needed to be sured, that auction window's been already enabled.
            generate_blocks(fc::time_point_sec(STEEMIT_HARDFORK_0_6_REVERSE_AUCTION_TIME), true);

            auto &wso = db->get_witness_schedule_object();
            db->modify(wso, [&](witness_schedule_object &w) {
                w.median_props.auction_window_size = 10 * 60;
            });

            generate_block();

            ACTOR(alice);
            int voters_count = 10;

            comment_operation comment;
            comment_options_operation cop;
            signed_transaction tx;
            set_price_feed(price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG")));

            BOOST_TEST_MESSAGE("Create a post.");
            comment.author = "alice";
            comment.permlink = "testauw2";
            comment.parent_permlink = "test";
            comment.title = "test";
            comment.body = "Let's test auction window improvements!";

            tx.operations.clear();
            tx.signatures.clear();
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, comment));
            generate_block();

            protocol::comment_auction_window_reward_destination dest;
            dest.destination = protocol::to_reward_fund;

            cop.author = comment.author;
            cop.permlink = comment.permlink;

            cop.extensions.insert(dest);

            tx.operations.clear();
            tx.signatures.clear();

            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
            generate_block();
            auto& alice_post = db->get_comment_by_perm(comment.author, comment.permlink);
            generate_voters(voters_count);

            BOOST_TEST_MESSAGE("Create votes.");
            BOOST_CHECK_EQUAL(alice_post.cashout_time, alice_post.created + STEEMIT_CASHOUT_WINDOW_SECONDS);

            const auto& alice_bill = db->get_comment_bill(alice_post.id);

            generate_blocks((alice_post.created + alice_bill.auction_window_size), true);
            vote_sequence(comment.author, comment.permlink, voters_count, 5);

            generate_blocks((alice_post.cashout_time - STEEMIT_BLOCK_INTERVAL), true);

            comment_fund total_comment_fund(*db);
            comment_reward alice_post_reward(*db, total_comment_fund, alice_post);

            generate_block();

            share_type rwd = 0;
            for (int i = 0; i < voters_count; i++) {
                std::string acc_name = "voter" + std::to_string(i);
                auto& account = db->get_account(acc_name);
                rwd += account.curation_rewards;
            }
            auto& alice_acc = db->get_account(alice_post.author);
            BOOST_CHECK_EQUAL(rwd.value, alice_post_reward.total_curators_reward());
            BOOST_CHECK_EQUAL(alice_acc.posting_rewards.value, alice_post_reward.total_author_reward());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(auction_window_tokens_to_reward_fund_all_in_auw) {
        try {
            BOOST_TEST_MESSAGE("Testing: Auction window reward goes to reward fund. All votes made in auw");

            // Needed to be sured, that auction window's been already enabled.
            generate_blocks(fc::time_point_sec(STEEMIT_HARDFORK_0_6_REVERSE_AUCTION_TIME), true);

            auto &wso = db->get_witness_schedule_object();
            db->modify(wso, [&](witness_schedule_object &w) {
                w.median_props.auction_window_size = 10 * 60;
            });

            generate_block();

            ACTOR(alice);
            int voters_count = 10;

            comment_operation comment;
            comment_options_operation cop;
            signed_transaction tx;
            set_price_feed(price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG")));

            BOOST_TEST_MESSAGE("Create a post.");
            comment.author = "alice";
            comment.permlink = "testauw3";
            comment.parent_permlink = "test";
            comment.title = "test";
            comment.body = "Let's test auction window improvements!";

            tx.operations.clear();
            tx.signatures.clear();
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, comment));
            generate_block();

            protocol::comment_auction_window_reward_destination dest;
            dest.destination = protocol::to_reward_fund;

            cop.author = comment.author;
            cop.permlink = comment.permlink;

            cop.extensions.insert(dest);

            tx.operations.clear();
            tx.signatures.clear();

            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
            generate_block();
            auto& alice_post = db->get_comment_by_perm(comment.author, comment.permlink);
            generate_voters(voters_count);

            BOOST_TEST_MESSAGE("Create votes.");
            BOOST_CHECK_EQUAL(alice_post.cashout_time, alice_post.created + STEEMIT_CASHOUT_WINDOW_SECONDS);

            vote_sequence(comment.author, comment.permlink, voters_count, 5);
            
            const auto& alice_bill = db->get_comment_bill(alice_post.id);
            generate_blocks((alice_post.created + alice_bill.auction_window_size), true);

            generate_blocks((alice_post.cashout_time - STEEMIT_BLOCK_INTERVAL), true);

            comment_fund total_comment_fund(*db);
            comment_reward alice_post_reward(*db, total_comment_fund, alice_post);

            generate_block();

            share_type rwd = 0;
            for (int i = 0; i < voters_count; i++) {
                std::string acc_name = "voter" + std::to_string(i);
                auto& account = db->get_account(acc_name);
                rwd += account.curation_rewards;
            }
            auto& alice_acc = db->get_account(alice_post.author);
            BOOST_CHECK_EQUAL(rwd.value, alice_post_reward.total_curators_reward());
            BOOST_CHECK_EQUAL(alice_acc.posting_rewards.value, alice_post_reward.total_author_reward());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(auction_window_tokens_to_curators_case_empty_auw) {
        try {
            BOOST_TEST_MESSAGE("Testing: Auction window is 0. The to_curators case.");

            // Needed to be sured, that auction window's been already enabled.
            generate_blocks(fc::time_point_sec(STEEMIT_HARDFORK_0_6_REVERSE_AUCTION_TIME), true);

            auto &wso = db->get_witness_schedule_object();
            db->modify(wso, [&](witness_schedule_object &w) {
                w.median_props.auction_window_size = 10 * 60;
            });

            generate_block();

            ACTOR(alice);
            int voters_count = 10;

            comment_operation comment;
            comment_options_operation cop;
            signed_transaction tx;
            set_price_feed(price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG")));

            BOOST_TEST_MESSAGE("Create a post.");
            comment.author = "alice";
            comment.permlink = "testauw4";
            comment.parent_permlink = "test";
            comment.title = "test";
            comment.body = "Let's test auction window improvements!";

            tx.operations.clear();
            tx.signatures.clear();
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, comment));
            generate_block();

            protocol::comment_auction_window_reward_destination dest;
            dest.destination = protocol::to_curators;

            cop.author = comment.author;
            cop.permlink = comment.permlink;

            cop.extensions.insert(dest);

            tx.operations.clear();
            tx.signatures.clear();

            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
            generate_block();
            auto& alice_post = db->get_comment_by_perm(comment.author, comment.permlink);
            generate_voters(voters_count);

            BOOST_TEST_MESSAGE("Create votes.");
            BOOST_CHECK_EQUAL(alice_post.cashout_time, alice_post.created + STEEMIT_CASHOUT_WINDOW_SECONDS);

            const auto& alice_bill = db->get_comment_bill(alice_post.id);

            generate_blocks((alice_post.created + alice_bill.auction_window_size), true);
            vote_sequence(comment.author, comment.permlink, voters_count, 5);

            generate_blocks((alice_post.cashout_time - STEEMIT_BLOCK_INTERVAL), true);

            comment_fund total_comment_fund(*db);
            comment_reward alice_post_reward(*db, total_comment_fund, alice_post);

            generate_block();

            share_type rwd = 0;
            for (int i = 0; i < voters_count; i++) {
                std::string acc_name = "voter" + std::to_string(i);
                auto& account = db->get_account(acc_name);
                rwd += account.curation_rewards;
            }

            auto& alice_acc = db->get_account(alice_post.author);
            BOOST_CHECK_EQUAL(rwd.value, alice_post_reward.total_curators_reward());
            BOOST_CHECK_EQUAL(alice_acc.posting_rewards.value, alice_post_reward.total_author_reward());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(auction_window_tokens_to_curators_case_all_in_auw) {
        try {
            BOOST_TEST_MESSAGE("Testing: Auction window reward goes to reward fund. The to_curators case");

            // Needed to be sured, that auction window's been already enabled.
            generate_blocks(fc::time_point_sec(STEEMIT_HARDFORK_0_6_REVERSE_AUCTION_TIME), true);

            auto &wso = db->get_witness_schedule_object();
            db->modify(wso, [&](witness_schedule_object &w) {
                w.median_props.auction_window_size = 10 * 60;
            });

            generate_block();

            ACTOR(alice);
            int voters_count = 10;

            comment_operation comment;
            comment_options_operation cop;
            signed_transaction tx;
            set_price_feed(price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG")));

            BOOST_TEST_MESSAGE("Create a post.");
            comment.author = "alice";
            comment.permlink = "testauw5";
            comment.parent_permlink = "test";
            comment.title = "test";
            comment.body = "Let's test auction window improvements!";

            tx.operations.clear();
            tx.signatures.clear();
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, comment));
            generate_block();

            protocol::comment_auction_window_reward_destination dest;
            dest.destination = protocol::to_curators;

            cop.author = comment.author;
            cop.permlink = comment.permlink;

            cop.extensions.insert(dest);

            tx.operations.clear();
            tx.signatures.clear();

            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
            generate_block();
            auto& alice_post = db->get_comment_by_perm(comment.author, comment.permlink);
            generate_voters(voters_count);

            BOOST_TEST_MESSAGE("Create votes.");
            BOOST_CHECK_EQUAL(alice_post.cashout_time, alice_post.created + STEEMIT_CASHOUT_WINDOW_SECONDS);

            vote_sequence(comment.author, comment.permlink, voters_count, 5);

            const auto& alice_bill = db->get_comment_bill(alice_post.id);

            generate_blocks((alice_post.created + alice_bill.auction_window_size), true);

            generate_blocks((alice_post.cashout_time - STEEMIT_BLOCK_INTERVAL), true);

            comment_fund total_comment_fund(*db);
            comment_reward alice_post_reward(*db, total_comment_fund, alice_post);

            generate_block();

            share_type rwd = 0;
            for (int i = 0; i < voters_count; i++) {
                std::string acc_name = "voter" + std::to_string(i);
                auto& account = db->get_account(acc_name);
                rwd += account.curation_rewards;
            }
            auto& alice_acc = db->get_account(alice_post.author);
            BOOST_CHECK_EQUAL(rwd.value, alice_post_reward.total_curators_reward());
            BOOST_CHECK_EQUAL(alice_acc.posting_rewards.value, alice_post_reward.total_author_reward());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(auction_window_tokens_to_curators_case) {
        try {
            BOOST_TEST_MESSAGE("Testing: auction_window_tokens_to_curators_case");

            // Needed to be sured, that auction window's been already enabled.
            generate_blocks(fc::time_point_sec(STEEMIT_HARDFORK_0_6_REVERSE_AUCTION_TIME), true);

            auto &wso = db->get_witness_schedule_object();
            db->modify(wso, [&](witness_schedule_object &w) {
                w.median_props.auction_window_size = 10 * 60;
            });

            generate_block();

            ACTOR(alice);
            int voters_count = 6;

            comment_operation comment;
            comment_options_operation cop;
            signed_transaction tx;
            set_price_feed(price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG")));

            BOOST_TEST_MESSAGE("Create a post.");
            comment.author = "alice";
            comment.permlink = "testauw6";
            comment.parent_permlink = "test";
            comment.title = "test";
            comment.body = "Let's test auction window improvements!";

            tx.operations.clear();
            tx.signatures.clear();
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, comment));
            generate_block();

            protocol::comment_auction_window_reward_destination dest;
            dest.destination = protocol::to_curators;

            cop.author = comment.author;
            cop.permlink = comment.permlink;

            cop.extensions.insert(dest);

            tx.operations.clear();
            tx.signatures.clear();

            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
            generate_block();
            auto& alice_post = db->get_comment_by_perm(comment.author, comment.permlink);
            generate_voters(voters_count);

            BOOST_TEST_MESSAGE("Create votes.");
            BOOST_CHECK_EQUAL(alice_post.cashout_time, alice_post.created + STEEMIT_CASHOUT_WINDOW_SECONDS);

            vote_sequence(comment.author, comment.permlink, voters_count / 2, 5);

            const auto& alice_bill = db->get_comment_bill(alice_post.id);

            generate_blocks((alice_post.created + alice_bill.auction_window_size), true);
            vote_sequence(comment.author, comment.permlink, voters_count / 2, 5);

            generate_blocks((alice_post.cashout_time - STEEMIT_BLOCK_INTERVAL), true);

            comment_fund total_comment_fund(*db);
            comment_reward alice_post_reward(*db, total_comment_fund, alice_post);

            generate_block();

            share_type rwd = 0;
            for (int i = 0; i < voters_count; i++) {
                std::string acc_name = "voter" + std::to_string(i);
                auto& account = db->get_account(acc_name);
                rwd += account.curation_rewards;
            }
            auto& alice_acc = db->get_account(alice_post.author);

            BOOST_CHECK_EQUAL(rwd.value, alice_post_reward.total_curators_reward());
            BOOST_CHECK_EQUAL(alice_acc.posting_rewards.value, alice_post_reward.total_author_reward());
        }
        FC_LOG_AND_RETHROW()
    }

BOOST_AUTO_TEST_SUITE_END()

#endif
