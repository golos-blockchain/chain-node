#include <boost/test/unit_test.hpp>

#include "database_fixture.hpp"
#include "helpers.hpp"

#include <golos/chain/account_object.hpp>
#include <golos/plugins/follow/plugin.hpp>
#include <golos/plugins/follow/follow_operations.hpp>
#include <golos/plugins/event_plugin/event_plugin.hpp>

#include <string>
#include <cstdint>

using golos::chain::database_fixture;

using namespace golos::protocol;
using namespace golos::chain;
using namespace golos::plugins::follow;
using golos::plugins::json_rpc::msg_pack;

struct event_plugin_fixture : public database_fixture {
    using follow_plugin = golos::plugins::follow::plugin;
    using event_plugin = golos::plugins::event_plugin::event_plugin;

    template<typename... Plugins>
    void initialize(const database_fixture::plugin_options& opts = {}) {
        database_fixture::initialize<follow_plugin, event_plugin>(opts);
        e_plugin = find_plugin<event_plugin>();
        open_database();
        startup();
    }

    event_plugin* e_plugin = nullptr;
};

BOOST_FIXTURE_TEST_SUITE(event_plugin, event_plugin_fixture)

BOOST_AUTO_TEST_CASE(comment_feed_test) {
    BOOST_TEST_MESSAGE("Testing: comment_feed_test");

    initialize({
        {"event-blocks", "100"},
        {"store-evaluator-events", "true"}
    });

    ACTORS((alice)(bob));

    signed_transaction tx;

    BOOST_TEST_MESSAGE("--- Follow bob to alice");

    follow_operation fop;
    fop.follower = "bob";
    fop.following = "alice";
    fop.what = {"blog"};

    {
        follow_plugin_operation fpop = fop;

        custom_json_operation jop;
        jop.id = "follow";
        jop.json = fc::json::to_string(fpop);
        jop.required_posting_auths = {"bob"};

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, jop));
    }

    BOOST_TEST_MESSAGE("--- Publishing post by alice");

    comment_operation cop;
    cop.author = "alice";
    cop.permlink = "lorem";
    cop.parent_author = "";
    cop.parent_permlink = "ipsum";
    cop.title = "Lorem Ipsum";
    cop.body = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
    cop.json_metadata = "{\"foo\":\"bar\"}";
    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));

    BOOST_TEST_MESSAGE("--- Generating block to trigger event");

    generate_block();

    BOOST_TEST_MESSAGE("--- Getting events");

    msg_pack mp;
    mp.args = std::vector<fc::variant>({fc::variant(db->head_block_num()), fc::variant(false)});
    auto events = e_plugin->get_events_in_block(mp);
    auto str = fc::json::to_string(events);

    BOOST_TEST_MESSAGE("--- Checking there is a feed notification");

    BOOST_CHECK(str.find("\"comment_feed\"") != std::string::npos);

    BOOST_TEST_MESSAGE("--- Checking it is the same block with comment");

    BOOST_CHECK(str.find("\"comment\"") != std::string::npos);

    BOOST_TEST_MESSAGE("--- Generating block");

    generate_block();

    BOOST_TEST_MESSAGE("--- Checking events are cleared now");

    mp.args = std::vector<fc::variant>({fc::variant(db->head_block_num()), fc::variant(false)});
    events = e_plugin->get_events_in_block(mp);
    str = fc::json::to_string(events);

    BOOST_CHECK(str.find("\"comment_feed\"") == std::string::npos);
    BOOST_CHECK(str.find("\"comment\"") == std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
