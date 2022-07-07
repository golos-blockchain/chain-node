#include <golos/plugins/follow/follow_operations.hpp>
#include <golos/plugins/follow/follow_objects.hpp>
#include <golos/plugins/follow/follow_evaluators.hpp>
#include <golos/chain/account_object.hpp>
#include <golos/chain/comment_object.hpp>

namespace golos {
    namespace plugins {
        namespace follow {
            using golos::chain::from_string;

            void save_blog_stats(database& db, account_name_type blogger, account_name_type guest, uint32_t start_count = 0) {

                const auto& stats_idx = db.get_index<blog_author_stats_index, by_blogger_guest_count>();
                auto stats_itr = stats_idx.lower_bound(boost::make_tuple(blogger, guest));
                if (stats_itr != stats_idx.end() && stats_itr->blogger == blogger && stats_itr->guest == guest) {
                    db.modify(*stats_itr, [&](blog_author_stats_object& s) {
                        if (start_count > 0) {
                            ++s.count;
                        } else {
                            --s.count;
                        }
                    });
                } else {
                    db.create<blog_author_stats_object>([&](blog_author_stats_object& s) {
                        s.count = start_count;
                        s.blogger = blogger;
                        s.guest = guest;
                    });
                }
            }

            void follow_evaluator::do_apply(const follow_operation& o) {
                try {
                    static map<string, follow_type> follow_type_map = []() {
                        map<string, follow_type> follow_map;
                        follow_map["undefined"] = follow_type::undefined;
                        follow_map["blog"] = follow_type::blog;
                        follow_map["ignore"] = follow_type::ignore;

                        return follow_map;
                    }();

                    const auto& idx = db().get_index<follow_index>().indices().get<by_follower_following>();
                    auto itr = idx.find(boost::make_tuple(o.follower, o.following));

                    uint16_t what = 0;
                    bool is_following = false;

                    for (auto target : o.what) {
                        switch (follow_type_map[target]) {
                            case blog:
                                what |= 1 << blog;
                                is_following = true;
                                break;
                            case ignore:
                                what |= 1 << ignore;
                                break;
                            default:
                                //ilog( "Encountered unknown option ${o}", ("o", target) );
                                break;
                        }
                    }

                    bool hf27 = db().has_hardfork(STEEMIT_HARDFORK_0_27);

                    if (what & (1 << ignore)) {
                        GOLOS_CHECK_VALUE(!hf27,
                            "Follow ignore list is deprecated now, use account_setup");
                        GOLOS_CHECK_LOGIC(!(what & (1 << blog)),
                                logic_errors::cannot_follow_and_ignore_simultaneously,
                                "Cannot follow blog and ignore author at the same time");
                    }

                    if (what & (1 << blog)) {
                        GOLOS_CHECK_VALUE(db().is_blocking(o.follower, o.following) != 1,
                                "You cannot follow because you was blocked user");
                        GOLOS_CHECK_VALUE(db().is_blocking(o.following, o.follower) != 1,
                                "You cannot follow because you are blocked by user");
                    }

                    bool was_followed = false;

                    if (itr == idx.end()) {
                        db().create<follow_object>([&](follow_object& obj) {
                            obj.follower = o.follower;
                            obj.following = o.following;
                            obj.what = what;
                        });
                    } else {
                        was_followed = itr->what & 1 << blog;

                        db().modify(*itr, [&](follow_object& obj) {
                            obj.what = what;
                        });
                    }

                    const auto& follower = db().find<follow_count_object, by_account>(o.follower);

                    if (follower == nullptr) {
                        db().create<follow_count_object>([&](follow_count_object& obj) {
                            obj.account = o.follower;

                            if (is_following) {
                                obj.following_count = 1;
                            }
                        });
                    } else {
                        db().modify(*follower, [&](follow_count_object& obj) {
                            if (was_followed) {
                                obj.following_count--;
                            }
                            if (is_following) {
                                obj.following_count++;
                            }
                        });
                    }

                    const auto& following = db().find<follow_count_object, by_account>(o.following);

                    if (following == nullptr) {
                        db().create<follow_count_object>([&](follow_count_object& obj) {
                            obj.account = o.following;

                            if (is_following) {
                                obj.follower_count = 1;
                            }
                        });
                    } else {
                        db().modify(*following, [&](follow_count_object& obj) {
                            if (was_followed) {
                                obj.follower_count--;
                            }
                            if (is_following) {
                                obj.follower_count++;
                            }
                        });
                    }
                }
                FC_CAPTURE_AND_RETHROW((o))
            }

