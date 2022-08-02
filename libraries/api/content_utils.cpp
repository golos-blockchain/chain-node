#include <golos/api/content_utils.hpp>

namespace golos { namespace api {

fc::optional<bad_comment> process_content_prefs(const opt_prefs& prefs, const account_name_type& author, const database& _db) {
	bad_comment res;
    if (!!prefs) {
        for (const auto& pair : prefs->blockers) {
            bool blocking = _db.is_blocking(pair.first, author);
            if (blocking) {
                res.who_blocked.insert(pair.first);
                if (pair.second.remove) {
                    res.to_remove = true;
                }
            }
        }
    }
    if (!res.who_blocked.size()) {
    	return fc::optional<bad_comment>();
    }
    return res;
}

}}
