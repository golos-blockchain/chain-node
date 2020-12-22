#include <boost/program_options/options_description.hpp>
#include <golos/plugins/social_network/social_network.hpp>
#include <golos/chain/index.hpp>
#include <golos/api/vote_state.hpp>
#include <golos/chain/steem_objects.hpp>

#include <golos/api/discussion_helper.hpp>
// These visitors creates additional tables, we don't really need them in LOW_MEM mode
#include <golos/plugins/tags/plugin.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/chain/operation_notification.hpp>
#include <golos/protocol/types.hpp>
#include <golos/protocol/config.hpp>
#include <golos/protocol/exceptions.hpp>

#include <diff_match_patch.h>
#include <boost/locale/encoding_utf.hpp>


#ifndef DEFAULT_VOTE_LIMIT
#  define DEFAULT_VOTE_LIMIT 10000
#endif


// Depth of comment information storage history.
struct comment_depth_params {
    comment_depth_params() {
    }

    bool miss_content() const {
        return
            has_comment_title_depth && !comment_title_depth &&
            has_comment_body_depth && !comment_body_depth &&
            has_comment_json_metadata_depth && !comment_json_metadata_depth;
    }

    bool need_clear_content() const {
        return has_comment_title_depth || has_comment_body_depth || has_comment_json_metadata_depth;
    }

    bool need_clear_last_update() const {
        return has_comment_last_update_depth;
    }

    bool should_delete_whole_content_object(const uint32_t delta) const {
        return
            (has_comment_title_depth && delta > comment_title_depth) &&
            (has_comment_body_depth && delta > comment_body_depth) &&
            (has_comment_json_metadata_depth && delta > comment_json_metadata_depth);
    }

    bool should_delete_part_of_content_object(const uint32_t delta) const {
        return
            (has_comment_title_depth && delta > comment_title_depth) ||
            (has_comment_body_depth && delta > comment_body_depth) ||
            (has_comment_json_metadata_depth && delta > comment_json_metadata_depth);
    }

    bool should_delete_last_update_object(const uint32_t delta) const {
        if (!has_comment_last_update_depth) {
            return false;
        }
        return delta > comment_last_update_depth;
    }

    uint32_t comment_title_depth;
    uint32_t comment_body_depth;
    uint32_t comment_json_metadata_depth;
    uint32_t comment_last_update_depth;

    bool has_comment_title_depth = false;
    bool has_comment_body_depth = false;
    bool has_comment_json_metadata_depth = false;
    bool has_comment_last_update_depth = false;

    bool set_null_after_update = false;
};

namespace golos { namespace plugins { namespace social_network {
    using golos::plugins::tags::fill_promoted;
    using golos::api::discussion_helper;
    using golos::plugins::social_network::comment_last_update_index;

    using boost::locale::conv::utf_to_utf;

    struct social_network::impl final {
        impl(): db(appbase::app().get_plugin<chain::plugin>().db()) {
            helper = std::make_unique<discussion_helper>(db, follow::fill_account_reputation, fill_promoted, fill_comment_info);
        }

        ~impl() = default;

        std::vector<vote_state> select_active_votes (
            const std::string& author, const std::string& permlink, uint32_t limit, uint32_t offset
        ) const ;

        std::vector<donate_api_object> select_donates (
            bool uia, const fc::variant_object& target, const std::string& from, const std::string& to, uint32_t limit, uint32_t offset, bool join_froms
        ) const ;

        std::vector<discussion> get_content_replies(
            const std::string& author, const std::string& permlink, uint32_t vote_limit, uint32_t vote_offset
        ) const;

        std::vector<discussion> get_all_content_replies(
            const std::string& author, const std::string& permlink, uint32_t vote_limit, uint32_t vote_offset
        ) const;

        std::vector<discussion> get_replies_by_last_update(
            account_name_type start_parent_author, std::string start_permlink,
            uint32_t limit, uint32_t vote_limit, uint32_t vote_offset
        ) const;

        std::vector<discussion> get_all_discussions_by_active(
            account_name_type start_author, std::string start_permlink,
            uint32_t limit, std::string category, uint32_t vote_limit, uint32_t vote_offset
        ) const;

        discussion get_content(const std::string& author, const std::string& permlink, uint32_t limit, uint32_t offset) const;

        discussion get_discussion(const comment_object& c, uint32_t vote_limit, uint32_t vote_offset) const;

        void set_depth_parameters(const comment_depth_params& params);

        // Looks for a comment_operation, fills the comment_content state objects.
        void pre_operation(const operation_notification& o);

        void post_operation(const operation_notification& o);

        void on_block(const signed_block& b);

        comment_api_object create_comment_api_object(const comment_object& o) const;

        const comment_content_object& get_comment_content(const comment_id_type& comment) const;

        const comment_content_object* find_comment_content(const comment_id_type& comment) const;

        bool set_comment_update(const comment_object& comment, time_point_sec active, bool set_last_update) const;

        void activate_parent_comments(const comment_object& comment) const;

    private:
        void select_content_replies(std::vector<discussion>& result, const std::string& author, const std::string& permlink, uint32_t limit, uint32_t offset) const;

