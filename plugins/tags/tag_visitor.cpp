#include <boost/algorithm/string.hpp>
#include <golos/plugins/tags/tag_visitor.hpp>
#include <golos/plugins/social_network/social_network.hpp>

namespace golos { namespace plugins { namespace tags {

    using golos::plugins::social_network::comment_last_update_index;

    comment_metadata get_metadata(
        const std::string& json_metadata, std::size_t tags_number, std::size_t tag_max_length
    ) {
        comment_metadata meta;

        if (!json_metadata.empty()) {
            try {
                meta = fc::json::from_string(json_metadata).as<comment_metadata>();
            } catch (const fc::exception& e) {
                // Do nothing on malformed json_metadata
            }
        }

        std::set<std::string> lower_tags;

        for (const auto& name : meta.tags) {
            if (lower_tags.size() > tags_number) {
                break;
            }
            auto value = boost::trim_copy(name);
            if (value.empty()) {
                continue;
            }
            boost::to_lower(value);
            // skip long tags
            if (value.size() > tag_max_length) {
                continue;
            }
            lower_tags.insert(std::move(value));
        }

        meta.tags.swap(lower_tags);

        boost::trim(meta.language);
        boost::to_lower(meta.language);
        if (meta.language.size() > tag_max_length) {
            meta.language.clear();
        }

        return meta;
    }

    comment_metadata get_metadata(
        const golos::chain::database& db, const comment_object& c, std::size_t tags_number, std::size_t tag_max_length
    ) {
        return get_metadata(golos::plugins::social_network::get_json_metadata(db, c), tags_number, tag_max_length);
    }

    operation_visitor::operation_visitor(database& db, std::size_t tags_number, std::size_t tag_max_length)
        : db_(db),
          tags_number_(tags_number),
          tag_max_length_(tag_max_length) {
    }

    void operation_visitor::remove_stats(const tag_object& tag) const {
        const auto& idx = db_.get_index<tag_stats_index>().indices().get<by_tag>();
        auto itr = idx.find(std::make_tuple(tag.type, tag.name));
        if (itr == idx.end()) {
            return;
        }

        bool need_remove = false;
        db_.modify(*itr, [&](tag_stats_object& s) {
            if (tag.parent == comment_object::id_type()) {
                s.total_children_rshares2 -= tag.children_rshares2;
                s.top_posts--;
            } else {
                s.comments--;
            }
            s.net_votes -= tag.net_votes;

            need_remove = (s.top_posts == 0) && (s.comments == 0);
        });

        if (need_remove) {
            if (itr->type == tag_type::language) {
                auto& lidx = db_.get_index<language_index>().indices().get<by_tag>();
                auto litr = lidx.find(tag.name);
                if (lidx.end() != litr) {
                    db_.remove(*litr);
                }
            }
            db_.remove(*itr);
        }
    }

    const tag_stats_object& operation_visitor::get_stats(const tag_object& tag) const {
        const auto& idx = db_.get_index<tag_stats_index>().indices().get<by_tag>();
        auto itr = idx.find(std::make_tuple(tag.type, tag.name));
        if (itr != idx.end()) {
            return *itr;
        }

        return db_.create<tag_stats_object>([&](tag_stats_object& stats) {
            stats.name = tag.name;
            stats.type = tag.type;
        });
    }

    void operation_visitor::add_stats(const tag_object& tag) const {
        db_.modify(get_stats(tag), [&](tag_stats_object& s) {
            if (tag.parent == comment_object::id_type()) {
                s.total_children_rshares2 += tag.children_rshares2;
                s.top_posts++;
            } else {
                s.comments++;
            }
            s.net_votes += tag.net_votes;
        });
    }

