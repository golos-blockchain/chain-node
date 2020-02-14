#pragma once

#include <fc/optional.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/chain/worker_objects.hpp>

using namespace golos::chain;

namespace golos { namespace plugins { namespace worker_api {

    struct worker_request_query {
        uint32_t                        limit = 20;
        fc::optional<std::string>       start_author;
        fc::optional<std::string>       start_permlink;
        std::set<std::string>           select_authors;
        std::set<worker_request_state> select_states;

        void validate() const;

        bool has_start() const {
            return !!start_author;
        }

        bool has_author() const {
            return !select_authors.empty();
        }

        bool is_good_author(const std::string& author) const {
            return !has_author() || select_authors.count(author);
        }

        bool is_good_state(const worker_request_state& state) const {
            return select_states.empty() || select_states.count(state);
        }
    };

} } } // golos::plugins::worker_api

FC_REFLECT((golos::plugins::worker_api::worker_request_query),
    (limit)(start_author)(start_permlink)(select_authors)(select_states)
);