    public:
        golos::chain::database& db;
        std::unique_ptr<discussion_helper> helper;
        comment_depth_params depth_parameters;

        // variables to temporarily store values through states of operation visitor
        asset author_gbg_payout_value{0, SBD_SYMBOL}; // part of author payout
        asset author_golos_payout_value{0, STEEM_SYMBOL}; // part of author payout
        asset author_gests_payout_value{0, VESTS_SYMBOL}; // part of author payout
        asset benef_payout_gests{0, VESTS_SYMBOL}; // GESTS version of benef payout
        asset curator_payout_gests{0, VESTS_SYMBOL}; // GESTS version of curator payout
    };

    const comment_content_object& social_network::impl::get_comment_content(const comment_id_type& comment) const {
        try {
            return db.get<comment_content_object, by_comment>(comment);
        } catch(const std::out_of_range &e) {
            GOLOS_THROW_MISSING_OBJECT("comment_content", comment);
        } FC_CAPTURE_AND_RETHROW((comment))
    }

    const comment_content_object& social_network::get_comment_content(const comment_id_type& comment) const {
        return pimpl->get_comment_content(comment);
    }

    const comment_content_object* social_network::impl::find_comment_content(const comment_id_type& comment) const {
        return db.find<comment_content_object, by_comment>(comment);
    }

    const comment_content_object* social_network::find_comment_content(const comment_id_type& comment) const {
        return pimpl->find_comment_content(comment);
    }

    discussion social_network::impl::get_discussion(const comment_object& c, uint32_t vote_limit, uint32_t vote_offset) const {
        return helper->get_discussion(c, vote_limit, vote_offset);
    }

    std::vector<vote_state> social_network::impl::select_active_votes(
        const std::string& author, const std::string& permlink, uint32_t limit, uint32_t offset
    ) const {
        return helper->select_active_votes(author, permlink, limit, offset);
    }

    std::vector<donate_api_object> social_network::impl::select_donates(
        bool uia, const fc::variant_object& target, const std::string& from, const std::string& to, uint32_t limit, uint32_t offset, bool join_froms
    ) const {
        if (limit == 0) {
            return {};
        }

        std::vector<donate_api_object> result;
        result.reserve(limit);

        auto fill_donates = [&](const auto& idx, auto& itr, auto&& verify) {
            uint32_t i = 0;
            bool need_sort = false;
            for (; itr != idx.end() && verify(itr) && result.size() < limit; ++itr, ++i) {
                if (i < offset) continue;
                if (uia) {
                    if (itr->amount.symbol == STEEM_SYMBOL) continue;
                } else {
                    if (itr->amount.symbol != STEEM_SYMBOL) continue;
                }
                if (join_froms) {
                    auto join_itr = std::find_if(result.begin(), result.end(), [&](auto& dao) {
                        return dao.from == itr->from && dao.amount.symbol == itr->amount.symbol;
                    });
                    if (join_itr != result.end()) {
                        join_itr->amount += itr->amount;
                        need_sort = true;
                        continue;
                    }
                }
                result.emplace_back(*itr, db);
            }
            if (need_sort) {
                std::sort(result.begin(), result.end(), [&](auto& lhs, auto& rhs) {
                    return lhs.get_amount() > rhs.get_amount();
                });
            }
        };

        if (target.size()) {
            const auto target_id = fc::sha256::hash(fc::json::to_string(target));
            const auto& idx = db.get_index<donate_data_index, by_target_amount>();
            auto itr = idx.lower_bound(std::make_tuple(target_id, uia));
            fill_donates(idx, itr, [&](auto& itr) {
                return itr->target_id == target_id && itr->uia == uia;
            });
        } else if (from.size()) {
            const auto& idx = db.get_index<donate_data_index, by_from_to>();
            auto itr = idx.lower_bound(std::make_tuple(from, to));
            fill_donates(idx, itr, [&](auto& itr) {
                return itr->from == from && (!to.size() || itr->to == to);
            });
        } else if (to.size()) {
            const auto& idx = db.get_index<donate_data_index, by_to_from>();
            auto itr = idx.lower_bound(std::make_tuple(to));
            fill_donates(idx, itr, [&](auto& itr) {
                return itr->to == to;
            });
        }
        return result;
    }

    bool social_network::impl::set_comment_update(const comment_object& comment, time_point_sec active, bool set_last_update) const {
        const auto& clu_idx = db.get_index<comment_last_update_index>().indices().get<golos::plugins::social_network::by_comment>();
        auto clu_itr = clu_idx.find(comment.id);
        if (clu_itr != clu_idx.end()) {
            db.modify(*clu_itr, [&](comment_last_update_object& clu) {
                clu.active = active;
                if (set_last_update) {
                    clu.last_update = clu.active;
                }
            });
            return true;
        } else {
            db.create<comment_last_update_object>([&](comment_last_update_object& clu) {
                clu.comment = comment.id;
                clu.author = comment.author;
                clu.parent_author = comment.parent_author;
                clu.active = active;
                if (set_last_update) {
                    clu.last_update = clu.active;
                }
            });
            return false;
        }
    }

