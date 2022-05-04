#include <golos/plugins/elastic_search/elastic_search_plugin.hpp>
#include <golos/plugins/elastic_search/elastic_search_state.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <golos/protocol/block.hpp>
#include <golos/chain/operation_notification.hpp>

namespace golos { namespace plugins { namespace elastic_search {

using golos::protocol::signed_block;

class elastic_search_plugin::elastic_search_plugin_impl final {
public:
    elastic_search_plugin_impl(const std::string& url, const std::string& login, const std::string& password,
        uint16_t versions_depth, fc::time_point_sec skip_comments_before)
            : _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()),
            writer(url, login, password, _db, versions_depth, skip_comments_before) {
    }

    ~elastic_search_plugin_impl() {
    }

    void on_operation(const operation_notification& note) {
        if (!_db.is_generating() && !_db.is_producing()) {
            note.op.visit(writer);
        }
    }

    database& _db;
    elastic_search_state_writer writer;
};

elastic_search_plugin::elastic_search_plugin() = default;

elastic_search_plugin::~elastic_search_plugin() = default;

const std::string& elastic_search_plugin::name() {
    static std::string name = "elastic_search";
    return name;
}

void elastic_search_plugin::set_program_options(bpo::options_description& cli, bpo::options_description& cfg) {
    cfg.add_options() (
        "elastic-search-uri", bpo::value<string>()->default_value("http://127.0.0.1:9200"),
        "Elastic Search URI"
    ) (
        "elastic-search-login", bpo::value<string>(),
        "Elastic Search Login"
    ) (
        "elastic-search-password", bpo::value<string>(),
        "Elastic Search Password"
    ) (
        "elastic-search-versions-depth", bpo::value<uint16_t>()->default_value(10),
        "Count of post/comment versions stored"
    ) (
        "elastic-search-skip-comments-before", bpo::value<string>()->default_value("2019-01-01T00:00:00"),
        "Do not track version history (excl. first version) for comments/posts before this date. Saves disk space"
    );
}

void elastic_search_plugin::plugin_initialize(const bpo::variables_map &options) {
    ilog("Initializing elastic search plugin");

    if (options.count("elastic-search-uri")) {
        auto uri_str = options.at("elastic-search-uri").as<std::string>();
        ilog("Connecting Elastic Search to ${u}", ("u", uri_str));

        auto login = options.at("elastic-search-login").as<std::string>();
        auto password = options.at("elastic-search-password").as<std::string>();
        auto versions_depth = options.at("elastic-search-versions-depth").as<uint16_t>();
        auto skip_comments_before = options.at("elastic-search-skip-comments-before").as<std::string>();

        time_point_sec skb;
        try {
            skb = time_point_sec::from_iso_string(skip_comments_before);
        } catch (...) {
            elog("Cannot parse elastic-search-skip-comments-before - use proper format, example: 2019-01-01T00:00:00");
        }

        my = std::make_unique<elastic_search_plugin::elastic_search_plugin_impl>(uri_str, login, password, versions_depth, skb);

        my->_db.post_apply_operation.connect([&](const operation_notification& note) {
            my->on_operation(note);
        });
    } else {
        ilog("Elastic search plugin configured, but no elastic-search-uri specified. Plugin disabled.");
    }
} 

void elastic_search_plugin::plugin_startup() {
    ilog("Starting up elastic search plugin");
}

void elastic_search_plugin::plugin_shutdown() {
    ilog("Shutting down elastic search plugin");
}

} } } // golos::plugins::elastic_search
