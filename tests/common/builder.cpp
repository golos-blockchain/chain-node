
#include "builder.hpp"

#include "database_fixture.hpp"

namespace golos { namespace chain {

void builder_op::push(const private_key_type& private_key) {
    signed_transaction tx;
    operation oop = to_operation();
    GOLOS_CHECK_NO_THROW(fixture()->push_tx_with_ops(tx, private_key, oop));
}

} }