    void social_network::impl::activate_parent_comments(const comment_object& comment) const {
        if (db.has_hardfork(STEEMIT_HARDFORK_0_6__80) && comment.parent_author != STEEMIT_ROOT_POST_PARENT) {
            auto parent = &db.get_comment(comment.parent_author, comment.parent_permlink);
            auto now = db.head_block_time();
            while (parent) {
                set_comment_update(*parent, now, false);
                if (parent->parent_author != STEEMIT_ROOT_POST_PARENT) {
                    parent = &db.get_comment(parent->parent_author, parent->parent_permlink);
                } else {
                    parent = nullptr;
                }
            }
        }
    }

    template <typename TImpl>
    struct delete_visitor {
        using result_type = void;

        TImpl& impl;
        golos::chain::database& db;

        delete_visitor(TImpl& impl) : impl(impl), db(impl.db) {
        }

        template<class T>
        void operator()(const T& o) const {
        }

        void operator()(const delete_comment_operation& o) const {
            const auto* comment = db.find_comment(o.author, o.permlink);
            if (comment == nullptr) {
                return;
            }

            const auto content = impl.find_comment_content(comment->id);

            if (content != nullptr) {
                impl.db.remove(*content);
            }

            if (db.has_index<comment_last_update_index>()) {
                impl.activate_parent_comments(*comment);
                auto& idx = db.get_index<comment_last_update_index, by_comment>();
                auto itr = idx.find(comment->id);
                if (idx.end() != itr) {
                    impl.db.remove(*itr);
                }
            }

            if (db.has_index<comment_reward_index>()) {
                auto& idx = db.get_index<comment_reward_index, by_comment>();
                auto itr = idx.find(comment->id);
                if (idx.end() != itr) {
                    impl.db.remove(*itr);
                }
            }
        }
    };

    template <typename TImpl>
    struct operation_visitor {
        using result_type = void;

        TImpl& impl;
        golos::chain::database& db;
        comment_depth_params& depth_parameters;

        operation_visitor(TImpl& p): impl(p), db(p.db), depth_parameters(p.depth_parameters) {
        }

        std::wstring utf8_to_wstring(const std::string& str) const {
            return utf_to_utf<wchar_t>(str.c_str(), str.c_str() + str.size());
        }

        std::string wstring_to_utf8(const std::wstring& str) const {
            return utf_to_utf<char>(str.c_str(), str.c_str() + str.size());
        }

        template<class T>
        void operator()(const T& o) const {
        } /// ignore all other ops

        void operator()(const golos::protocol::comment_operation& o) const {
            const auto* comment = db.find_comment(o.author, o.permlink);
            if (nullptr == comment) {
                return;
            }

            const auto& dp = depth_parameters;
            if (!dp.miss_content()) {
                const auto comment_content = impl.find_comment_content(comment->id);
                if ( comment_content != nullptr) {
                    // Edit case
                    db.modify(*comment_content, [&]( comment_content_object& con ) {
                        if (o.title.size() && (!dp.has_comment_title_depth || dp.comment_title_depth > 0)) {
                            from_string(con.title, o.title);
                        }
                        if (o.json_metadata.size()) {
                            if ((!dp.has_comment_json_metadata_depth || dp.comment_json_metadata_depth > 0) &&
                                fc::is_utf8(o.json_metadata)
                            ) {
                                from_string(con.json_metadata, o.json_metadata );
                            }
                        }
                        if (o.body.size() && (!dp.has_comment_body_depth || dp.comment_body_depth > 0)) {
                            try {
                                diff_match_patch<std::wstring> dmp;
                                auto patch = dmp.patch_fromText(utf8_to_wstring(o.body));
                                if (patch.size()) {
                                    auto result = dmp.patch_apply(patch, utf8_to_wstring(to_string(con.body)));
                                    auto patched_body = wstring_to_utf8(result.first);
                                    if(!fc::is_utf8(patched_body)) {
                                        from_string(con.body, fc::prune_invalid_utf8(patched_body));
                                    } else {
                                        from_string(con.body, patched_body);
                                    }
                                } else { // replace
                                    from_string(con.body, o.body);
                                }
                            } catch ( ... ) {
                                from_string(con.body, o.body);
                            }
                        }
                        // Set depth null if needed (this parameter is given in config)
                        if (dp.set_null_after_update) {
                            con.block_number = db.head_block_num();
                        }
                    });
                } else {
                    // Creation case
                    db.create<comment_content_object>([&](comment_content_object& con) {
                        con.comment = comment->id;
                        if (!dp.has_comment_title_depth || dp.comment_title_depth > 0) {
                            from_string(con.title, o.title);
                        }

                        if ((!dp.has_comment_body_depth || dp.comment_body_depth > 0) && o.body.size() < 1024*1024*128) {
                            from_string(con.body, o.body);
                        }
                        if ((!dp.has_comment_json_metadata_depth || dp.comment_json_metadata_depth > 0) &&
                            fc::is_utf8(o.json_metadata)
                        ) {
                            from_string(con.json_metadata, o.json_metadata);
                        }
                        con.block_number = db.head_block_num();
                    });
                }
            }

            if (db.has_index<comment_last_update_index>()) {
                auto now = db.head_block_time();
                if (!impl.set_comment_update(*comment, now, true)) { // If create case
                    impl.activate_parent_comments(*comment);
                }
            }
        }

