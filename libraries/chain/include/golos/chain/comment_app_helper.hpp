#pragma once

#include <fc/io/json.hpp>
#include <golos/chain/comment_object.hpp>
#include <golos/chain/database.hpp>
#include <boost/algorithm/string.hpp>

namespace golos { namespace chain {

comment_app parse_comment_app(const database& _db, const comment_operation& op);

const comment_app_id* find_comment_app_id(const database& _db, const comment_app& app);

comment_app get_comment_app_by_id(const database& _db, const comment_app_id& id);

comment_app_id singleton_comment_app(database& _db, const comment_app& app);

}}
