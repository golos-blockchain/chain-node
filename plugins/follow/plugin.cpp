#include <golos/plugins/follow/follow_objects.hpp>
#include <golos/plugins/follow/follow_operations.hpp>
#include <golos/plugins/follow/follow_evaluators.hpp>
#include <golos/protocol/config.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/generic_custom_operation_interpreter.hpp>
#include <golos/chain/operation_notification.hpp>
#include <golos/chain/account_object.hpp>
#include <golos/chain/comment_object.hpp>
#include <memory>
#include <golos/plugins/json_rpc/plugin.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/chain/index.hpp>
#include <golos/api/discussion_helper.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace golos {

template<>
std::string get_logic_error_namespace<golos::plugins::follow::logic_errors::error_type>() {
    return golos::plugins::follow::plugin::name();
}

} // namespace golos

namespace golos {
    namespace plugins {
        namespace follow {
            using namespace golos::protocol;
            using golos::chain::generic_custom_operation_interpreter;
            using golos::chain::custom_operation_interpreter;
            using golos::chain::operation_notification;
            using golos::chain::to_string;
            using golos::chain::account_index;
            using golos::chain::by_name;
            using golos::api::discussion_helper;

            struct pre_operation_visitor {
                plugin& _plugin;
                golos::chain::database& _db;

                pre_operation_visitor(plugin& plugin, golos::chain::database& db) : _plugin(plugin), _db(db) {
                }

                typedef void result_type;

                template<typename T>
                void operator()(const T&) const {
                }

                void operator()(const delete_comment_operation& op) const {
                    try {
                        const auto* comment = _db.find_comment_by_perm(op.author, op.permlink);

                        if (comment == nullptr) {
                            return;
                        }
                        if (comment->parent_author.size()) {
                            return;
                        }

                        const auto& feed_idx = _db.get_index<feed_index, by_comment>();
                        auto itr = feed_idx.lower_bound(comment->id);

                        while (itr != feed_idx.end() && itr->comment == comment->id) {
                            const auto& old_feed = *itr;
                            ++itr;
                            _db.remove(old_feed);
                        }

                        const auto& blog_idx = _db.get_index<blog_index, by_comment>();
                        auto blog_itr = blog_idx.lower_bound(comment->id);

                        while (blog_itr != blog_idx.end() && blog_itr->comment == comment->id) {
                            const auto& old_blog = *blog_itr;
                            ++blog_itr;
                            _db.remove(old_blog);
                        }
                    } FC_CAPTURE_AND_RETHROW()
                }
            };

            struct post_operation_visitor {
                plugin& _plugin;
                database& _db;

                post_operation_visitor(plugin& plugin, database& db) : _plugin(plugin), _db(db) {
                }

                typedef void result_type;

                template<typename T>
                void operator()(const T&) const {
                }

                void operator()(const custom_json_operation& op) const {
                    try {
                        if (op.id == plugin::plugin_name) {
                            custom_json_operation new_cop;

                            new_cop.required_auths = op.required_auths;
                            new_cop.required_posting_auths = op.required_posting_auths;
                            new_cop.id = plugin::plugin_name;
                            follow_operation fop;

                            try {
                                fop = fc::json::from_string(op.json).as<follow_operation>();
                            } catch (const fc::exception&) {
                                return;
                            }

                            auto new_fop = follow_plugin_operation(fop);
                            new_cop.json = fc::json::to_string(new_fop);
                            std::shared_ptr<custom_operation_interpreter> eval = _db.get_custom_json_evaluator(op.id);
                            eval->apply(new_cop);
                        }
                    } FC_CAPTURE_AND_RETHROW()
                }

                void init_parent_hash(hashlink_type& parent_hashlink, const comment_operation& op) const {
                    if (!parent_hashlink) parent_hashlink = _db.make_hashlink(op.parent_permlink);
                }

                int is_blocking(const account_name_type& account, const account_name_type& blocking) const {
                    if (_db.has_hardfork(STEEMIT_HARDFORK_0_27__185)) {
                        return _db.is_blocking(account, blocking);
                    } else {
                        const auto& idx = _db.get_index<follow_index, by_following_follower>();
                        auto itr = idx.find(std::make_tuple(blocking, account));
                        if (itr == idx.end() || !(itr->what & (1 << ignore))) {
                            return 0;
                        } else {
                            return 1;
                        }
                    }
                }