        void operator()(const golos::protocol::vote_operation& op) const {
            const auto* comment = db.find_comment(op.author, op.permlink);
            if (comment == nullptr) {
                return;
            }

            const auto* comment_content = impl.find_comment_content(comment->id);
            if (comment_content != nullptr) {
                db.modify(*comment_content, [&](auto& con) {
                    con.net_rshares = comment->net_rshares;
                });
            }
        }

        result_type operator()(const author_reward_operation& op) const {
            if (!db.has_index<comment_reward_index>()) {
                return;
            }

            impl.author_gbg_payout_value += op.sbd_payout;
            impl.author_golos_payout_value += op.steem_payout;
            impl.author_gests_payout_value += op.vesting_payout;
        }

        result_type operator()(const comment_payout_update_operation& op) const {
            if (!db.has_index<comment_reward_index>()) {
                return;
            }

            const auto& comment = db.get_comment(op.author, op.permlink);

            const auto& cprops = db.get_dynamic_global_properties();
            auto vesting_sp = cprops.get_vesting_share_price();

            // Calculation author rewards value

            auto author_rewards = impl.author_golos_payout_value.amount;
            author_rewards += (impl.author_gests_payout_value * vesting_sp).amount;
            author_rewards += db.to_steem(impl.author_gbg_payout_value).amount;

            // Converting

            asset total_payout_golos = asset(author_rewards, STEEM_SYMBOL);
            asset total_payout_gbg = db.to_sbd(total_payout_golos);

            asset benef_payout_golos = impl.benef_payout_gests * vesting_sp;
            asset benef_payout_gbg = db.to_sbd(benef_payout_golos);

            asset curator_payout_golos = impl.curator_payout_gests * vesting_sp;
            asset curator_payout_gbg = db.to_sbd(curator_payout_golos);

            if (total_payout_golos.amount.value || benef_payout_golos.amount.value || curator_payout_golos.amount.value ) {
                const auto& cr_idx = db.get_index<comment_reward_index, by_comment>();
                auto cr_itr = cr_idx.find(comment.id);
                if (cr_itr == cr_idx.end()) {
                    db.create<comment_reward_object>([&](auto& cr) {
                        cr.comment = comment.id;
                        cr.author_rewards = author_rewards;
                        cr.author_gbg_payout_value = impl.author_gbg_payout_value;
                        cr.author_golos_payout_value = impl.author_golos_payout_value;
                        cr.author_gests_payout_value = impl.author_gests_payout_value;
                        cr.total_payout_value = total_payout_gbg;
                        cr.beneficiary_payout_value = benef_payout_gbg;
                        cr.beneficiary_gests_payout_value = impl.benef_payout_gests;
                        cr.curator_payout_value = curator_payout_gbg;
                        cr.curator_gests_payout_value = impl.curator_payout_gests;
                    });
                } else {
                    db.modify(*cr_itr, [&](comment_reward_object& cr) {
                        cr.author_rewards += author_rewards;
                        cr.author_gbg_payout_value += impl.author_gbg_payout_value;
                        cr.author_golos_payout_value += impl.author_golos_payout_value;
                        cr.author_gests_payout_value += impl.author_gests_payout_value;
                        cr.total_payout_value += total_payout_gbg;
                        cr.beneficiary_payout_value += benef_payout_gbg;
                        cr.beneficiary_gests_payout_value = impl.benef_payout_gests;
                        cr.curator_payout_value += curator_payout_gbg;
                        cr.curator_gests_payout_value += impl.curator_payout_gests;
                    });
                }
            }

            impl.author_gbg_payout_value.amount = 0;
            impl.benef_payout_gests.amount = 0;
            impl.curator_payout_gests.amount = 0;
            impl.author_golos_payout_value.amount = 0;
            impl.author_gests_payout_value.amount = 0;
        }

        result_type operator()(const curation_reward_operation& op) const {
            if (!db.has_index<comment_reward_index>()) {
                return;
            }

            impl.curator_payout_gests += op.reward;
        }

        result_type operator()(const comment_benefactor_reward_operation& op) const {
            if (!db.has_index<comment_reward_index>()) {
                return;
            }

            impl.benef_payout_gests += op.reward;
        }

        result_type operator()(const donate_operation& op) const {
            if (!db.has_index<donate_data_index>()) {
                return;
            }

            const auto& donate_idx = db.get_index<donate_index, by_id>();
            auto donate = --donate_idx.end();

            db.create<donate_data_object>([&](auto& don) {
                don.donate = donate->id;
                don.from = op.from;
                don.to = op.to.size() ? op.to : op.from;
                don.amount = op.amount;
                don.uia = (op.amount.symbol != STEEM_SYMBOL);
                don.target_id = fc::sha256::hash(to_string(donate->target));
                if (!!op.memo.comment) from_string(don.comment, *op.memo.comment);
            	don.time = db.head_block_time();
            });

            try {
                auto author = account_name_type(op.memo.target["author"].as_string());
                auto permlink = op.memo.target["permlink"].as_string();
                const auto* comment = db.find_comment(author, permlink);
                if (comment != nullptr) {
                    const auto content = impl.find_comment_content(comment->id);
                    if (content != nullptr) {
                        db.modify(*content, [&](auto& con) {
                            if (op.amount.symbol == STEEM_SYMBOL) {
                                con.donates += op.amount;
                            } else {
                                con.donates_uia += (op.amount.amount / op.amount.precision());
                            }
                        });
                    }
                }
            } catch (...) {}
        }
    };