    void operation_visitor::remove_tag(const tag_object& tag) const {
        const auto& idx = db_.get_index<author_tag_stats_index>().indices().get<by_author_tag_posts>();
        auto itr = idx.lower_bound(std::make_tuple(tag.author, tag.type, tag.name));
        if (itr != idx.end() && itr->author == tag.author && itr->name == tag.name && itr->type == tag.type) {
            if (itr->total_posts == 1) {
                db_.remove(*itr);
            } else {
                db_.modify(*itr, [&](author_tag_stats_object& stats) {
                    stats.total_posts--;
                });
            }
        }

        remove_stats(tag);
        db_.remove(tag);
    }

    comment_date operation_visitor::get_comment_last_update(const comment_object& comment) const {
        comment_date result;
        if (db_.has_index<comment_last_update_index>()) {
            const auto& clu_idx = db_.get_index<comment_last_update_index>().indices().get<golos::plugins::social_network::by_comment>();
            auto clu_itr = clu_idx.find(comment.id);
            if (clu_itr != clu_idx.end()) {
                result.active = clu_itr->active;
                result.last_update = clu_itr->last_update;
                return result;
            }
        }
        return result;
    }

    void operation_visitor::update_tag(
        const tag_object& current, const comment_object& comment, double hot, double trending
    ) const {
        auto cashout_time = db_.calculate_discussion_payout_time(comment);
        remove_stats(current);

        db_.modify(current, [&](tag_object& obj) {
            obj.active = get_comment_last_update(comment).active;
            obj.cashout = cashout_time;
            obj.children = comment.children;
            obj.net_rshares = comment.net_rshares.value;
            obj.net_votes = comment.net_votes;
            obj.children_rshares2 = comment.children_rshares2;
            obj.hot = hot;
            obj.trending = trending;
            if (obj.cashout == fc::time_point_sec()) {
                obj.promoted_balance = 0;
            }
        });
        add_stats(current);
    }

    void operation_visitor::create_tag(
        const std::string& name, const tag_type type, const comment_object& comment, double hot, double trending
    ) const {
        auto author = db_.get_account(comment.author).id;

        comment_object::id_type parent;
        if (comment.parent_author.size()) {
            parent = db_.get_comment(comment.parent_author, comment.parent_permlink).id;
        }

        auto com_date = get_comment_last_update(comment);

        const auto& tag_obj = db_.create<tag_object>([&](tag_object& obj) {
            obj.name = name;
            obj.type = type;
            obj.comment = comment.id;
            obj.parent = parent;
            obj.created = comment.created;
            obj.active = com_date.active;
            obj.updated = com_date.last_update;
            obj.cashout = comment.cashout_time;
            obj.net_votes = comment.net_votes;
            obj.children = comment.children;
            obj.net_rshares = comment.net_rshares.value;
            obj.children_rshares2 = comment.children_rshares2;
            obj.author = author;
            obj.hot = hot;
            obj.trending = trending;
        });

        add_stats(tag_obj);

        const auto& idx = db_.get_index<author_tag_stats_index>().indices().get<by_author_tag_posts>();
        auto itr = idx.lower_bound(std::make_tuple(author, type, name));
        if (itr != idx.end() && itr->author == author && itr->name == name) {
            db_.modify(*itr, [&](author_tag_stats_object& stats) {
                stats.total_posts++;
            });
        } else {
            db_.create<author_tag_stats_object>([&](author_tag_stats_object& stats) {
                stats.author = author;
                stats.name = name;
                stats.type = type;
                stats.total_posts = 1;
            });
        }

        if (type == tag_type::language) {
            auto& lidx = db_.get_index<language_index>().indices().get<by_tag>();
            auto litr = lidx.find(name);
            if (litr == lidx.end()) {
                db_.create<language_object>([&](language_object& object) {
                    object.name = name;
                });
            }
        }
    }

    double operation_visitor::calculate_hot(const share_type& score, const time_point_sec& created) const {
        return calculate_score<10000000, 100000>(score, created);
    }

    double operation_visitor::calculate_trending(const share_type& score, const time_point_sec& created) const {
        return calculate_score<10000000, 480000>(score, created);
    }