                void process_mentions(const comment_operation& op, hashlink_type hashlink, hashlink_type& parent_hashlink) const {
                    auto max_mentions = _plugin.max_mentions_count();

                    auto body = op.body;
                    std::smatch match;
                    std::set<std::string> proceed;
                    for (uint16_t mentions = 0;
                        std::regex_search(body, match, _plugin.mention_regex())
                            && mentions < max_mentions;
                        ++mentions) {
                        auto mentioned = match.str();
                        if (mentioned.size() >= 4) {
                            mentioned = mentioned.substr(1); // remove @

                            if (mentioned != op.author && mentioned != op.parent_author
                                    && proceed.insert(mentioned).second
                                    && _db.find_account(mentioned)) {
                                if (!is_blocking(mentioned, op.author)) {
                                    init_parent_hash(parent_hashlink, op);
                                    _db.push_event(comment_mention_operation(mentioned,
                                            op.author, hashlink, op.parent_author, parent_hashlink));
                                }
                            }
                        }
                        body = match.suffix();
                    }
                }

                void operator()(const comment_operation& op) const {
                    try {
                        auto hashlink = _db.make_hashlink(op.permlink);

                        const auto& c = _db.get_comment(op.author, hashlink);

                        if (c.created != _db.head_block_time()) {
                            return;
                        }

                        hashlink_type parent_hashlink = 0;

                        process_mentions(op, hashlink, parent_hashlink);

                        const auto& idx = _db.get_index<follow_index, by_following_follower>();

                        if (op.parent_author.size() && op.parent_author != op.author) {
                            if (!is_blocking(op.parent_author, op.author)) {
                                init_parent_hash(parent_hashlink, op);
                                _db.push_event(comment_reply_operation(
                                            op.author, hashlink, op.parent_author, parent_hashlink));
                            }
                        }

                        if (op.parent_author.size() > 0) {
                            return;
                        }

                        const auto& comment_idx = _db.get_index<feed_index, by_comment>();
                        const auto& feed_idx = _db.get_index<feed_index, by_feed>();

                        auto itr = idx.find(op.author);
                        for (; itr != idx.end() && itr->following == op.author; ++itr) {
                            if (itr->what & (1 << blog)) {
                                auto* foll = _db.find_account(itr->follower);
                                if (!foll || foll->frozen) {
                                    continue;
                                }

                                uint32_t next_id = 0;
                                auto last_feed = feed_idx.lower_bound(itr->follower);
                                if (last_feed != feed_idx.end() && last_feed->account == itr->follower) {
                                    next_id = last_feed->account_feed_id + 1;
                                }

                                if (comment_idx.find(std::make_tuple(c.id, itr->follower)) == comment_idx.end()) {
                                    _db.create<feed_object>([&](auto& f) {
                                        f.account = itr->follower;
                                        f.comment = c.id;
                                        f.account_feed_id = next_id;
                                    });

                                    init_parent_hash(parent_hashlink, op);
                                    _db.push_event(comment_feed_operation(itr->follower,
                                        op.author, hashlink, op.parent_author, parent_hashlink));

                                    const auto& old_feed_idx = _db.get_index<feed_index, by_old_feed>();
                                    auto old_feed = old_feed_idx.lower_bound(itr->follower);

                                    while (old_feed != old_feed_idx.end() && old_feed->account == itr->follower &&
                                           next_id - old_feed->account_feed_id > _plugin.max_feed_size()) {
                                        _db.remove(*old_feed);
                                        old_feed = old_feed_idx.lower_bound(itr->follower);
                                    }
                                }
                            }
                        }

                        const auto& blog_idx = _db.get_index<blog_index, by_blog>();
                        auto last_blog = blog_idx.lower_bound(op.author);
                        uint32_t next_id = 0;
                        if (last_blog != blog_idx.end() && last_blog->account == op.author) {
                            next_id = last_blog->blog_feed_id + 1;
                        }

                        const auto& comment_blog_idx = _db.get_index<blog_index, by_comment>();
                        if (comment_blog_idx.find(std::make_tuple(c.id, op.author)) == comment_blog_idx.end()) {
                            _db.create<blog_object>([&](auto& b) {
                                b.account = op.author;
                                b.comment = c.id;
                                b.blog_feed_id = next_id;
                            });
                        }
                    } FC_LOG_AND_RETHROW()
                }

                struct account_setting_visitor {
                    account_setting_visitor(const account_setup_operation& _op, database& db) : op(_op), _db(db) {
                    }

