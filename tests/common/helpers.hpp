#pragma once

// Common code sequences for testing and boost container extensions


// Validate
//-------------------------------------------------------------

/// validate operation
#define CHECK_OP_VALID(OP) BOOST_CHECK_NO_THROW(OP.validate())

/// change operation parameter value and check it's still valid
#define CHECK_PARAM_VALID(OP, NAME, VAL) \
    CHECK_PARAM_VALID_I(OP, NAME, VAL, CHECK_OP_VALID(OP))

/// change operation parameter value and check it's invalid (+restore param)
#define CHECK_PARAM_INVALID(OP, N, V) \
    CHECK_PARAM_VALIDATION_FAIL(OP, N, V, CHECK_ERROR(invalid_parameter, #N))

/// change operation parameter value and check it fails with logic_exceprion (+restore param)
#define CHECK_PARAM_INVALID_LOGIC(OP, N, V, EX) \
    CHECK_PARAM_VALIDATION_FAIL(OP, N, V, CHECK_ERROR(logic_exception, logic_exception:: EX))

/// same as previous but with configurable CHECK_ERROR parameters
#define CHECK_PARAM_VALIDATION_FAIL(OP, N, V, VALIDATOR) \
    CHECK_PARAM_VALID_I(OP, N, V, \
        GOLOS_CHECK_ERROR_PROPS(OP.validate(), VALIDATOR))


// Check operation authorities
#define CHECK_OP_AUTHS(OP, OWNER, ACTIVE, POSTING) {               \
    golos::chain::account_name_set                                 \
            owner_auths, active_auths, posting_auths;              \
    OP.get_required_owner_authorities(owner_auths);                \
    BOOST_CHECK_EQUAL(owner_auths, OWNER);                         \
    OP.get_required_active_authorities(active_auths);              \
    BOOST_CHECK_EQUAL(active_auths, ACTIVE);                       \
    OP.get_required_posting_authorities(posting_auths);            \
    BOOST_CHECK_EQUAL(posting_auths, POSTING);                     \
}

// Check if 2 numeric values are approximately equal
#define APPROX_CHECK_EQUAL(X, Y, DELTA) {                          \
    BOOST_CHECK_LE(std::abs(X - Y), DELTA);                         \
}

// Check if 2 doubles are approximately equal
#define APPROX_CHECK_DOUBLE_EQUAL(X, Y, DELTA) {                   \
    BOOST_CHECK_LT(std::fabs(X - Y), DELTA);                         \
}

// internals
//-------------------------------------------------------------

// validate
#define CHECK_PARAM_VALID_I(OP, NAME, VAL, CHECK) { \
    auto t = OP.NAME; \
    OP.NAME = VAL; \
    CHECK; \
    OP.NAME = t; \
}
