#pragma once

#include <golos/chain/comment_object.hpp>
#include <golos/chain/steem_object_types.hpp>

namespace golos {
    namespace plugins {
        namespace follow {
            using protocol::account_name_type;
            using protocol::share_type;
            using chainbase::object;
            using chainbase::object_id;
            using chainbase::allocator;
            using golos::chain::comment_object;
            using golos::chain::by_id;
            using golos::chain::comment_vote_index;
            using golos::chain::by_comment_voter;
            using golos::chain::shared_string;

            namespace bip = boost::interprocess;

#ifndef FOLLOW_SPACE_ID
#define FOLLOW_SPACE_ID 8
#endif

            enum follow_plugin_object_type {
                follow_object_type = (FOLLOW_SPACE_ID << 8),
                feed_object_type = (FOLLOW_SPACE_ID << 8) + 1,
                blog_object_type = (FOLLOW_SPACE_ID << 8) + 2,
                follow_count_object_type = (FOLLOW_SPACE_ID << 8) + 3,
                blog_author_stats_object_type = (FOLLOW_SPACE_ID << 8) + 4
            };


            class follow_object : public object<follow_object_type, follow_object> {
            public:
                template<typename Constructor, typename Allocator>
                follow_object(Constructor &&c, allocator<Allocator> a) {
                    c(*this);
                }

                id_type id;

                account_name_type follower;
                account_name_type following;
                uint16_t what = 0;
            };

            typedef object_id<follow_object> follow_id_type;

            class feed_object : public object<feed_object_type, feed_object> {
            public:
                feed_object() = delete;

                template<typename Constructor, typename Allocator>
                feed_object(Constructor &&c, allocator<Allocator> a)
                        :reblogged_by(a) {
                    c(*this);
                }

                id_type id;

                account_name_type account;
                bip::vector<account_name_type, allocator<account_name_type>> reblogged_by;
                account_name_type first_reblogged_by;
                time_point_sec first_reblogged_on;
                comment_object::id_type comment;
                uint32_t reblogs;
                uint32_t account_feed_id = 0;
            };

            typedef object_id<feed_object> feed_id_type;

            class blog_object : public object<blog_object_type, blog_object> {
            public:
                blog_object() = delete;

                template<typename Constructor, typename Allocator>
                blog_object(Constructor &&c, allocator<Allocator> a)
                        : reblog_title(a), reblog_body(a), reblog_json_metadata(a) {
                    c(*this);
                }

                id_type id;

                account_name_type account;
                comment_object::id_type comment;
                time_point_sec reblogged_on;
                uint32_t blog_feed_id = 0;

                shared_string reblog_title;
                shared_string reblog_body;
                shared_string reblog_json_metadata;
            };

            typedef object_id<blog_object> blog_id_type;

            /**
             *  This index is maintained to get an idea of which authors are resteemed by a particular blogger and
             *  how frequnetly. It is designed to give an overview of the type of people a blogger sponsors as well
             *  as to enable generation of filter set for a blog list.
             *
             *  Give me the top authors promoted by this blog
             *  Give me all blog posts by [authors] that were resteemed by this blog
             */
            class blog_author_stats_object : public object<blog_author_stats_object_type, blog_author_stats_object> {
            public:
                template<typename Constructor, typename Allocator>
                blog_author_stats_object(Constructor &&c, allocator<Allocator> a) {
                    c(*this);
                }

                id_type id;
                account_name_type blogger;
                account_name_type guest;
                uint32_t count = 0;
            };

            typedef object_id<blog_author_stats_object> blog_author_stats_id_type;

            class follow_count_object : public object<follow_count_object_type, follow_count_object> {
            public:
                template<typename Constructor, typename Allocator>
                follow_count_object(Constructor &&c, allocator<Allocator> a) {
                    c(*this);
                }

                id_type id;

                account_name_type account;
                uint32_t follower_count = 0;
                uint32_t following_count = 0;
            };

            typedef object_id<follow_count_object> follow_count_id_type;


            struct by_following_follower;
            struct by_follower_following;

            using namespace boost::multi_index;

            typedef multi_index_container<follow_object,
                    indexed_by<ordered_unique<tag<by_id>, member<follow_object, follow_id_type, &follow_object::id>>,
                            ordered_unique<tag<by_following_follower>, composite_key<follow_object,
                                    member<follow_object, account_name_type, &follow_object::following>,
                                    member<follow_object, account_name_type, &follow_object::follower> >,
                                    composite_key_compare<std::less<account_name_type>, std::less<account_name_type>>>,
                            ordered_unique<tag<by_follower_following>, composite_key<follow_object,
                                    member<follow_object, account_name_type, &follow_object::follower>,
                                    member<follow_object, account_name_type, &follow_object::following> >,
                                    composite_key_compare<std::less<account_name_type>,
                                            std::less<account_name_type>>> >, allocator<follow_object> > follow_index;

            struct by_blogger_guest_count;
            typedef chainbase::shared_multi_index_container<blog_author_stats_object, indexed_by<
                    ordered_unique<tag<by_id>,
                            member<blog_author_stats_object, blog_author_stats_id_type, &blog_author_stats_object::id>>,
                    ordered_unique<tag<by_blogger_guest_count>, composite_key<blog_author_stats_object,
                            member<blog_author_stats_object, account_name_type, &blog_author_stats_object::blogger>,
                            member<blog_author_stats_object, account_name_type, &blog_author_stats_object::guest>,
                            member<blog_author_stats_object, uint32_t, &blog_author_stats_object::count> >,
                            composite_key_compare<std::less<account_name_type>, std::less<account_name_type>,
                                    std::greater<uint32_t>>> > > blog_author_stats_index;