                    const account_setup_operation& op;
                    database& _db;

                    using result_type = void;

                    template<typename T>
                    void operator()(const T&) const {
                    }

                    bool unfollow(account_name_type follower, account_name_type following) const {
                        const auto& idx = _db.get_index<follow_index, by_follower_following>();
                        auto itr = idx.find(std::make_tuple(follower, following));
                        if (itr != idx.end() && (itr->what & (1 << blog))) {
                            _db.remove(*itr);
                            return true;
                        }
                        return false;
                    }

                    void operator()(const account_block_setting& abs) const {
                        if (!abs.block) {
                            return;
                        }
                        bool blocking_was = unfollow(abs.account, op.account);
                        bool blocker_was = unfollow(op.account, abs.account);
                        const auto* blocking = _db.find<follow_count_object, by_account>(abs.account);
                        if (blocking) {
                            _db.modify(*blocking, [&](auto& f) {
                                if (blocking_was) f.following_count--;
                                if (blocker_was) f.follower_count--;
                            });
                        }
                        const auto* blocker = _db.find<follow_count_object, by_account>(op.account);
                        if (blocker) {
                            _db.modify(*blocker, [&](auto& f) {
                                if (blocking_was) f.follower_count--;
                                if (blocker_was) f.following_count--;
                            });
                        }
                    }
                };

                void operator()(const account_setup_operation& op) const {
                    try {
                        account_setting_visitor asv(op, _db);
                        for (auto& s : op.settings) {
                            s.visit(asv);
                        }
                    } FC_LOG_AND_RETHROW()
                }
            };

            inline void set_what(std::vector<follow_type>& what, uint16_t bitmask) {
                if (bitmask & 1 << blog) {
                    what.push_back(blog);
                }
                if (bitmask & 1 << ignore) {
                    what.push_back(ignore);
                }
            }

            struct plugin::impl final {
            public:
                impl() : database_(appbase::app().get_plugin<chain::plugin>().db()) {
                    helper = std::make_unique<discussion_helper>(
                        database_,
                        nullptr,
                        golos::plugins::social_network::fill_comment_info
                    );
                }

                ~impl() {
                };

                void plugin_initialize(plugin& self) {
                    // Each plugin needs its own evaluator registry.
                    _custom_operation_interpreter = std::make_shared<
                            generic_custom_operation_interpreter<follow_plugin_operation>>(database());

                    // Add each operation evaluator to the registry
                    _custom_operation_interpreter->register_evaluator<follow_evaluator>(&self);
                    _custom_operation_interpreter->register_evaluator<reblog_evaluator>(&self);
                    _custom_operation_interpreter->register_evaluator<delete_reblog_evaluator>(&self);

                    // Add the registry to the database so the database can delegate custom ops to the plugin
                    database().set_custom_operation_interpreter(plugin_name, _custom_operation_interpreter);
                }

                golos::chain::database& database() {
                    return database_;
                }


                void pre_operation(const operation_notification& op_obj, plugin& self) {
                    try {
                        op_obj.op.visit(pre_operation_visitor(self, database()));
                    } catch (const fc::assert_exception&) {
                        if (database().is_producing()) {
                            throw;
                        }
                    }
                }

                void post_operation(const operation_notification& op_obj, plugin& self) {
                    try {
                        op_obj.op.visit(post_operation_visitor(self, database()));
                    } catch (fc::assert_exception) {
                        if (database().is_producing()) {
                            throw;
                        }
                    }
                }

                std::vector<follow_api_object> get_followers(
                        account_name_type account,
                        account_name_type start,
                        follow_type type,
                        uint32_t limit = 1000);

                std::vector<follow_api_object> get_following(
                        account_name_type account,
                        account_name_type start,
                        follow_type type,
                        uint32_t limit = 1000);

                /**
                * Checks if post category matches at least one of masks.
                * If you have only comment_object, pass parent_permlink as category (so it is not for non-root comments).
                */
                bool category_matches_masks(
                        const std::string& category,
                        const std::set<std::string>* masks);

                std::vector<feed_entry> get_feed_entries(
                        account_name_type account,
                        uint32_t start_entry_id = 0,
                        uint32_t limit = 500,
                        const std::set<std::string>* filter_tag_masks = nullptr);

