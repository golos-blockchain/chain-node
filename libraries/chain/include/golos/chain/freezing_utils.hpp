#include <golos/chain/database.hpp>

namespace golos { namespace chain {

class freezing_utils {
public:
    freezing_utils(database& db);

    bool is_inactive(const account_object& acc);

    bool is_system_account(account_name_type name);

    void unfreeze(const account_object& acc, const asset& fee);

    database& _db;
    uint32_t hardfork;
    bool hf_long_ago = false;
    bool hf_ago_ended_now = false;
};

}} // golos::chain
