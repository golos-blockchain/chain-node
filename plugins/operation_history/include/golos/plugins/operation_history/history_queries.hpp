#pragma once

#include <golos/chain/database.hpp>
#include <appbase/plugin.hpp>

using namespace golos::protocol;

namespace golos { namespace plugins { namespace operation_history {

struct nft_token_ops_query {
    std::set<uint32_t> token_ids;

    uint32_t from = 0;
    uint32_t limit = 20;

    bool reverse_sort = false;
};

}}}

FC_REFLECT(
    (golos::plugins::operation_history::nft_token_ops_query),
    (token_ids)(from)(limit)(reverse_sort)
)