                std::vector<comment_feed_entry> get_feed(
                        account_name_type account,
                        uint32_t start_entry_id = 0,
                        uint32_t limit = 500,
                        const std::set<std::string>* filter_tag_masks = nullptr);

                std::vector<blog_entry> get_blog_entries(
                        account_name_type account,
                        uint32_t start_entry_id = 0,
                        uint32_t limit = 500,
                        const std::set<std::string>* filter_tag_masks = nullptr);

                std::vector<comment_blog_entry> get_blog(
                        account_name_type account,
                        uint32_t start_entry_id = 0,
                        uint32_t limit = 500,
                        const std::set<std::string>* filter_tag_masks = nullptr);

                std::vector<account_reputation> get_account_reputations(
                        std::vector<account_name_type> accounts);

                follow_count_api_obj get_follow_count(account_name_type start);

                std::vector<account_name_type> get_reblogged_by(
                        account_name_type author,
                        std::string permlink);

                blog_authors_r get_blog_authors(account_name_type );

                golos::chain::database& database_;

                uint32_t max_feed_size_ = 500;

                uint16_t max_mentions_count_ = 30;

                std::shared_ptr<generic_custom_operation_interpreter<
                        follow::follow_plugin_operation>> _custom_operation_interpreter;

                std::unique_ptr<discussion_helper> helper;

                std::regex mention_regex_{"\\@[\\w\\d.-]+"};
            };

            plugin::plugin() {

            }

            void plugin::set_program_options(boost::program_options::options_description& cli,
                                                    boost::program_options::options_description& cfg) {
                cfg.add_options() (
                    "follow-max-feed-size", boost::program_options::value<uint32_t>()->default_value(500),
                    "Set the maximum size of cached feed for an account"
                ) (
                    "max-comment-mentions-count", boost::program_options::value<uint16_t>()->default_value(30),
                    "Set the maximum of @ mentions in comment or post, to be processed"
                );
            }

            void plugin::plugin_initialize(const boost::program_options::variables_map& options) {
                try {
                    ilog("Intializing follow plugin");
                    pimpl.reset(new impl());
                    auto& db = pimpl->database();
                    pimpl->plugin_initialize(*this);

                    db.pre_apply_operation.connect([&](operation_notification& o) {
                        pimpl->pre_operation(o, *this);
                    });
                    db.post_apply_operation.connect([&](const operation_notification& o) {
                        pimpl->post_operation(o, *this);
                    });
                    golos::chain::add_plugin_index<follow_index>(db);
                    golos::chain::add_plugin_index<feed_index>(db);
                    golos::chain::add_plugin_index<blog_index>(db);
                    golos::chain::add_plugin_index<follow_count_index>(db);
                    golos::chain::add_plugin_index<blog_author_stats_index>(db);

                    if (options.count("follow-max-feed-size")) {
                        uint32_t feed_size = options["follow-max-feed-size"].as<uint32_t>();
                        pimpl->max_feed_size_ = feed_size;
                    }
                    if (options.count("max-comment-mentions-count")) {
                        pimpl->max_mentions_count_ = options["max-comment-mentions-count"].as<uint16_t>();
                    }

                    JSON_RPC_REGISTER_API ( name() ) ;
                } FC_CAPTURE_AND_RETHROW()
            }

            void plugin::plugin_startup() {
            }

            uint32_t plugin::max_feed_size() {
                return pimpl->max_feed_size_;
            }

            uint16_t plugin::max_mentions_count() {
                return pimpl->max_mentions_count_;
            }

            const std::regex& plugin::mention_regex() {
                return pimpl->mention_regex_;
            }

            plugin::~plugin() {

            }


            std::vector<follow_api_object> plugin::impl::get_followers(
                    account_name_type account,
                    account_name_type start,
                    follow_type type,
                    uint32_t limit) {

                GOLOS_CHECK_LIMIT_PARAM(limit, 1000);
                std::vector<follow_api_object> result;
                result.reserve(limit);

                const auto& idx = database().get_index<follow_index>().indices().get<by_following_follower>();
                auto itr = idx.lower_bound(std::make_tuple(account, start));
                while (itr != idx.end() && result.size() < limit && itr->following == account) {
                    if (type == undefined || itr->what & (1 << type)) {
                        follow_api_object entry;
                        entry.follower = itr->follower;
                        entry.following = itr->following;
                        set_what(entry.what, itr->what);
                        result.push_back(entry);
                    }

                    ++itr;
                }

                return result;
            }

