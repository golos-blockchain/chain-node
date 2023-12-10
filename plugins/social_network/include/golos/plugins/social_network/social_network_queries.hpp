#pragma once

#include <golos/chain/steem_objects.hpp>

using namespace golos::protocol;

namespace golos { namespace plugins { namespace social_network {

enum class referrals_sort : uint8_t {
    by_joined,
    by_rewards
};

struct referrals_query {
    account_name_type referrer;

    std::string start_name;
    uint32_t limit = 20;

    referrals_sort sort = referrals_sort::by_joined;
};

enum class referrers_sort : uint8_t {
    by_referral_count,
    by_referral_vesting,
    by_referral_post_count,
    by_referral_comment_count,
    by_referral_total_postings
};

struct referrers_query {
    std::string start_name;
    uint32_t limit = 20;

    referrers_sort sort = referrers_sort::by_referral_count;
};

}}}

FC_REFLECT_ENUM(
    golos::plugins::social_network::referrals_sort,
    (by_joined)(by_rewards)
)

FC_REFLECT(
    (golos::plugins::social_network::referrals_query),
    (referrer)(start_name)(limit)(sort)
)

FC_REFLECT_ENUM(
    golos::plugins::social_network::referrers_sort,
    (by_referral_count)(by_referral_vesting)
    (by_referral_post_count)(by_referral_comment_count)(by_referral_total_postings)
)

FC_REFLECT(
    (golos::plugins::social_network::referrers_query),
    (start_name)(limit)(sort)
)