    void social_network::impl::pre_operation(const operation_notification& o) { try {
        delete_visitor<social_network::impl> ovisit(*this);
        o.op.visit(ovisit);
    } FC_CAPTURE_AND_RETHROW() }

    void social_network::impl::post_operation(const operation_notification& o) { try {
        operation_visitor<social_network::impl> ovisit(*this);
        o.op.visit(ovisit);
    } FC_CAPTURE_AND_RETHROW() }


    void social_network::impl::on_block(const signed_block& b) { try {
        const auto& dp = depth_parameters;

        if (dp.need_clear_content()) {
            const auto& content_idx = db.get_index<comment_content_index>().indices().get<by_block_number>();

            auto head_block_num = db.head_block_num();
            for (auto itr = content_idx.begin(); itr != content_idx.end();) {
                auto& content = *itr;
                ++itr;

                auto* comment = db.find<comment_object, by_id>(content.comment);
                if (nullptr == comment) {
                    db.remove(content);
                    continue;
                }

                auto delta = head_block_num - content.block_number;
                if (comment->mode == archived && dp.should_delete_part_of_content_object(delta)) {
                    if (dp.should_delete_whole_content_object(delta)) {
                        db.remove(content);
                        continue;
                    }

                    db.modify(content, [&](comment_content_object& con) {
                        if (dp.has_comment_title_depth && delta > dp.comment_title_depth) {
                            con.title.clear();
                        }

                        if (dp.has_comment_body_depth && delta > dp.comment_body_depth) {
                            con.body.clear();
                        }

                        if (dp.has_comment_json_metadata_depth && delta > dp.comment_json_metadata_depth) {
                            con.json_metadata.clear();
                        }
                    });

                } else {
                    break;
                }
            }
        }

        if (dp.need_clear_last_update()) {
            const auto& clu_idx = db.get_index<comment_last_update_index>().indices().get<by_block_number>();

            auto head_block_num = db.head_block_num();
            for (auto itr = clu_idx.begin(); itr != clu_idx.end();) {
                auto& clu = *itr;
                ++itr;

                auto* comment = db.find<comment_object, by_id>(clu.comment);
                if (nullptr == comment) {
                    db.remove(clu);
                    continue;
                }

                auto delta = head_block_num - clu.block_number;
                if (comment->mode == archived && depth_parameters.should_delete_last_update_object(delta)) {
                    db.remove(clu);
                } else {
                    break;
                }
            }
        }
    } FC_CAPTURE_AND_RETHROW() }

    void social_network::plugin_startup() {
        wlog("social_network plugin: plugin_startup()");
    }

    void social_network::plugin_shutdown() {
        wlog("social_network plugin: plugin_shutdown()");
    }

    const std::string& social_network::name() {
        static const std::string name = "social_network";
        return name;
    }

    social_network::social_network() = default;

    void social_network::set_program_options(
        boost::program_options::options_description& cli,
        boost::program_options::options_description& config_file_options
    ) {
        config_file_options.add_options()
            ( // Depth of comment_content information storage history.
                "comment-title-depth", boost::program_options::value<uint32_t>(),
                "If set, remove comment titles older than specified number of blocks"
            ) (
                "comment-body-depth", boost::program_options::value<uint32_t>(),
                "If set, remove comment bodies older than specified number of blocks."
            ) (
                "comment-json-metadata-depth", boost::program_options::value<uint32_t>(),
                "If set, remove comment json-metadatas older than specified number of blocks."
            ) (
                "set-content-storing-depth-null-after-update", boost::program_options::value<bool>()->default_value(false),
                "should content's depth be set to null after update"
            ) (
                "comment-last-update-depth", boost::program_options::value<uint32_t>()->default_value(std::numeric_limits<uint32_t>::max()),
                "mode of storing records of comment.active and comment.last_update: 4294967295 = store all, 0 = do not store, N = storing N blocks depth"
            ) (
                "store-comment-rewards", boost::program_options::value<bool>()->default_value(true),
                "store comment rewards"
            );
        //  Do not use bool_switch() in cfg!
    }

