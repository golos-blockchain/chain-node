#include <golos/chain/database.hpp>
#include <golos/chain/comment_bill.hpp>

namespace golos { namespace chain {

void database::set_clear_comment_bills(bool clear_comment_bills) {
    _clear_comment_bills = clear_comment_bills;
}

const comment_bill_object* database::find_comment_bill(const comment_id_type& id) const {
    return find<comment_bill_object, by_comment>(id);
}

comment_bill database::get_comment_bill(const comment_id_type& id) const {
    const auto* obj = find_comment_bill(id);
    if (obj) {
        return obj->bill;
    } else {
        return comment_bill();
    }
}

const comment_bill_object* database::upsert_comment_bill(const comment_id_type& id) {
    const auto* obj = find_comment_bill(id);
    if (obj) {
        return obj;
    }
    return &create<comment_bill_object>([&](auto& obj) {
        obj.comment = id;
    });
}

}}
