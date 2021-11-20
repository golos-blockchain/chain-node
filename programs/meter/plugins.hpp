#pragma once

#include <golos/chain/database.hpp>

#include <golos/plugins/account_by_key/account_by_key_objects.hpp>
#include <golos/plugins/follow/follow_objects.hpp>
#include <golos/plugins/private_message/private_message_operations.hpp>
#include <golos/plugins/private_message/private_message_objects.hpp>
#include <golos/plugins/social_network/social_network_types.hpp>
#include <golos/plugins/tags/tags_object.hpp>
#include <golos/plugins/worker_api/worker_api_plugin.hpp>
#include <golos/plugins/account_notes/account_notes_objects.hpp>
#include <golos/plugins/account_history/history_object.hpp>
#include <golos/plugins/operation_history/history_object.hpp>
#include <golos/plugins/market_history/market_history_objects.hpp>
#include <golos/plugins/event_plugin/event_api_objects.hpp>

using namespace golos::chain;

void add_plugins(database& _db) {
    using namespace golos::plugins::account_by_key;
    add_plugin_index<key_lookup_index>(_db);

    using namespace golos::plugins::account_notes;
    add_plugin_index<account_note_index>(_db);
    add_plugin_index<account_note_stats_index>(_db);

    using namespace golos::plugins::follow;
    add_plugin_index<follow_index>(_db);
    add_plugin_index<feed_index>(_db);
    add_plugin_index<blog_index>(_db);
    add_plugin_index<follow_count_index>(_db);
    add_plugin_index<blog_author_stats_index>(_db);

    using namespace golos::plugins::social_network;
    add_plugin_index<comment_content_index>(_db);
    add_plugin_index<comment_last_update_index>(_db);
    add_plugin_index<comment_reward_index>(_db);
    add_plugin_index<donate_data_index>(_db);

    using namespace golos::plugins::private_message;
    add_plugin_index<message_index>(_db);
    add_plugin_index<settings_index>(_db);
    add_plugin_index<contact_index>(_db);
    add_plugin_index<contact_size_index>(_db);

    using namespace golos::plugins;
    add_plugin_index<tags::tag_index>(_db);
    add_plugin_index<tags::tag_stats_index>(_db);
    add_plugin_index<tags::author_tag_stats_index>(_db);
    add_plugin_index<tags::language_index>(_db);

    using namespace golos::plugins::worker_api;
    add_plugin_index<worker_request_metadata_index>(_db);

    using namespace golos::plugins::account_history;
    add_plugin_index<account_history_index>(_db);

    using namespace golos::plugins::operation_history;
    add_plugin_index<operation_index>(_db);

    using namespace golos::plugins::market_history;
    add_plugin_index<bucket_index>(_db);
    add_plugin_index<order_history_index>(_db);

    using namespace golos::plugins::event_plugin;
    add_plugin_index<op_note_index>(_db);
}