    void operation_visitor::create_update_tags(
        const account_name_type& author, const std::string& permlink
    ) const { try {
        const auto& comment = db_.get_comment(author, permlink);
        auto hot = calculate_hot(comment.net_rshares, comment.created);
        auto trending = calculate_trending(comment.net_rshares, comment.created);
        const auto& comment_idx = db_.get_index<tag_index>().indices().get<by_comment>();

        auto meta = get_metadata(db_, comment, tags_number_, tag_max_length_);
        auto citr = comment_idx.lower_bound(comment.id);
        const tag_object* language_tag = nullptr;

        if (meta.tags.empty()) {
            meta.tags.insert(std::string());
        }

        std::map<std::string, const tag_object*> existing_tags;
        std::vector<const tag_object*> remove_queue;
        for (; citr != comment_idx.end() && citr->comment == comment.id; ++citr) {
            const tag_object* tag = &*citr;
            switch (tag->type) {
                case tag_type::tag:
                    if (meta.tags.find(tag->name) == meta.tags.end()) {
                        remove_queue.push_back(tag);
                    } else {
                        existing_tags[tag->name] = tag;
                    }
                    break;

                case tag_type::language:
                    language_tag = tag;
                    if (meta.language != language_tag->name) {
                        remove_queue.push_back(tag);
                    }
                    break;
            }
        }

        for (const auto& name : meta.tags) {
            auto existing = existing_tags.find(name);
            if (existing == existing_tags.end()) {
                create_tag(name, tag_type::tag, comment, hot, trending);
            } else {
                update_tag(*existing->second, comment, hot, trending);
            }
        }

        if (!meta.language.empty() && (!language_tag || meta.language != language_tag->name)) {
            create_tag(meta.language, tag_type::language, comment, hot, trending);
        }

        for (const auto& item : remove_queue) {
            remove_tag(*item);
        }
    } FC_CAPTURE_LOG_AND_RETHROW(()) }

    void operation_visitor::update_tags(const account_name_type& author, const std::string& permlink) const {
        const auto& comment = db_.get_comment(author, permlink);
        auto hot = calculate_hot(comment.net_rshares, comment.created);
        auto trending = calculate_trending(comment.net_rshares, comment.created);
        const auto& comment_idx = db_.get_index<tag_index>().indices().get<by_comment>();

        auto citr = comment_idx.lower_bound(comment.id);
        for (; citr != comment_idx.end() && citr->comment == comment.id; ++citr) {
            update_tag(*citr, comment, hot, trending);
        }

        if (comment.parent_author.size()) {
            update_tags(comment.parent_author, to_string(comment.parent_permlink));
        }
    }

    void operation_visitor::remove_tags(const account_name_type& author, const std::string& permlink) const {
        const auto& comment = db_.get_comment(author, permlink);
        const auto& comment_idx = db_.get_index<tag_index>().indices().get<by_comment>();
        std::vector<const tag_object*> remove_queue;

        remove_queue.reserve(10);
        auto citr = comment_idx.lower_bound(comment.id);
        for (; citr != comment_idx.end() && citr->comment == comment.id; ++citr) {
            const tag_object* tag = &*citr;
            remove_queue.push_back(tag);
        }

        for (const auto& tag : remove_queue) {
            remove_tag(*tag);
        }

        if (comment.parent_author.size()) {
            update_tags(comment.parent_author, to_string(comment.parent_permlink));
        }
    }

    void operation_visitor::operator()(const transfer_operation& op) const {
        if (op.to == STEEMIT_NULL_ACCOUNT && op.amount.symbol == SBD_SYMBOL) {
            std::vector<std::string> part;
            part.reserve(4);
            auto path = op.memo;
            boost::split(part, path, boost::is_any_of("/"));
            if (!part[0].empty() && part[0][0] == '@' && part.size() >= 2) {
                auto acnt = part[0].substr(1);
                auto perm = part[1];

                auto c = db_.find_comment(acnt, perm);
                if (c && c->parent_author.size() == 0) {
                    const auto& comment_idx = db_.get_index<tag_index>().indices().get<by_comment>();
                    auto citr = comment_idx.lower_bound(c->id);
                    while (citr != comment_idx.end() && citr->comment == c->id) {
                        db_.modify(*citr, [&](tag_object& t) {
                            if (t.cashout != fc::time_point_sec::maximum()) {
                                t.promoted_balance += op.amount.amount;
                            }
                        });
                        ++citr;
                    }
                }
            }
        }
    }