            std::vector<follow_api_object> plugin::impl::get_following(
                    account_name_type account,
                    account_name_type start,
                    follow_type type,
                    uint32_t limit) {

                GOLOS_CHECK_LIMIT_PARAM(limit, 100);
                std::vector<follow_api_object> result;
                const auto& idx = database().get_index<follow_index>().indices().get<by_follower_following>();
                auto itr = idx.lower_bound(std::make_tuple(account, start));
                while (itr != idx.end() && result.size() < limit && itr->follower == account) {
                    if (type == undefined || itr->what & (1 << type)) {
                        follow_api_object entry;
                        entry.follower = itr->follower;
                        entry.following = itr->following;
                        set_what(entry.what, itr->what);
                        result.push_back(entry);
                    }

                    ++itr;
                }

                return result;
            }

            follow_count_api_obj plugin::impl::get_follow_count(account_name_type acct) {
                follow_count_api_obj result;
                auto itr = database().find<follow_count_object, by_account>(acct);

                if (itr != nullptr) {
                    result = follow_count_api_obj(itr->account, itr->follower_count, itr->following_count, 1000);
                } else {
                    result.account = acct;
                }

                return result;
            }

            bool plugin::impl::category_matches_masks(const std::string& category, const std::set<std::string>* masks) {
                if (!masks) return false;
                bool found = false;
                for (const auto& mask : *masks) {
                    if (!mask.size()) continue;
                    if (mask.front() == '-') {
                        if (boost::algorithm::ends_with(category, mask)) {
                            found = true; break;
                        }
                    } else {
                        if (boost::algorithm::starts_with(category, mask)) {
                            found = true; break;
                        }
                    }
                }
                return found;
            }

            std::vector<feed_entry> plugin::impl::get_feed_entries(
                    account_name_type account,
                    uint32_t entry_id,
                    uint32_t limit,
                    const std::set<std::string>* filter_tag_masks) {
                GOLOS_CHECK_LIMIT_PARAM(limit, 500);

                if (entry_id == 0) {
                    entry_id = ~0;
                }

                std::vector<feed_entry> result;
                result.reserve(limit);

                const auto& db = database();
                const auto& feed_idx = db.get_index<feed_index, by_feed>();
                auto itr = feed_idx.lower_bound(std::make_tuple(account, entry_id));

                for (; itr != feed_idx.end() && itr->account == account && result.size() < limit; ++itr) {
                    const auto& comment = db.get(itr->comment);
                    const auto* extras = db.find_extras(comment.author, comment.hashlink);
                    if (!extras) continue;
                    if (category_matches_masks(to_string(extras->parent_permlink), filter_tag_masks)) continue;
                    feed_entry entry;
                    entry.author = comment.author;
                    entry.hashlink = comment.hashlink;
                    entry.permlink = to_string(extras->permlink);
                    entry.entry_id = itr->account_feed_id;
                    if (itr->first_reblogged_by != account_name_type()) {
                        entry.reblog_by.reserve(itr->reblogged_by.size());
                        entry.reblog_entries.reserve(itr->reblogged_by.size());
                        for (const auto& a : itr->reblogged_by) {
                            entry.reblog_by.push_back(a);
                            const auto& blog_idx = db.get_index<blog_index, by_comment>();
                            auto blog_itr = blog_idx.find(std::make_tuple(itr->comment, a));
                            entry.reblog_entries.emplace_back(
                                a,
                                to_string(blog_itr->reblog_title),
                                to_string(blog_itr->reblog_body),
                                to_string(blog_itr->reblog_json_metadata)
                            );
                        }
                        entry.reblog_on = itr->first_reblogged_on;
                    }
                    result.push_back(entry);
                }

                return result;
            }

