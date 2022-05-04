#pragma once

#include <golos/api/vote_state.hpp>
#include <golos/api/comment_api_object.hpp>
#include <golos/api/reblog_entry.hpp>


namespace golos { namespace api {

    struct discussion : public comment_api_object {
        discussion(const comment_api_object& o)
                : comment_api_object(o) {
            
        }

        discussion() = default;

        string url; /// /category/@rootauthor/root_permlink#author/permlink

        asset pending_author_payout_value = asset(0, SBD_SYMBOL);
        asset pending_author_payout_in_golos = asset(0, STEEM_SYMBOL);
        asset pending_author_payout_gbg_value = asset(0, SBD_SYMBOL);
        asset pending_author_payout_gests_value = asset(0, VESTS_SYMBOL);
        asset pending_author_payout_golos_value = asset(0, STEEM_SYMBOL);
        asset pending_benefactor_payout_value = asset(0, SBD_SYMBOL);
        asset pending_benefactor_payout_gests_value = asset(0, VESTS_SYMBOL);
        asset pending_curator_payout_value = asset(0, SBD_SYMBOL);
        asset pending_curator_payout_gests_value = asset(0, VESTS_SYMBOL);
        asset pending_payout_value = asset(0, SBD_SYMBOL); ///< sbd
        asset total_pending_payout_value = asset(0, SBD_SYMBOL); ///< sbd including replies

        std::vector<vote_state> active_votes;
        uint32_t active_votes_count = 0;
        std::vector<string> replies; ///< author/slug mapping
        fc::optional<share_type> author_reputation;
        fc::optional<asset> promoted;
        double hot = 0;
        double trending = 0;
        uint32_t body_length = 0;

        std::vector<account_name_type> reblogged_by;
        optional<account_name_type> first_reblogged_by;
        optional<time_point_sec> first_reblogged_on;
        optional<account_name_type> reblog_author;
        optional<std::string> reblog_title;
        optional<std::string> reblog_body;
        optional<std::string> reblog_json_metadata;
        std::vector<reblog_entry> reblog_entries;

        fc::optional<comment_api_object> last_reply;
    };

} } // golos::api

FC_REFLECT_DERIVED( (golos::api::discussion), ((golos::api::comment_api_object)),
    (url)(pending_author_payout_value)(pending_author_payout_in_golos)
    (pending_author_payout_gbg_value)(pending_author_payout_gests_value)(pending_author_payout_golos_value)
    (pending_benefactor_payout_value)(pending_benefactor_payout_gests_value)
    (pending_curator_payout_value)(pending_curator_payout_gests_value)
    (pending_payout_value)(total_pending_payout_value)(active_votes)(active_votes_count)(replies)
    (author_reputation)(promoted)(body_length)(reblogged_by)(first_reblogged_by)(first_reblogged_on)
    (reblog_author)(reblog_title)(reblog_body)(reblog_json_metadata)(reblog_entries)(last_reply)
)