    void social_network::plugin_initialize(const boost::program_options::variables_map& options) {
        pimpl = std::make_unique<impl>();
        JSON_RPC_REGISTER_API(name());

        auto& db = pimpl->db;

        add_plugin_index<comment_content_index>(db);

        comment_depth_params& params = pimpl->depth_parameters;

        auto comment_last_update_depth = options.at("comment-last-update-depth").as<uint32_t>();
        if (comment_last_update_depth != 0) {
            add_plugin_index<comment_last_update_index>(db);
            if (comment_last_update_depth != std::numeric_limits<uint32_t>::max()) {
                params.comment_last_update_depth = comment_last_update_depth;
                params.has_comment_last_update_depth = true;
            }
        }

        if (options.at("store-comment-rewards").as<bool>()) {
            add_plugin_index<comment_reward_index>(db);
        }

        add_plugin_index<donate_data_index>(db);

        db.pre_apply_operation.connect([&](const operation_notification &o) {
            pimpl->pre_operation(o);
        });

        db.post_apply_operation.connect([&](const operation_notification &o) {
            pimpl->post_operation(o);
        });

        db.applied_block.connect([&](const signed_block &b) {
            pimpl->on_block(b);
        });

        if (options.count("comment-title-depth")) {
            params.comment_title_depth = options.at("comment-title-depth").as<uint32_t>();
            params.has_comment_title_depth = true;
        }

        if (options.count("comment-body-depth")) {
            params.comment_body_depth = options.at("comment-body-depth").as<uint32_t>();
            params.has_comment_body_depth = true;
        }

        if (options.count("comment-json-metadata-depth")) {
            params.comment_json_metadata_depth = options.at("comment-json-metadata-depth").as<uint32_t>();
            params.has_comment_json_metadata_depth = true;
        }

        if (options.count("set-content-storing-depth-null-after-update")) {
            params.set_null_after_update = options.at("set-content-storing-depth-null-after-update").as<bool>();
        }
    }

    social_network::~social_network() = default;

    comment_api_object social_network::impl::create_comment_api_object(const comment_object& o) const {
        return helper->create_comment_api_object(o);
    }

    comment_api_object social_network::create_comment_api_object(const comment_object& o) const {
        return pimpl->create_comment_api_object(o);
    }

    DEFINE_API(social_network, get_content_replies) {
        PLUGIN_API_VALIDATE_ARGS(
            (string,   author)
            (string,   permlink)
            (uint32_t, vote_limit, DEFAULT_VOTE_LIMIT)
            (uint32_t, vote_offset, 0)
        );
        return pimpl->db.with_weak_read_lock([&]() {
            return pimpl->get_content_replies(author, permlink, vote_limit, vote_offset);
        });
    }

    std::vector<discussion> social_network::impl::get_content_replies(
        const std::string& author, const std::string& permlink, uint32_t vote_limit, uint32_t vote_offset
    ) const {
        std::vector<discussion> result;
        select_content_replies(result, author, permlink, vote_limit, vote_offset);
        return result;
    }

    DEFINE_API(social_network, get_all_content_replies) {
        PLUGIN_API_VALIDATE_ARGS(
            (string,   author)
            (string,   permlink)
            (uint32_t, vote_limit, DEFAULT_VOTE_LIMIT)
            (uint32_t, vote_offset, 0)
        );
        return pimpl->db.with_weak_read_lock([&]() {
            return pimpl->get_all_content_replies(author, permlink, vote_limit, vote_offset);
        });
    }

    std::vector<discussion> social_network::impl::get_all_content_replies(
        const std::string& author, const std::string& permlink, uint32_t vote_limit, uint32_t vote_offset
    ) const {
        std::vector<discussion> result;

        result.reserve(vote_limit * 16);

        select_content_replies(result, author, permlink, vote_limit, vote_offset);

        for (std::size_t i = 0; i < result.size(); ++i) {
            if (result[i].children > 0) {
                auto j = result.size();
                select_content_replies(result, result[i].author, result[i].permlink, vote_limit, vote_offset);
                for (; j < result.size(); ++j) {
                    result[i].replies.push_back(result[j].author + "/" + result[j].permlink);
                }
            }
        }
        return result;
    }

    void social_network::impl::select_content_replies(
        std::vector<discussion>& result, const std::string& author, const std::string& permlink, uint32_t limit, uint32_t offset
    ) const {
        account_name_type acc_name = account_name_type(author);
        const auto& by_permlink_idx = db.get_index<comment_index>().indices().get<by_parent>();
        auto itr = by_permlink_idx.find(std::make_tuple(acc_name, permlink));
        while (
            itr != by_permlink_idx.end() &&
            itr->parent_author == author &&
            to_string(itr->parent_permlink) == permlink
        ) {
            result.emplace_back(get_discussion(*itr, limit, offset));
            ++itr;
        }
    }

    DEFINE_API(social_network, get_account_votes) {
        PLUGIN_API_VALIDATE_ARGS(
            (account_name_type, voter)
            (uint32_t,          from,  0)
            (uint64_t,          limit, DEFAULT_VOTE_LIMIT)
        );
        auto& db = pimpl->db;
        return db.with_weak_read_lock([&]() {
            std::vector<account_vote> result;

            const auto& voter_acnt = db.get_account(voter);
            const auto& idx = db.get_index<comment_vote_index>().indices().get<by_voter_comment>();

            account_object::id_type aid(voter_acnt.id);
            auto itr = idx.lower_bound(aid);
            auto end = idx.upper_bound(aid);

            limit += from;
            for (uint32_t i = 0; itr != end && i < limit; ++itr, ++i) {
                if (i < from) {
                    continue;
                }

                const auto* vo = db.find(itr->comment);
                if (nullptr == vo) {
                    continue;
                }
                account_vote avote;
                avote.authorperm = vo->author + "/" + to_string(vo->permlink);
                //avote.weight = itr->weight; // TODO:
                avote.rshares = itr->rshares;
                avote.percent = itr->vote_percent;
                avote.time = itr->last_update;
                result.emplace_back(avote);
            }
            return result;
        });
    }