    void operation_visitor::operator()(const donate_operation& op) const {
        if (op.amount.symbol != STEEM_SYMBOL)
            return;
        const comment_object* comment = nullptr;
        try {
            auto author_str = op.memo.target["author"].as_string();
            auto permlink = op.memo.target["permlink"].as_string();

            if (is_valid_account_name(author_str)) {
                auto author = account_name_type(author_str);

                comment = db_.find_comment(author, permlink);
            }
        } catch (...) {}
        if (comment != nullptr) {
            const auto& comment_idx = db_.get_index<tag_index, by_comment>();
            auto citr = comment_idx.lower_bound(comment->id);
            for (; citr != comment_idx.end() && citr->comment == comment->id; ++citr) {
                db_.modify(*citr, [&](auto& t) {
                    t.donates += op.amount;
                });
            }
        }
    }

    void operation_visitor::operator()(const delete_comment_operation& op) const {
        const auto& author = db_.get_account(op.author).id;
        const auto& idx = db_.get_index<tag_index>().indices().get<by_author_comment>();
        auto itr = idx.lower_bound(author);
        while (itr != idx.end() && itr->author == author) {
            const auto& tag = *itr;
            const auto* comment = db_.find<comment_object>(itr->comment);
            ++itr;
            if (!comment) {
                remove_tag(tag);
            }
        }
    }

    void operation_visitor::operator()(const comment_reward_operation& op) const {
        // only update existing tags
        update_tags(op.author, op.permlink);

        const auto& comment = db_.get_comment(op.author, op.permlink);
        const auto& author = db_.get_account(op.author).id;

        auto meta = get_metadata(db_, comment, tags_number_, tag_max_length_);
        const auto& stats_idx = db_.get_index<tag_stats_index>().indices().get<by_tag>();
        const auto& auth_idx = db_.get_index<author_tag_stats_index>().indices().get<by_author_tag_posts>();

        auto update_payout = [&](const tag_type type, const auto& name) {
            auto sitr = stats_idx.find(std::make_tuple(type, name));
            if (stats_idx.end() != sitr) {
                db_.modify(*sitr, [&](tag_stats_object& ts) {
                    ts.total_payout.amount += op.payout.amount;
                });
            }

            auto aitr = auth_idx.find(std::make_tuple(author, type, name));
            if (auth_idx.end() != aitr) {
                db_.modify(*aitr, [&](author_tag_stats_object& as) {
                    as.total_rewards.amount += op.payout.amount;
                });
            }
        };

        for (const auto& name : meta.tags) {
            update_payout(tag_type::tag, name );
        }

        if (!meta.language.empty()) {
            update_payout(tag_type::language, meta.language);
        }
    }

    void operation_visitor::operator()(const comment_operation& op) const {
        const auto& comment = db_.get_comment(op.author, op.permlink);

        if (db_.calculate_discussion_payout_time(comment) != fc::time_point_sec::maximum()) {
            // in a cashout window
            create_update_tags(op.author, op.permlink);
        }
    }

    void operation_visitor::operator()(const vote_operation& op) const {
        // only update existing tags
        update_tags(op.author, op.permlink);
    }

    void operation_visitor::operator()(const comment_payout_update_operation& op) const {
        const auto& comment = db_.get_comment(op.author, op.permlink);
        const auto cashout_time = db_.calculate_discussion_payout_time(comment);

        if (cashout_time != fc::time_point_sec::maximum()) {
            update_tags(op.author, op.permlink);
        } else {
            // it can be the end of a cashout window
            remove_tags(op.author, op.permlink);
        }
    }

} } } // golos::plugins::tags