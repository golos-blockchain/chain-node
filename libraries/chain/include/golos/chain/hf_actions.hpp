#include <golos/chain/database.hpp>

namespace golos { namespace chain {

class hf_actions {
public:
    hf_actions(database& db);

    void prepare_for_tests();
    void create_worker_pool();
private:
    template<typename FillAuth, typename FillAccount>
    void create_test_account(account_name_type acc, FillAuth&& fillAuth, FillAccount&& fillAcc);

    template<typename FillAuth>
    void create_test_account(account_name_type acc, FillAuth&& fillAuth);

    database& _db;
};

}} // golos::chain
