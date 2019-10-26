#pragma once

#include <golos/protocol/exceptions.hpp>
#include <golos/protocol/operations.hpp>
#include <golos/chain/account_object.hpp>

#define ASSERT_REQ_HF_IN_DB(HF, FEATURE, DATABASE) \
    GOLOS_ASSERT(DATABASE.has_hardfork(HF), unsupported_operation, \
        "${feature} is not enabled until HF ${hardfork}", \
        ("feature",FEATURE)("hardfork",BOOST_PP_STRINGIZE(HF)));

#define ASSERT_REQ_HF(HF, FEATURE) \
    ASSERT_REQ_HF_IN_DB(HF, FEATURE, _db)

#define GOLOS_CHECK_BALANCE(ACCOUNT, TYPE, REQUIRED ...) \
    FC_EXPAND_MACRO( \
        FC_MULTILINE_MACRO_BEGIN \
            asset exist = get_balance(ACCOUNT, TYPE, (REQUIRED).symbol); \
            if( UNLIKELY( exist < (REQUIRED) )) { \
                FC_THROW_EXCEPTION( golos::insufficient_funds, \
                        "Account \"${account}\" does not have enough ${balance}: required ${required}, exist ${exist}", \
                        ("account",ACCOUNT.name)("balance",get_balance_name(TYPE))("required",REQUIRED)("exist",exist)); \
            } \
        FC_MULTILINE_MACRO_END \
    )

namespace golos {
    namespace chain {

        class database;

        template<typename OperationType=golos::protocol::operation>
        class evaluator {
        public:
            virtual void apply(const OperationType &op) = 0;

            virtual int get_type() const = 0;
        };

        template<typename EvaluatorType, typename OperationType=golos::protocol::operation>
        class evaluator_impl : public evaluator<OperationType> {
        public:
            typedef OperationType operation_sv_type;
            // typedef typename EvaluatorType::operation_type op_type;

            evaluator_impl(database &d)
                    : _db(d) {
            }

            virtual void apply(const OperationType &o) final override {
                auto *eval = static_cast< EvaluatorType * >(this);
                const auto &op = o.template get<typename EvaluatorType::operation_type>();
                eval->do_apply(op);
            }

            virtual int get_type() const override {
                return OperationType::template tag<typename EvaluatorType::operation_type>::value;
            }

            database &db() {
                return _db;
            }

        protected:
            database &_db;
        };

        enum balance_type {
            MAIN_BALANCE,
            SAVINGS,
            VESTING,
            EFFECTIVE_VESTING,
            HAVING_VESTING,
            AVAILABLE_VESTING
        };

        asset get_balance(const account_object &account, balance_type type, asset_symbol_type symbol);

        std::string get_balance_name(balance_type type);
    }
}

#define DEFINE_EVALUATOR(X) \
class X ## _evaluator : public golos::chain::evaluator_impl< X ## _evaluator > \
{                                                                           \
   public:                                                                  \
      typedef X ## _operation operation_type;                               \
                                                                            \
      X ## _evaluator( database& db )                                       \
         : golos::chain::evaluator_impl< X ## _evaluator >( db )          \
      {}                                                                    \
                                                                            \
      void do_apply( const X ## _operation& o );                            \
};