            std::vector<comment_feed_entry> plugin::impl::get_feed(
                    account_name_type account,
                    uint32_t entry_id,
                    uint32_t limit,
                    const std::set<std::string>* filter_tag_masks) {
                GOLOS_CHECK_LIMIT_PARAM(limit, 500);

                if (entry_id == 0) {
                    entry_id = ~0;
                }

                std::vector<comment_feed_entry> result;
                result.reserve(limit);

                const auto& db = database();
                const auto& feed_idx = db.get_index<feed_index, by_feed>();
                auto itr = feed_idx.lower_bound(std::make_tuple(account, entry_id));

                for (; itr != feed_idx.end() && itr->account == account && result.size() < limit; ++itr) {
                    const auto& comment = db.get(itr->comment);
                    comment_feed_entry entry;
                    entry.comment = helper->create_comment_api_object(comment);
                    if (category_matches_masks(entry.comment.category, filter_tag_masks)) continue;
                    entry.entry_id = itr->account_feed_id;
                    if (itr->first_reblogged_by != account_name_type()) {
                        entry.reblog_by.reserve(itr->reblogged_by.size());
                        entry.reblog_entries.reserve(itr->reblogged_by.size());
                        for (const auto& a : itr->reblogged_by) {
                            entry.reblog_by.push_back(a);
                            const auto& blog_idx = db.get_index<blog_index, by_comment>();
                            auto blog_itr = blog_idx.find(std::make_tuple(itr->comment, a));
                            entry.reblog_entries.emplace_back(
                                a,
                                to_string(blog_itr->reblog_title),
                                to_string(blog_itr->reblog_body),
                                to_string(blog_itr->reblog_json_metadata)
                            );
                        }
                        entry.reblog_on = itr->first_reblogged_on;
                    }
                    result.push_back(entry);
                }

                return result;
            }

            std::vector<blog_entry> plugin::impl::get_blog_entries(
                    account_name_type account,
                    uint32_t entry_id,
                    uint32_t limit,
                    const std::set<std::string>* filter_tag_masks) {
                GOLOS_CHECK_LIMIT_PARAM(limit, 500);

                if (entry_id == 0) {
                    entry_id = ~0;
                }

                std::vector<blog_entry> result;
                result.reserve(limit);

                const auto& db = database();
                const auto& blog_idx = db.get_index<blog_index, by_blog>();
                auto itr = blog_idx.lower_bound(std::make_tuple(account, entry_id));

                for (; itr != blog_idx.end() && itr->account == account && result.size() < limit; ++itr) {
                    const auto& comment = db.get(itr->comment);
                    const auto* extras = db.find_extras(comment.author, comment.hashlink);
                    if (!extras) continue;
                    if (category_matches_masks(to_string(extras->parent_permlink), filter_tag_masks)) continue;
                    blog_entry entry;
                    entry.author = comment.author;
                    entry.hashlink = comment.hashlink;
                    entry.permlink = to_string(extras->permlink);
                    entry.blog = account;
                    entry.reblog_on = itr->reblogged_on;
                    entry.entry_id = itr->blog_feed_id;
                    entry.reblog_title = to_string(itr->reblog_title);
                    entry.reblog_body = to_string(itr->reblog_body);
                    entry.reblog_json_metadata = to_string(itr->reblog_json_metadata);

                    result.push_back(entry);
                }

                return result;
            }

            std::vector<comment_blog_entry> plugin::impl::get_blog(
                    account_name_type account,
                    uint32_t entry_id,
                    uint32_t limit,
                    const std::set<std::string>* filter_tag_masks) {
                GOLOS_CHECK_LIMIT_PARAM(limit, 500);

                if (entry_id == 0) {
                    entry_id = ~0;
                }

                std::vector<comment_blog_entry> result;
                result.reserve(limit);

                const auto& db = database();
                const auto& blog_idx = db.get_index<blog_index, by_blog>();
                auto itr = blog_idx.lower_bound(std::make_tuple(account, entry_id));

                for (; itr != blog_idx.end() && itr->account == account && result.size() < limit; ++itr) {
                    const auto& comment = db.get(itr->comment);
                    comment_blog_entry entry;
                    entry.comment = helper->create_comment_api_object(comment);
                    if (category_matches_masks(entry.comment.category, filter_tag_masks)) continue;
                    entry.blog = account;
                    entry.reblog_on = itr->reblogged_on;
                    entry.entry_id = itr->blog_feed_id;
                    entry.reblog_title = to_string(itr->reblog_title);
                    entry.reblog_body = to_string(itr->reblog_body);
                    entry.reblog_json_metadata = to_string(itr->reblog_json_metadata);

                    result.push_back(entry);
                }

                return result;
            }

            std::vector<account_reputation> plugin::impl::get_account_reputations(
                    std::vector<account_name_type> accounts
                ) {

                GOLOS_CHECK_PARAM(accounts,
                    GOLOS_CHECK_VALUE(accounts.size() <= 100, "Cannot retrieve more than 100 account reputations at a time."));

                std::vector<account_reputation> results;
                results.reserve(accounts.size());

                for (auto& name : accounts) {
                    account_reputation rep;
                    rep.account = name;
                    rep.reputation = database().get_account_reputation(name);
                    results.push_back(std::move(rep));
                }

                return results;
            }

