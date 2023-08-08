#ifndef GOLOS_COMMENT_API_OBJ_H
#define GOLOS_COMMENT_API_OBJ_H

#include <boost/algorithm/string.hpp>
#include <golos/chain/comment_object.hpp>
#include <golos/chain/database.hpp>
#include <vector>

namespace golos { namespace api {

    using namespace golos::chain;
    using namespace golos::protocol;
    using protocol::auction_window_reward_destination_type;

    struct bad_comment {
        std::set<account_name_type> who_blocked;
        bool to_remove = false; // Not reflected. Internal
    };

    struct bad_action {
        bool remove = false;
    };

    struct content_prefs {
        std::map<account_name_type, bad_action> blockers;

        // These do not using bad_action. Just skipping post
        std::set<comment_app> filter_apps;
        std::set<comment_app> select_apps;
        bool is_good_app(const comment_app& app) const {
            if (filter_apps.find(app) != filter_apps.end()) {
                return false;
            }
            if (select_apps.size() && select_apps.find(app) == select_apps.end()) {
                return false;
            }
            return true;
        }
        bool filter_special = false;
    };

    using opt_prefs = fc::optional<content_prefs>;

    struct comment_api_object {
        comment_object::id_type id;

        std::string title;
        std::string body;
        std::string json_metadata;

        account_name_type parent_author;
        std::string parent_permlink;
        hashlink_type parent_hashlink = 0;
        account_name_type author;
        std::string permlink;
        hashlink_type hashlink = 0;

        std::string category;

        fc::optional<time_point_sec> last_update;
        time_point_sec created;
        fc::optional<time_point_sec> active;
        fc::optional<uint16_t> num_changes;
        time_point_sec last_payout;
        comment_object::id_type last_reply_id; // not reflected

        uint8_t depth = 0;
        uint32_t children = 0;

        uint128_t children_rshares2 = 0;

        share_type net_rshares;
        share_type abs_rshares;
        share_type vote_rshares;

        share_type children_abs_rshares;
        time_point_sec cashout_time;
        time_point_sec max_cashout_time;
        uint64_t total_vote_weight = 0;

        uint16_t reward_weight = 0;

        asset donates = asset(0, STEEM_SYMBOL);
        share_type donates_uia = 0;
        asset total_payout_value = asset(0, SBD_SYMBOL);
        asset beneficiary_payout_value = asset(0, SBD_SYMBOL);
        asset beneficiary_gests_payout_value = asset(0, VESTS_SYMBOL);
        asset curator_payout_value = asset(0, SBD_SYMBOL);
        asset curator_gests_payout_value = asset(0, VESTS_SYMBOL);

        share_type author_rewards;
        asset author_payout_in_golos = asset(0, STEEM_SYMBOL);
        asset author_gbg_payout_value = asset(0, SBD_SYMBOL);
        asset author_golos_payout_value = asset(0, STEEM_SYMBOL);
        asset author_gests_payout_value = asset(0, VESTS_SYMBOL);

        int32_t net_votes = 0;

        comment_mode mode = not_set;

        comment_object::id_type root_comment;
        account_name_type root_author;
        std::string root_permlink;     //Not reflected - used only to construct url

        comment_app app;

        protocol::curation_curve curation_reward_curve = protocol::curation_curve::detect;
        fc::optional<auction_window_reward_destination_type> auction_window_reward_destination;
        fc::optional<uint16_t> auction_window_size;
        uint64_t auction_window_weight = 0;
        uint64_t votes_in_auction_window_weight = 0;
        
        string root_title;

        fc::optional<protocol::asset> max_accepted_payout;
        fc::optional<uint16_t> percent_steem_dollars;
        bool allow_replies = true;
        bool allow_votes = 0;
        fc::optional<bool> allow_curation_rewards;
        fc::optional<uint16_t> curation_rewards_percent;
        fc::optional<asset> min_golos_power_to_curate;

        bool has_worker_request = false;

        fc::optional<vector<protocol::beneficiary_route_type>> beneficiaries;
    
        fc::optional<bad_comment> bad;

        bool is_special() const {
            return boost::algorithm::starts_with(body, "{\"t\":");
        }
    };

} } // golos::api

FC_REFLECT((golos::api::bad_comment),
    (who_blocked)
)
FC_REFLECT((golos::api::bad_action),
    (remove)
)
FC_REFLECT((golos::api::content_prefs),
    (blockers)(filter_apps)(select_apps)(filter_special)
)

FC_REFLECT(
    (golos::api::comment_api_object),
    (id)(author)(permlink)(hashlink)(parent_author)(parent_permlink)(parent_hashlink)(category)(title)(body)(json_metadata)(last_update)
    (created)(active)(num_changes)(last_payout)(depth)(children)(children_rshares2)(net_rshares)(abs_rshares)
    (vote_rshares)(children_abs_rshares)(cashout_time)(max_cashout_time)(total_vote_weight)
    (reward_weight)(donates)(donates_uia)(total_payout_value)(beneficiary_payout_value)(beneficiary_gests_payout_value)(curator_payout_value)(curator_gests_payout_value)
    (author_rewards)(author_payout_in_golos)(author_gbg_payout_value)(author_golos_payout_value)(author_gests_payout_value)(net_votes)
    (mode)(curation_reward_curve)(auction_window_reward_destination)
    (auction_window_size)(auction_window_weight)(votes_in_auction_window_weight)
    (root_comment)(root_title)(root_author)(app)(max_accepted_payout)(percent_steem_dollars)(allow_replies)(allow_votes)
    (allow_curation_rewards)(curation_rewards_percent)(min_golos_power_to_curate)
    (has_worker_request)
    (beneficiaries)(bad))

#endif //GOLOS_COMMENT_API_OBJ_H