            struct by_feed;
            struct by_old_feed;
            struct by_account;
            struct by_comment;

            typedef multi_index_container<feed_object,
                    indexed_by<ordered_unique<tag<by_id>, member<feed_object, feed_id_type, &feed_object::id>>,
                            ordered_unique<tag<by_feed>, composite_key<feed_object,
                                    member<feed_object, account_name_type, &feed_object::account>,
                                    member<feed_object, uint32_t, &feed_object::account_feed_id> >,
                                    composite_key_compare<std::less<account_name_type>, std::greater<uint32_t>>>,
                            ordered_unique<tag<by_old_feed>, composite_key<feed_object,
                                    member<feed_object, account_name_type, &feed_object::account>,
                                    member<feed_object, uint32_t, &feed_object::account_feed_id> >,
                                    composite_key_compare<std::less<account_name_type>, std::less<uint32_t>>>,
                            ordered_unique<tag<by_account>, composite_key<feed_object,
                                    member<feed_object, account_name_type, &feed_object::account>,
                                    member<feed_object, feed_id_type, &feed_object::id> >,
                                    composite_key_compare<std::less<account_name_type>, std::less<feed_id_type>>>,
                            ordered_unique<tag<by_comment>, composite_key<feed_object,
                                    member<feed_object, comment_object::id_type, &feed_object::comment>,
                                    member<feed_object, account_name_type, &feed_object::account> >,
                                    composite_key_compare<std::less<comment_object::id_type>,
                                            std::less<account_name_type>>> >, allocator<feed_object> > feed_index;

            struct by_blog;
            struct by_old_blog;

            typedef multi_index_container<blog_object,
                    indexed_by<ordered_unique<tag<by_id>, member<blog_object, blog_id_type, &blog_object::id>>,
                            ordered_unique<tag<by_blog>, composite_key<blog_object,
                                    member<blog_object, account_name_type, &blog_object::account>,
                                    member<blog_object, uint32_t, &blog_object::blog_feed_id> >,
                                    composite_key_compare<std::less<account_name_type>, std::greater<uint32_t>>>,
                            ordered_unique<tag<by_old_blog>, composite_key<blog_object,
                                    member<blog_object, account_name_type, &blog_object::account>,
                                    member<blog_object, uint32_t, &blog_object::blog_feed_id> >,
                                    composite_key_compare<std::less<account_name_type>, std::less<uint32_t>>>,
                            ordered_unique<tag<by_comment>, composite_key<blog_object,
                                    member<blog_object, comment_object::id_type, &blog_object::comment>,
                                    member<blog_object, account_name_type, &blog_object::account> >,
                                    composite_key_compare<std::less<comment_object::id_type>,
                                            std::less<account_name_type>>> >, allocator<blog_object> > blog_index;

            struct by_followers;
            struct by_following;

            typedef multi_index_container<follow_count_object, indexed_by<ordered_unique<tag<by_id>,
                    member<follow_count_object, follow_count_id_type, &follow_count_object::id>>,
                    ordered_unique<tag<by_account>,
                            member<follow_count_object, account_name_type, &follow_count_object::account>>,
                    ordered_unique<tag<by_followers>, composite_key<follow_count_object,
                            member<follow_count_object, uint32_t, &follow_count_object::follower_count>,
                            member<follow_count_object, follow_count_id_type, &follow_count_object::id> >,
                            composite_key_compare<std::greater<uint32_t>, std::less<follow_count_id_type>>>,
                    ordered_unique<tag<by_following>, composite_key<follow_count_object,
                            member<follow_count_object, uint32_t, &follow_count_object::following_count>,
                            member<follow_count_object, follow_count_id_type, &follow_count_object::id> >,
                            composite_key_compare<std::greater<uint32_t>, std::less<follow_count_id_type>>> >,
                    allocator<follow_count_object> > follow_count_index;

        }
    }
} // golos::follow



FC_REFLECT((golos::plugins::follow::follow_object), (id)(follower)(following)(what))
CHAINBASE_SET_INDEX_TYPE(golos::plugins::follow::follow_object, golos::plugins::follow::follow_index)

FC_REFLECT((golos::plugins::follow::feed_object),
           (id)(account)(first_reblogged_by)(first_reblogged_on)(comment)(reblogs)(account_feed_id))
CHAINBASE_SET_INDEX_TYPE(golos::plugins::follow::feed_object, golos::plugins::follow::feed_index)

FC_REFLECT((golos::plugins::follow::blog_object), (id)(account)(comment)(reblogged_on)(blog_feed_id))
CHAINBASE_SET_INDEX_TYPE(golos::plugins::follow::blog_object, golos::plugins::follow::blog_index)

FC_REFLECT((golos::plugins::follow::follow_count_object), (id)(account)(follower_count)(following_count))
CHAINBASE_SET_INDEX_TYPE(golos::plugins::follow::follow_count_object, golos::plugins::follow::follow_count_index)

FC_REFLECT((golos::plugins::follow::blog_author_stats_object), (id)(blogger)(guest)(count))
CHAINBASE_SET_INDEX_TYPE(golos::plugins::follow::blog_author_stats_object,
                         golos::plugins::follow::blog_author_stats_index);