            std::vector<account_name_type> plugin::impl::get_reblogged_by(
                    account_name_type author,
                    std::string permlink
            ) {
                auto& db = database();
                std::vector<account_name_type> result;
                const auto& post = db.get_comment_by_perm(author, permlink);
                const auto& blog_idx = db.get_index<blog_index, by_comment>();
                auto itr = blog_idx.lower_bound(post.id);
                while (itr != blog_idx.end() && itr->comment == post.id && result.size() < 2000) {
                    result.push_back(itr->account);
                    ++itr;
                }
                return result;
            }

            blog_authors_r plugin::impl::get_blog_authors(account_name_type blog_account) {
                auto& db = database();
                blog_authors_r result;
                const auto& stats_idx = db.get_index<blog_author_stats_index, by_blogger_guest_count>();
                auto itr = stats_idx.lower_bound(boost::make_tuple(blog_account));
                while (itr != stats_idx.end() && itr->blogger == blog_account && result.size()) {
                    result.emplace_back(itr->guest, itr->count);
                    ++itr;
                }
                return result;
            }

            DEFINE_API(plugin, get_followers) {
                PLUGIN_API_VALIDATE_ARGS(
                    (account_name_type, following)
                    (account_name_type, start_follower)
                    (follow_type,       type)
                    (uint32_t,          limit)
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_followers(following, start_follower, type, limit);
                });
            }

            DEFINE_API(plugin, get_following) {
                PLUGIN_API_VALIDATE_ARGS(
                    (account_name_type, follower)
                    (account_name_type, start_following)
                    (follow_type,       type)
                    (uint32_t,          limit)
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_following(follower, start_following, type, limit);
                });
            }

            DEFINE_API(plugin, get_follow_count) {
                PLUGIN_API_VALIDATE_ARGS(
                    (account_name_type, account)
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_follow_count(account);
                });
            }

            DEFINE_API(plugin, get_feed_entries){
                PLUGIN_API_VALIDATE_ARGS(
                    (account_name_type,     account)
                    (uint32_t,              entry_id, 0)
                    (uint32_t,              limit, 500)
                    (std::set<std::string>, filter_tag_masks, std::set<std::string>())
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_feed_entries(account, entry_id, limit, &filter_tag_masks);
                });
            }

            DEFINE_API(plugin, get_feed) {
                PLUGIN_API_VALIDATE_ARGS(
                    (account_name_type,     account)
                    (uint32_t,              entry_id, 0)
                    (uint32_t,              limit, 500)
                    (std::set<std::string>, filter_tag_masks, std::set<std::string>())
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_feed(account, entry_id, limit, &filter_tag_masks);
                });
            }

            DEFINE_API(plugin, get_blog_entries) {
                PLUGIN_API_VALIDATE_ARGS(
                    (account_name_type,     account)
                    (uint32_t,              entry_id, 0)
                    (uint32_t,              limit, 500)
                    (std::set<std::string>, filter_tag_masks, std::set<std::string>())
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_blog_entries(account, entry_id, limit, &filter_tag_masks);
                });
            }

            DEFINE_API(plugin, get_blog) {
                PLUGIN_API_VALIDATE_ARGS(
                    (account_name_type,     account)
                    (uint32_t,              entry_id, 0)
                    (uint32_t,              limit, 500)
                    (std::set<std::string>, filter_tag_masks, std::set<std::string>())
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_blog(account, entry_id, limit, &filter_tag_masks);
                });
            }

            DEFINE_API(plugin, get_account_reputations) {
                PLUGIN_API_VALIDATE_ARGS(
                    (std::vector<account_name_type>, accounts)
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_account_reputations(accounts);
                });
            }

            DEFINE_API(plugin, get_reblogged_by) {
                PLUGIN_API_VALIDATE_ARGS(
                    (account_name_type, author)
                    (std::string,       permlink)
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_reblogged_by(author, permlink);
                });
            }

            DEFINE_API(plugin, get_blog_authors) {
                PLUGIN_API_VALIDATE_ARGS(
                    (account_name_type, account)
                )
                return pimpl->database().with_weak_read_lock([&]() {
                    return pimpl->get_blog_authors(account);
                });
            }
        }
    }
} // golos::follow
