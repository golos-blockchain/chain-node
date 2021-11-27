#pragma once

#include <golos/chain/witness_objects.hpp>

namespace golos { namespace plugins { namespace witness_api {

            using namespace golos::chain;
            using namespace golos::protocol;

            struct witness_vote_api_object {
                witness_vote_api_object(const witness_vote_object& v, const database& _db) :
                        account(_db.get(v.account).name),
                        account_id(v.account),
                        rshares(v.rshares) {
                }

                witness_vote_api_object() {
                }

                account_name_type account;
                account_id_type account_id;
                share_type rshares;
            };
        }
    }
}

FC_REFLECT((golos::plugins::witness_api::witness_vote_api_object),
    (account)(account_id)(rshares))
