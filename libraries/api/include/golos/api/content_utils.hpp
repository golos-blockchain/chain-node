#pragma once

#include <golos/api/comment_api_object.hpp>
#include <golos/chain/database.hpp>

namespace golos { namespace api {

    fc::optional<bad_comment> process_content_prefs(const opt_prefs& prefs, const account_name_type& author, const database& _db);

}}