    discussion social_network::impl::get_content(const std::string& author, const std::string& permlink, uint32_t limit, uint32_t offset) const {
        const auto& by_permlink_idx = db.get_index<comment_index>().indices().get<by_permlink>();
        auto itr = by_permlink_idx.find(std::make_tuple(author, permlink));
        if (itr != by_permlink_idx.end()) {
            return get_discussion(*itr, limit, offset);
        }
        return helper->create_discussion(author);
    }

    DEFINE_API(social_network, get_content) {
        PLUGIN_API_VALIDATE_ARGS(
            (string,   author)
            (string,   permlink)
            (uint32_t, vote_limit, DEFAULT_VOTE_LIMIT)
            (uint32_t, vote_offset, 0)
        );
        return pimpl->db.with_weak_read_lock([&]() {
            return pimpl->get_content(author, permlink, vote_limit, vote_offset);
        });
    }

    DEFINE_API(social_network, get_active_votes) {
        PLUGIN_API_VALIDATE_ARGS(
            (string,   author)
            (string,   permlink)
            (uint32_t, vote_limit, DEFAULT_VOTE_LIMIT)
            (uint32_t, vote_offset, 0)
        );
        return pimpl->db.with_weak_read_lock([&]() {
            return pimpl->select_active_votes(author, permlink, vote_limit, vote_offset);
        });
    }

    DEFINE_API(social_network, get_donates) {
        PLUGIN_API_VALIDATE_ARGS(
            (bool,               uia)
            (fc::variant_object, target)
            (string,             from)
            (string,             to)
            (uint32_t,           limit, DEFAULT_VOTE_LIMIT)
            (uint32_t,           offset, 0)
            (bool,               join_froms, false)
        );
        return pimpl->db.with_weak_read_lock([&]() {
            return pimpl->select_donates(uia, target, from, to, limit, offset, join_froms);
        });
    }

    std::vector<discussion> social_network::impl::get_replies_by_last_update(
        account_name_type start_parent_author,
        std::string start_permlink,
        uint32_t limit,
        uint32_t vote_limit,
        uint32_t vote_offset
    ) const {
        std::vector<discussion> result;

        if (!db.has_index<comment_last_update_index>()) {
            return result;
        }

        // Method returns only comments which are created when config flag was true

        const auto& clu_index = db.get_index<comment_last_update_index>();
        const auto& clu_cmt_idx = clu_index.indices().get<golos::plugins::social_network::by_comment>();
        const auto& clu_idx = clu_index.indices().get<golos::plugins::social_network::by_last_update>();

        auto itr = clu_idx.begin();
        const account_name_type* parent_author = &start_parent_author;

        if (start_permlink.size()) {
            const auto& comment = db.find_comment(start_parent_author, start_permlink);
            if (nullptr == comment) {
                return result;
            }
            auto clu_itr = clu_cmt_idx.find(comment->id);
            if (clu_itr == clu_cmt_idx.end()) {
                return result;
            }
            itr = clu_idx.iterator_to(*clu_itr);
            parent_author = &comment->parent_author;
        } else if (start_parent_author.size()) {
            itr = clu_idx.lower_bound(start_parent_author);
        }

        result.reserve(limit);

        while (itr != clu_idx.end() && result.size() < limit && itr->parent_author == *parent_author) {
            auto* comment = db.find<comment_object, by_id>(itr->comment);
            if (nullptr != comment) {
                result.emplace_back(get_discussion(*comment, vote_limit, vote_offset));
            }
            ++itr;
        }

        return result;
    }

    std::vector<discussion> social_network::impl::get_all_discussions_by_active(
        account_name_type start_author,
        std::string start_permlink,
        uint32_t limit,
        std::string category,
        uint32_t vote_limit,
        uint32_t vote_offset
    ) const {
        std::vector<discussion> result;

        if (!db.has_index<comment_last_update_index>()) {
            return result;
        }

        // Method returns only comments which are created when config flag was true

        result.reserve(limit);

        std::vector<discussion> all_discussions;
        const auto& idx = db.get_index<comment_index, by_parent>();
        auto itr = idx.lower_bound(std::make_tuple(STEEMIT_ROOT_POST_PARENT, category));
        while (
            itr != idx.end() &&
            itr->parent_author == STEEMIT_ROOT_POST_PARENT
        ) {
            if (category.size() && to_string(itr->parent_permlink) != category) break;
            all_discussions.emplace_back(get_discussion(*itr, vote_limit, vote_offset));
            ++itr;
        }

        std::sort(all_discussions.begin(), all_discussions.end(), [&](auto& lhs, auto& rhs) {
            if (!!lhs.active && !!rhs.active) return *lhs.active > *rhs.active;
            return true;
        });

        int i = 0;
        if (start_author.size() && start_permlink.size()) {
            for (; i < all_discussions.size(); ++i) {
                auto& dis = all_discussions[i];
                if (dis.author == start_author && dis.permlink == start_permlink) {
                    break;
                }
            }
        }

        auto i_end = i + limit;
        for (; i < all_discussions.size() && i < i_end; ++i) {
            result.push_back(all_discussions[i]);
        }

        return result;
    }

