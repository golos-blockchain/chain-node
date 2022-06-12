#include <golos/chain/database.hpp>

namespace golos { namespace chain {

class freezing_utils {
public:
    freezing_utils(database& db);

    bool is_inactive(const account_object& acc);

    bool is_system_account(account_name_type name);

    void unfreeze(const account_object& acc);

    database& _db;
    uint32_t hardfork;
    bool hf_long_ago = false;
};

}} // golos::chain