            void reblog_evaluator::do_apply(const reblog_operation& o) {
                try {
                    const auto& c = db().get_comment_by_perm(o.author, o.permlink);
                    GOLOS_CHECK_LOGIC(c.parent_author.size() == 0, 
                            logic_errors::only_top_level_posts_reblogged,
                            "Only top level posts can be reblogged");

                    const auto& blog_idx = db().get_index<blog_index>().indices().get<by_blog>();
                    const auto& blog_comment_idx = db().get_index<blog_index>().indices().get<by_comment>();

                    auto next_blog_id = 0;
                    auto last_blog = blog_idx.lower_bound(o.account);

                    if (last_blog != blog_idx.end() && last_blog->account == o.account) {
                        next_blog_id = last_blog->blog_feed_id + 1;
                    }

                    auto blog_itr = blog_comment_idx.find(boost::make_tuple(c.id, o.account));

                    GOLOS_CHECK_LOGIC(blog_itr == blog_comment_idx.end(), 
                            logic_errors::account_already_reblogged_this_post,
                            "Account has already reblogged this post");
                    db().create<blog_object>([&](blog_object& b) {
                        b.account = o.account;
                        b.comment = c.id;
                        b.reblogged_on = db().head_block_time();
                        b.blog_feed_id = next_blog_id;
                        from_string(b.reblog_title, o.title);
                        from_string(b.reblog_body, o.body);
                        from_string(b.reblog_json_metadata, o.json_metadata);
                    });

                    save_blog_stats(db(), o.account, c.author, 1);

                    const auto& feed_idx = db().get_index<feed_index>().indices().get<by_feed>();
                    const auto& comment_idx = db().get_index<feed_index>().indices().get<by_comment>();
                    const auto& idx = db().get_index<follow_index>().indices().get<by_following_follower>();
                    
                    for (auto itr = idx.find(o.account); itr != idx.end() && itr->following == o.account; ++itr) {

                        if (itr->what & (1 << blog)) {
                            uint32_t next_id = 0;
                            auto last_feed = feed_idx.lower_bound(itr->follower);

                            if (last_feed != feed_idx.end() && last_feed->account == itr->follower) {
                                next_id = last_feed->account_feed_id + 1;
                            }

                            auto feed_itr = comment_idx.find(boost::make_tuple(c.id, itr->follower));

                            if (feed_itr == comment_idx.end()) {
                                db().create<feed_object>([&](feed_object& f) {
                                    f.account = itr->follower;
                                    f.reblogged_by.push_back(o.account);
                                    f.first_reblogged_by = o.account;
                                    f.first_reblogged_on = db().head_block_time();
                                    f.comment = c.id;
                                    f.reblogs = 1;
                                    f.account_feed_id = next_id;
                                });
                            } else {
                                db().modify(*feed_itr, [&](feed_object& f) {
                                    f.reblogged_by.push_back(o.account);
                                    f.reblogs++;
                                });
                            }

                            const auto& old_feed_idx = db().get_index<feed_index>().indices().get<by_old_feed>();
                            auto old_feed = old_feed_idx.lower_bound(itr->follower);

                            while (old_feed->account == itr->follower && next_id - old_feed->account_feed_id > _plugin->max_feed_size()) {
                                db().remove(*old_feed);
                                old_feed = old_feed_idx.lower_bound(itr->follower);
                            };
                        }
                    }
                } FC_CAPTURE_AND_RETHROW((o))
            }

            void delete_reblog_evaluator::do_apply(const delete_reblog_operation& o) {
                try {
                    const auto& c = db().get_comment_by_perm(o.author, o.permlink);

                    // Deleting blog object

                    const auto& blog_comment_idx = db().get_index<blog_index>().indices().get<by_comment>();
                    
                    auto blog_itr = blog_comment_idx.find(boost::make_tuple(c.id, o.account));

                    GOLOS_CHECK_LOGIC(blog_itr != blog_comment_idx.end(), 
                            logic_errors::account_has_not_reblogged_this_post,
                            "Account has not reblogged this post");

                    db().remove(*blog_itr);

                    // Fixing blog statistics

                    save_blog_stats(db(), o.account, c.author, 0);

                    // Removing info about reblog from feed_objects for followers of reblogger

                    const auto& comment_idx = db().get_index<feed_index>().indices().get<by_comment>();
                    const auto& idx = db().get_index<follow_index>().indices().get<by_following_follower>();
                    
                    for (auto itr = idx.find(o.account); itr != idx.end() && itr->following == o.account; ++itr) {

                        if (itr->what & (1 << blog)) {

                            auto feed_itr = comment_idx.find(boost::make_tuple(c.id, itr->follower));

                            if (feed_itr != comment_idx.end()) {
                                auto pos = std::find(feed_itr->reblogged_by.begin(), feed_itr->reblogged_by.end(), o.account);
                                if (pos == feed_itr->reblogged_by.end()) {
                                    continue;
                                }
                                if (feed_itr->reblogs <= 1) {
                                    db().remove(*feed_itr);
                                } else {
                                    db().modify(*feed_itr, [&](feed_object& f) {
                                        f.reblogged_by.erase(pos);
                                        f.reblogs--;
                                    });
                                }
                            }
                        }
                    }

                    
                } FC_CAPTURE_AND_RETHROW((o))
            }

        }
    }
} // golos::follow