    /**
     *  This method can be used to fetch replies to an account.
     *
     *  The first call should be (account_to_retrieve replies, "", limit)
     *  Subsequent calls should be (last_author, last_permlink, limit)
     */
    DEFINE_API(social_network, get_replies_by_last_update) {
        PLUGIN_API_VALIDATE_ARGS(
            (string,   start_parent_author)
            (string,   start_permlink)
            (uint32_t, limit)
            (uint32_t, vote_limit, DEFAULT_VOTE_LIMIT)
            (uint32_t, vote_offset, 0)

        );
        GOLOS_CHECK_LIMIT_PARAM(limit, 100);
        return pimpl->db.with_weak_read_lock([&]() {
            return pimpl->get_replies_by_last_update(start_parent_author, start_permlink, limit, vote_limit, vote_offset);
        });
    }

    /**
     *  Gets all discussions by active (last update by comment under post.
     * 
     * Unlike the tags_plugin::get_discussions_by_*** methods,
     * this method gets really ALL discussions, created from forum funding date to now.
     *
     * Supports filtering by category.
     */
    DEFINE_API(social_network, get_all_discussions_by_active) {
        PLUGIN_API_VALIDATE_ARGS(
            (string,   start_author)
            (string,   start_permlink)
            (uint32_t, limit)
            (string,   category, "")
            (uint32_t, vote_limit, DEFAULT_VOTE_LIMIT)
            (uint32_t, vote_offset, 0)

        );
        return pimpl->db.with_weak_read_lock([&]() {
            return pimpl->get_all_discussions_by_active(start_author, start_permlink, limit, category, vote_limit, vote_offset);
        });
    }

    void fill_comment_info(const golos::chain::database& db, const comment_object& co, comment_api_object& con) {
        if (db.has_index<comment_content_index>()) {
            const auto content = db.find<comment_content_object, by_comment>(co.id);
            if (content != nullptr) {
                con.title = to_string(content->title);
                con.body = to_string(content->body);
                con.json_metadata = to_string(content->json_metadata);
                con.net_rshares = content->net_rshares;
                con.donates = content->donates;
                con.donates_uia = content->donates_uia;
            }

            const auto root_content = db.find<comment_content_object, by_comment>(co.root_comment);
            if (root_content != nullptr) {
                con.root_title = to_string(root_content->title);
            }
        }

        if (db.has_index<comment_last_update_index>()) {
            const auto last_update = db.find<comment_last_update_object, by_comment>(co.id);
            if (last_update != nullptr) {
                con.active = last_update->active;
                con.last_update = last_update->last_update;
            } else {
                con.active = time_point_sec::min();
                con.last_update = time_point_sec::min();
            }
        }

        if (db.has_index<comment_reward_index>()) {
            const auto reward = db.find<comment_reward_object, by_comment>(co.id);
            if (reward != nullptr) {
                con.author_rewards = reward->author_rewards;
                con.author_gbg_payout_value = reward->author_gbg_payout_value;
                con.author_golos_payout_value = reward->author_golos_payout_value;
                con.author_gests_payout_value = reward->author_gests_payout_value;
                con.total_payout_value = reward->total_payout_value;
                con.beneficiary_payout_value = reward->beneficiary_payout_value;
                con.beneficiary_gests_payout_value = reward->beneficiary_gests_payout_value;
                con.curator_payout_value = reward->curator_payout_value;
                con.curator_gests_payout_value = reward->curator_gests_payout_value;
            } else {
                con.author_rewards = 0;
                con.author_gbg_payout_value = asset(0, SBD_SYMBOL);
                con.author_golos_payout_value = asset(0, STEEM_SYMBOL);
                con.author_gests_payout_value = asset(0, VESTS_SYMBOL);
                con.total_payout_value = asset(0, SBD_SYMBOL);
                con.beneficiary_payout_value = asset(0, SBD_SYMBOL);
                con.beneficiary_gests_payout_value = asset(0, VESTS_SYMBOL);
                con.curator_payout_value = asset(0, SBD_SYMBOL);
                con.curator_gests_payout_value = asset(0, VESTS_SYMBOL);
            }
        }
    }

    std::string get_json_metadata(const golos::chain::database& db, const comment_object& c) {
        if (!db.has_index<comment_content_index>()) {
            return std::string();
        }
        const auto content = db.find<comment_content_object, by_comment>(c.id);
        if (content != nullptr) {
            return to_string(content->json_metadata);
        }
        return std::string();
    }

} } } // golos::plugins::social_network
