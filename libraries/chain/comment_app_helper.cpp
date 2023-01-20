
#include <golos/chain/comment_app_helper.hpp>

namespace golos { namespace chain {

std::set<comment_app> blog_apps = { "golos-blog", "steemit", "golos-io" };

size_t max_length = 16;

comment_app parse_comment_app(const database& _db, const comment_operation& op) {
    comment_app app;
    if (op.json_metadata.size()) {
        try {
            auto obj = fc::json::from_string(op.json_metadata).get_object();
            auto app_val = obj.find("app");
            if (app_val == obj.end()) {
                if (!_db.has_hardfork(STEEMIT_HARDFORK_0_28)) {
                    app = "golos-blog";
                }
            } else {
                auto app_str = app_val->value().as_string();
                std::vector<std::string> parts;
                parts.reserve(2);
                boost::split(parts, app_str, boost::is_any_of("/"));
                if (app_str.size() > max_length) {
                    app = parts[0].substr(0, max_length - 1) + "|";
                } else {
                    app = parts[0];
                }
                if (blog_apps.find(app) != blog_apps.end()) {
                    app = "golos-blog";
                }
            }
            if (app != "golos-desktop") {
                auto tags = obj["tags"].as<std::set<std::string>>();
                if (tags.find("onlyapp") != tags.end()) {
                    app = "golos-desktop";
                }
            }
        } catch (const fc::exception& e) {
        }
    }
    return app;
}

const comment_app_id* find_comment_app_id(const database& _db, const comment_app& app) {
    const auto& idx = _db.get_index<comment_app_index, by_app>();
    auto itr = idx.find(app);
    if (itr != idx.end()) {
        return (const comment_app_id*)&(itr->id._id);
    } else {
        return nullptr;
    }
}

comment_app get_comment_app_by_id(const database& _db, const comment_app_id& id) {
    const auto& idx = _db.get_index<comment_app_index, by_id>();
    auto itr = idx.find(id);
    if (itr != idx.end()) {
        return itr->app;
    } else {
        return comment_app();
    }
}

comment_app_id singleton_comment_app(database& _db, const comment_app& app) {
    const auto* app_id = find_comment_app_id(_db, app);
    if (!app_id) {
        const auto& app_obj = _db.create<comment_app_object>([&](auto& cao) {
            cao.app = app;
        });
        app_id = (const comment_app_id*)&(app_obj.id._id);
    }
    return *app_id;
}

}}
